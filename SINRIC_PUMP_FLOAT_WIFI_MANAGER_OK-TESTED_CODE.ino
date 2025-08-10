/******** Sinric Pro Momentary Relay Control + Auto Mode + WiFiManager + 30s Delay + Manual Button (5s Hold) ********/

#include <WiFiManager.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <Preferences.h>

// Sinric Pro credentials
#define APP_KEY       "c7303938-59fa-4ad2-a3de-e62d9bae9cec"
#define APP_SECRET    "c41c8855-b93b-496c-bf27-43f0f72c2dc8-4e754444-82cd-4acb-bb2c-c9cfe22b9b13"
#define DEVICE_ID_1   "6889cacbedeca866fe96e2ee"
#define DEVICE_ID_2   "6889caffddd2551252ba8a70"
#define DEVICE_ID_3   "6890c55fedeca866fe994705"

// Relay pins (active LOW)
#define RELAY1_PIN    19
#define RELAY2_PIN    21
#define AUTO_MODE_LED 22

// Manual push button for Relay2
#define MANUAL_BTN_PIN 27  // Active LOW

// WiFi indicator LED
#define WIFI_LED_PIN  2

#define PULSE_TIME    3000   // milliseconds
#define AUTO_DELAY    30000  // 30 seconds
#define HOLD_TIME     5000   // 5 seconds hold

unsigned long bootTime = 0;
Preferences preferences;
bool autoMode = false;

// Manual button hold variables
unsigned long btnPressStart = 0;
bool btnPressed = false;

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
  } else if (deviceId == DEVICE_ID_3) {
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
  SinricProSwitch &sw3 = SinricPro[DEVICE_ID_3];

  sw1.onPowerState(onPowerState);
  sw2.onPowerState(onPowerState);
  sw3.onPowerState(onPowerState);

  SinricPro.restoreDeviceStates(false);
  SinricPro.begin(APP_KEY, APP_SECRET);
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

  pinMode(MANUAL_BTN_PIN, INPUT_PULLUP); // Manual button

  // WiFiManager
  WiFiManager wm;
  bool res = wm.autoConnect("Pump-Setup", "12345678");
  if (!res) {
    Serial.println("Failed to connect.");
    ESP.restart();
  }
  Serial.println("WiFi connected!");
  WiFi.setAutoReconnect(true);
  digitalWrite(WIFI_LED_PIN, HIGH);

  restoreAutoMode();
  setupSinricPro();
}

// ========== Loop ==========
void loop() {
  // Manual button 5-second hold detection
  if (digitalRead(MANUAL_BTN_PIN) == LOW) {
    if (!btnPressed) {
      btnPressed = true;
      btnPressStart = millis();
    } else if (millis() - btnPressStart >= HOLD_TIME) {
      Serial.println("Manual button held 5s: Trigger Relay2");
      pulseRelay(RELAY2_PIN);
      btnPressed = false;
      while (digitalRead(MANUAL_BTN_PIN) == LOW) delay(10); // Wait release
    }
  } else {
    btnPressed = false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(WIFI_LED_PIN, HIGH);
    SinricPro.handle();
  } else {
    digitalWrite(WIFI_LED_PIN, LOW);
    static unsigned long lastAttemptTime = 0;
    if (millis() - lastAttemptTime > 10000) {
      Serial.println("WiFi lost. Attempting reconnect...");
      WiFi.begin();
      lastAttemptTime = millis();
    }
  }
}