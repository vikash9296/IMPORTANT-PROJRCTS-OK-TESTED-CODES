/******** Sinric Pro + WiFiManager | Toggle Relay Control | 3 Channels ********/

#include <WiFiManager.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <Preferences.h>

// Sinric Pro credentials
#define APP_KEY     "c7303938-59fa-4ad2-a3de-e62d9bae9cec"
#define APP_SECRET  "c41c8855-b93b-496c-bf27-43f0f72c2dc8-4e754444-82cd-4acb-bb2c-c9cfe22b9b13"

#define DEVICE_ID_1 "6889cacbedeca866fe96e2ee"
#define DEVICE_ID_2 "6889caffddd2551252ba8a70"
#define DEVICE_ID_3 "6889cb32edeca866fe96e336"

// Relay and Button Pins
#define RELAY1_PIN  19
#define RELAY2_PIN  21
#define RELAY3_PIN  22

#define BUTTON1_PIN 13
#define BUTTON2_PIN 12
#define BUTTON3_PIN 14

#define WIFI_LED_PIN 2

Preferences prefs;

// Relay States
bool relayState1, relayState2, relayState3;

void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);  // active LOW
}

void saveState() {
  prefs.putBool("relay1", relayState1);
  prefs.putBool("relay2", relayState2);
  prefs.putBool("relay3", relayState3);
}

void toggleRelay(int relayNum) {
  switch (relayNum) {
    case 1:
      relayState1 = !relayState1;
      setRelay(RELAY1_PIN, relayState1);
      SinricPro[DEVICE_ID_1].sendPowerStateEvent(relayState1);
      break;
    case 2:
      relayState2 = !relayState2;
      setRelay(RELAY2_PIN, relayState2);
      SinricPro[DEVICE_ID_2].sendPowerStateEvent(relayState2);
      break;
    case 3:
      relayState3 = !relayState3;
      setRelay(RELAY3_PIN, relayState3);
      SinricPro[DEVICE_ID_3].sendPowerStateEvent(relayState3);
      break;
  }
  saveState();
}

// IRAM_ATTR = fast interrupt function
IRAM_ATTR void handleButton1() { toggleRelay(1); }
IRAM_ATTR void handleButton2() { toggleRelay(2); }
IRAM_ATTR void handleButton3() { toggleRelay(3); }

bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId == DEVICE_ID_1) { relayState1 = state; setRelay(RELAY1_PIN, state); }
  if (deviceId == DEVICE_ID_2) { relayState2 = state; setRelay(RELAY2_PIN, state); }
  if (deviceId == DEVICE_ID_3) { relayState3 = state; setRelay(RELAY3_PIN, state); }
  saveState();
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

void setup() {
  Serial.begin(115200);
  pinMode(WIFI_LED_PIN, OUTPUT);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), handleButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), handleButton2, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON3_PIN), handleButton3, FALLING);

  // WiFiManager Setup
  WiFiManager wm;
  if (!wm.autoConnect("ESP32_SINRIC", "12345678")) {
    Serial.println("Failed to connect.");
    ESP.restart();
  }
  digitalWrite(WIFI_LED_PIN, HIGH);
  Serial.println("WiFi connected!");

  // Load Relay States from Preferences
  prefs.begin("relayStates", false);
  relayState1 = prefs.getBool("relay1", false);
  relayState2 = prefs.getBool("relay2", false);
  relayState3 = prefs.getBool("relay3", false);

  setRelay(RELAY1_PIN, relayState1);
  setRelay(RELAY2_PIN, relayState2);
  setRelay(RELAY3_PIN, relayState3);

  setupSinricPro();
}

void loop() {
  SinricPro.handle();
}