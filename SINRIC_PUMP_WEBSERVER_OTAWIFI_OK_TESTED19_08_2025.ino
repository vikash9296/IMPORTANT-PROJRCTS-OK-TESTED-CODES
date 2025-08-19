#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>

// ------------------- Sinric Pro Credentials -------------------
#define APP_KEY       "c7303938-59fa-4ad2-a3de-e62d9bae9cec"
#define APP_SECRET    "c41c8855-b93b-496c-bf27-43f0f72c2dc8-4e754444-82cd-4acb-bb2c-c9cfe22b9b13"
#define DEVICE_ID_1   "6889cacbedeca866fe96e2ee"
#define DEVICE_ID_2   "6889caffddd2551252ba8a70"
#define DEVICE_3_ID   "6890c55fedeca866fe994705"

// ------------------- Relay pins -------------------
#define RELAY1_PIN    19
#define RELAY2_PIN    21
#define AUTO_MODE_LED 22

// ------------------- Wi-Fi LED -------------------
#define WIFI_LED_PIN  2

// ------------------- Manual button -------------------
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

// Wi-Fi variables
String ssid;
String password;

// Web server for Wi-Fi portal
WebServer server(80);

// ================= Relay Control =================
void setupRelay(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH); // OFF for active-low relay
}

void pulseRelay(int pin, int time = PULSE_TIME) {
  digitalWrite(pin, LOW);
  delay(time);
  digitalWrite(pin, HIGH);
}

// ================= Preferences =================
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

// ================= Sinric Pro =================
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

// ================= Wi-Fi =================
void handleRoot() {
  String html = "<h1>Pump Controller Wi-Fi Setup</h1>";
  html += "<form action='/save' method='POST'>";
  html += "SSID: <input name='ssid'><br>";
  html += "Password: <input name='pass'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    ssid = server.arg("ssid");
    password = server.arg("pass");
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    server.send(200, "text/html", "<h2>Saved! Device will restart...</h2>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "Missing SSID or Password!");
  }
}

void startAPMode() {
  WiFi.softAP("PumpController_AP");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
}

// Connect using saved Wi-Fi
void connectToWiFi() {
  preferences.begin("wifi", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();

  if (ssid != "" && password != "") {
    Serial.printf("Connecting to %s", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED_PIN, HIGH);
    lastReconnectAttempt = 0;
  } else {
    Serial.println("\nFailed to connect. Starting AP mode...");
    digitalWrite(WIFI_LED_PIN, LOW);
    startAPMode();
  }
}

// ================= OTA =================
void setupOTA() {
  ArduinoOTA.setHostname("PumpController_OTA");
  ArduinoOTA.begin();
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  bootTime = millis();

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);

  setupRelay(RELAY1_PIN);
  setupRelay(RELAY2_PIN);
  pinMode(AUTO_MODE_LED, OUTPUT);
  digitalWrite(AUTO_MODE_LED, LOW);

  pinMode(MANUAL_BTN_PIN, INPUT_PULLUP);

  restoreAutoMode(); // Auto Mode restore

  connectToWiFi();   // Wi-Fi connect

  WiFi.setAutoReconnect(true);
  setupOTA();        // OTA enable

  setupSinricPro();  // Sinric Pro setup
}

// ================= Loop =================
void loop() {
  ArduinoOTA.handle();

  // Wi-Fi reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(WIFI_LED_PIN, LOW);
    if (millis() - lastReconnectAttempt > reconnectInterval) {
      Serial.println("WiFi lost. Attempting reconnect...");
      WiFi.begin(ssid.c_str(), password.c_str());
      lastReconnectAttempt = millis();
    }
  } else {
    digitalWrite(WIFI_LED_PIN, HIGH);
  }

  // SinricPro reconnect
  if (!SinricPro.isConnected() && millis() - lastReconnectAttempt > reconnectInterval) {
    Serial.println("SinricPro disconnected. Attempting reconnect...");
    SinricPro.begin(APP_KEY, APP_SECRET);
    lastReconnectAttempt = millis();
  }

  SinricPro.handle();

  // Manual button 5-second hold
  if (digitalRead(MANUAL_BTN_PIN) == LOW) {
    if (!btnPressed) {
      btnPressed = true;
      btnPressStart = millis();
    } else if (millis() - btnPressStart >= HOLD_TIME) {
      Serial.println("Manual button held 5s: Trigger Relay2");
      pulseRelay(RELAY2_PIN);

      if (WiFi.status() == WL_CONNECTED) {
        SinricProSwitch &sw2 = SinricPro[DEVICE_ID_2];
        sw2.sendPowerStateEvent(true);
        delay(500);
        sw2.sendPowerStateEvent(false);
      }

      btnPressed = false;
      while (digitalRead(MANUAL_BTN_PIN) == LOW) delay(10);
    }
  } else {
    btnPressed = false;
  }

  server.handleClient(); // Handle Wi-Fi portal requests
}