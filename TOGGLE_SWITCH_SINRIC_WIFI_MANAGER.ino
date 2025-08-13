/**********************************************************************************
 *  ESP32 + Sinric Pro + WiFiManager + Manual Switch/Button control (4 Relays)
 *  Dynamic WiFi config via WiFiManager (no hardcoded SSID/PASS)
 **********************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <map>

// --- Sinric Pro Credentials ---
#define APP_KEY     "YOUR-APP-KEY"      
#define APP_SECRET  "YOUR-APP-SECRET"   

// Device IDs
#define device_ID_1 "SWITCH_ID_NO_1_HERE"
#define device_ID_2 "SWITCH_ID_NO_2_HERE"
#define device_ID_3 "SWITCH_ID_NO_3_HERE"
#define device_ID_4 "SWITCH_ID_NO_4_HERE"

// GPIO pin assignments
#define RelayPin1 23
#define RelayPin2 22
#define RelayPin3 21
#define RelayPin4 19

#define SwitchPin1 13
#define SwitchPin2 12
#define SwitchPin3 14
#define SwitchPin4 27

#define wifiLed 2  // Built-in LED

#define BAUD_RATE     9600
#define DEBOUNCE_TIME 250
//#define TACTILE_BUTTON 1

typedef struct {
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;

std::map<String, deviceConfig_t> devices = {
    {device_ID_1, {RelayPin1, SwitchPin1}},
    {device_ID_2, {RelayPin2, SwitchPin2}},
    {device_ID_3, {RelayPin3, SwitchPin3}},
    {device_ID_4, {RelayPin4, SwitchPin4}}     
};

typedef struct {
  String deviceId;
  bool lastFlipSwitchState;
  unsigned long lastFlipSwitchChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;

void setupRelays() { 
  for (auto &device : devices) {
    pinMode(device.second.relayPIN, OUTPUT);
    digitalWrite(device.second.relayPIN, HIGH);
  }
}

void setupFlipSwitches() {
  for (auto &device : devices) {
    flipSwitchConfig_t cfg;
    cfg.deviceId = device.first;
    cfg.lastFlipSwitchChange = 0;
    cfg.lastFlipSwitchState = true;
    int pin = device.second.flipSwitchPIN;
    flipSwitches[pin] = cfg;
    pinMode(pin, INPUT_PULLUP);
  }
}

bool onPowerState(String deviceId, bool &state) {
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN;
  digitalWrite(relayPIN, !state);
  return true;
}

void handleFlipSwitches() {
  unsigned long now = millis();
  for (auto &sw : flipSwitches) {
    if (now - sw.second.lastFlipSwitchChange > DEBOUNCE_TIME) {
      int pin = sw.first;
      bool lastState = sw.second.lastFlipSwitchState;
      bool state = digitalRead(pin);
      if (state != lastState) {
#ifdef TACTILE_BUTTON
        if (state) {
#endif
          sw.second.lastFlipSwitchChange = now;
          String deviceId = sw.second.deviceId;
          int relayPIN = devices[deviceId].relayPIN;
          bool newRelayState = !digitalRead(relayPIN);
          digitalWrite(relayPIN, newRelayState);
          SinricProSwitch &mySwitch = SinricPro[deviceId];
          mySwitch.sendPowerStateEvent(!newRelayState);
#ifdef TACTILE_BUTTON
        }
#endif
        sw.second.lastFlipSwitchState = state;
      }
    }
  }
}

void setupWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // Auto-close after 3 mins
  if (!wm.autoConnect("ESP32-SINRIC-WIFI-Setup", "12345678")) {
    Serial.println("⚠️ Failed to connect, restarting...");
    ESP.restart();
  }
  Serial.printf("[WiFi] Connected! IP: %s\r\n", WiFi.localIP().toString().c_str());
  digitalWrite(wifiLed, HIGH);
}

void setupSinricPro() {
  for (auto &device : devices) {
    SinricProSwitch &mySwitch = SinricPro[device.first.c_str()];
    mySwitch.onPowerState(onPowerState);
  }
  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, LOW);
  setupRelays();
  setupFlipSwitches();
  setupWiFi();      // Dynamic WiFi via WiFiManager
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  handleFlipSwitches();
}
