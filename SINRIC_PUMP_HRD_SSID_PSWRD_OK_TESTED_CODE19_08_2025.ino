/******** Sinric Pro Momentary Relay Control + Auto Mode + Hardcoded WiFi + Auto-Reconnect ********/

#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <Preferences.h>

// WiFi credentials - Please change these to your network's SSID and password
const char* ssid = "ElectroConnect";
const char* password = "vikash9296";

// Sinric Pro credentials
#define APP_KEY       "c7303938-59fa-4ad2-a3de-e62d9bae9cec"
#define APP_SECRET    "c41c8855-b93b-496c-bf27-43f0f72c2dc8-4e754444-82cd-4acb-bb2c-c9cfe22b9b13"
#define DEVICE_ID_1   "6889cacbedeca866fe96e2ee"
#define DEVICE_ID_2   "6889caffddd2551252ba8a70"
#define DEVICE_3_ID   "6890c55fedeca866fe994705"

// Relay pins (active LOW)
#define RELAY1_PIN    19
#define RELAY2_PIN    21
#define AUTO_MODE_LED 22

// WiFi indicator LED
#define WIFI_LED_PIN  2

// Manual push button
#define MANUAL_BTN_PIN 27
#define HOLD_TIME     5000  // 5 seconds hold

#define PULSE_TIME    3000  // milliseconds
#define AUTO_DELAY    30000 // 30 seconds

unsigned long bootTime = 0;
Preferences preferences;
bool autoMode = false;

// Manual button state
bool btnPressed = false;
unsigned long btnPressStart = 0;

// Reconnection tracking
unsigned long lastReconnectAttempt = 0;
unsigned long reconnectInterval = 5000; // Attempt to reconnect every 5 seconds

// ========== Relay Setup and Control ==========
void setupRelay(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH); // OFF for active-low relay
}

void pulseRelay(int pin, int time = PULSE_TIME) {
  digitalWrite(pin, LOW);
  delay(time);
  digitalWrite(pin, HIGH);
}

// ========== EEPROM Restore ==========
void restoreAutoMode() {
  preferences.begin("settings", false);
  autoMode = preferences.getBool("autoMode", false);
  preferences.end();

  digitalWrite(AUTO_MODE_LED, autoMode ? HIGH : LOW);

  if (autoMode) {
    Serial.println("Auto Mode ON: Waiting 30s to trigger Relay1 on boot");
    // Delay is blocking, but ensures the pulse happens before other operations
    delay(AUTO_DELAY);  
    pulseRelay(RELAY1_PIN);
  }
}

// ========== Sinric Pro Handler ==========
bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId == DEVICE_ID_1) {
    pulseRelay(RELAY1_PIN);
  } else if (deviceId == DEVICE_ID_2) {
    pulseRelay(RELAY2_PIN);
  } else if (deviceId == DEVICE_3_ID) {
    autoMode = state;
    preferences.begin("settings", false);
    preferences.putBool("autoMode", autoMode);
    preferences.end();
    digitalWrite(AUTO_MODE_LED, autoMode ? HIGH : LOW);
    Serial.printf("Auto Mode %s\n", autoMode ? "Enabled" : "Disabled");
  }
  return true;
}

void setupSinricPro() {
  SinricProSwitch &sw1 = SinricPro[DEVICE_ID_1];
  SinricProSwitch &sw2 = SinricPro[DEVICE_ID_2];
  SinricProSwitch &sw3 = SinricPro[DEVICE_3_ID];

  sw1.onPowerState(onPowerState);
  sw2.onPowerState(onPowerState);
  sw3.onPowerState(onPowerState);

  SinricPro.restoreDeviceStates(false);
  SinricPro.begin(APP_KEY, APP_SECRET);
}

// Function to handle WiFi connection and reconnection
void connectToWiFi() {
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);

  // Use a non-blocking loop for faster reconnection attempts
  // It will keep trying to connect without blocking the entire code
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED_PIN, HIGH);
    lastReconnectAttempt = 0; // Reset the timer on successful connection
  } else {
    Serial.println("\nFailed to connect. Will continue with main loop...");
    digitalWrite(WIFI_LED_PIN, LOW);
    // Removed ESP.restart() to allow the code to run even without WiFi
  }
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  bootTime = millis();

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);

  setupRelay(RELAY1_PIN);
  setupRelay(RELAY2_PIN);
  pinMode(AUTO_MODE_LED, OUTPUT);
  digitalWrite(AUTO_MODE_LED, LOW);

  // Manual button setup
  pinMode(MANUAL_BTN_PIN, INPUT_PULLUP);
  
  // FIX: Moved this function to the beginning of setup() so it runs regardless of WiFi status.
  restoreAutoMode(); 

  // Connect to WiFi after checking auto mode
  connectToWiFi();
  
  // Set auto-reconnect flag
  WiFi.setAutoReconnect(true);

  setupSinricPro();
}

// ========== Loop ==========
void loop() {
  // Check WiFi status and try to reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(WIFI_LED_PIN, LOW);
    // Check if enough time has passed since the last attempt
    if (millis() - lastReconnectAttempt > reconnectInterval) {
      Serial.println("WiFi lost. Attempting reconnect...");
      WiFi.begin(ssid, password);
      lastReconnectAttempt = millis();
    }
  } else {
    digitalWrite(WIFI_LED_PIN, HIGH);
  }

  // Check if SinricPro connection is alive
  if (!SinricPro.isConnected()) {
    // Attempt to reconnect to SinricPro if not connected
    if (millis() - lastReconnectAttempt > reconnectInterval) {
      Serial.println("SinricPro disconnected. Attempting reconnect...");
      SinricPro.begin(APP_KEY, APP_SECRET);
      lastReconnectAttempt = millis();
    }
  }

  // Handle SinricPro events
  SinricPro.handle();

  // Manual button 5-second hold detection
  if (digitalRead(MANUAL_BTN_PIN) == LOW) {
    if (!btnPressed) {
      btnPressed = true;
      btnPressStart = millis();
    } else if (millis() - btnPressStart >= HOLD_TIME) {
      Serial.println("Manual button held 5s: Trigger Relay2");

      // Trigger Relay2
      pulseRelay(RELAY2_PIN);

      // Notify Sinric Pro for app update/notification
      // Check if WiFi is connected before sending the event
      if (WiFi.status() == WL_CONNECTED) {
        SinricProSwitch &sw2 = SinricPro[DEVICE_ID_2];
        sw2.sendPowerStateEvent(true);
        delay(500);
        sw2.sendPowerStateEvent(false);
      }
      
      btnPressed = false;
      while (digitalRead(MANUAL_BTN_PIN) == LOW) delay(10); // Wait release
    }
  } else {
    btnPressed = false;
  }
}