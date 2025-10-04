// ================== Libraries ==================
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <DNSServer.h>

// ------------------- Sinric Pro Credentials -------------------
#define APP_KEY       "c7303938-59fa-4ad2-a3de-e62d9bae9cec"
#define APP_SECRET    "c41c8855-b93b-496c-bf27-43f0f72c2dc8-4e754444-82cd-4acb-bb2c-c9cfe22b9b13"
#define DEVICE_ID_1   "6889cacbedeca866fe96e2ee"
#define DEVICE_ID_2   "6889caffddd2551252ba8a70"
#define DEVICE_3_ID   "6890c55fedeca866fe994705"

// ------------------- Relay pins -------------------
#define RELAY1_PIN    19
#define RELAY2_PIN    21   // Active LOW relay
#define AUTO_MODE_LED 22

// ------------------- Wi-Fi LED -------------------
#define WIFI_LED_PIN  2

// ------------------- Manual button -------------------
#define MANUAL_BTN_PIN 27
#define HOLD_TIME     5000  // 5 seconds hold

#define PULSE_TIME    2000  // 2 seconds pulse
#define AUTO_DELAY    40000 // 40 seconds

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

// Web server + DNS server for captive portal
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// ================= Relay Control =================
void setupRelay(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH); // OFF for active-low relay
}

void pulseRelay(int pin, int time = PULSE_TIME) {
  digitalWrite(pin, LOW);   // Relay ON
  delay(time);
  digitalWrite(pin, HIGH);  // Relay OFF
  Serial.printf("Relay on pin %d triggered for %d ms\n", pin, time);
}

// ================= Preferences =================
void restoreAutoMode() {
  preferences.begin("settings", false);
  autoMode = preferences.getBool("autoMode", false);
  preferences.end();

  digitalWrite(AUTO_MODE_LED, autoMode ? HIGH : LOW);

  if (autoMode) {
    Serial.println("Auto Mode ON: Waiting 40s to trigger Relay1 on boot");
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

// ================= Local Web Control =================
void handleControlPage() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;text-align:center;} input,button{width:80%;padding:15px;margin:10px;font-size:18px;border:none;border-radius:8px;} ";
  html += ".on{background:#4CAF50;color:white;} .off{background:#f44336;color:white;} .auto{background:#2196F3;color:white;}</style></head><body>";
  html += "<h1>Pump Controller Portal</h1>";

  // ---- WiFi Setup Form ----
  html += "<h2>WiFi Setup</h2>";
  html += "<form action='/save' method='POST'>";
  html += "SSID:<br><input name='ssid'><br>";
  html += "Password:<br><input name='pass' type='password'><br>";
  html += "<input type='submit' value='Save WiFi'>";
  html += "</form><hr>";

  // ---- Local Control Section ----
  html += "<h2>Local Control</h2>";
  html += "<button class='on' onclick=\"location.href='/relay1/on'\">Start Pump</button><br>";
  html += "<button class='off' onclick=\"location.href='/relay2/on'\">Stop Pump</button><br>";
  if (autoMode) {
    html += "<button class='auto' onclick=\"location.href='/auto/off'\">Disable Auto Mode</button><br>";
  } else {
    html += "<button class='auto' onclick=\"location.href='/auto/on'\">Enable Auto Mode</button><br>";
  }

  html += "<p>Auto Mode: <b>" + String(autoMode ? "Enabled" : "Disabled") + "</b></p>";
  html += "<p>WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "AP Mode") + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Relay / Auto handlers
void handleRelay1() { pulseRelay(RELAY1_PIN); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }
void handleRelay2() { pulseRelay(RELAY2_PIN); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }
void handleAutoOn() { autoMode=true; preferences.begin("settings",false); preferences.putBool("autoMode",autoMode); preferences.end(); digitalWrite(AUTO_MODE_LED,HIGH); server.sendHeader("Location","/",true); server.send(302,"text/plain",""); }
void handleAutoOff() { autoMode=false; preferences.begin("settings",false); preferences.putBool("autoMode",autoMode); preferences.end(); digitalWrite(AUTO_MODE_LED,LOW); server.sendHeader("Location","/",true); server.send(302,"text/plain",""); }

void registerWebHandlers() {
  server.on("/", handleControlPage);
  server.on("/relay1/on", handleRelay1);
  server.on("/relay2/on", handleRelay2);
  server.on("/auto/on", handleAutoOn);
  server.on("/auto/off", handleAutoOff);
}

// ================= Captive Portal WiFi Save =================
void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    ssid = server.arg("ssid");
    password = server.arg("pass");
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    server.send(200, "text/html", "<h2>Saved! Restarting...</h2>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "Missing SSID or Password!");
  }
}

void startAPMode() {
  WiFi.softAP("PumpController_AP","12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(IP);

  dnsServer.start(DNS_PORT, "*", IP);

  server.onNotFound([IP]() {
    server.sendHeader("Location", String("http://")+IP.toString(), true);
    server.send(302, "text/plain","");
  });

  server.on("/save", handleSave);
  registerWebHandlers(); // unified page
  server.begin();
  Serial.println("Captive portal started...");
}

// Connect using saved Wi-Fi
void connectToWiFi() {
  preferences.begin("wifi", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();

  if (ssid!="" && password!="") {
    Serial.printf("Connecting to %s", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long startAttemptTime = millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-startAttemptTime<10000){delay(500); Serial.print(".");}
  }

  if(WiFi.status()==WL_CONNECTED){
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED_PIN,HIGH);
    registerWebHandlers(); // local control in STA mode
    server.begin();
  } else {
    Serial.println("\nFailed to connect. Starting AP mode...");
    digitalWrite(WIFI_LED_PIN,LOW);
    startAPMode();
  }
}

// ================= OTA =================
void setupOTA(){ArduinoOTA.setHostname("PumpController_OTA"); ArduinoOTA.begin();}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  bootTime=millis();
  pinMode(WIFI_LED_PIN,OUTPUT); digitalWrite(WIFI_LED_PIN,LOW);
  setupRelay(RELAY1_PIN); setupRelay(RELAY2_PIN);
  pinMode(AUTO_MODE_LED,OUTPUT); digitalWrite(AUTO_MODE_LED,LOW);
  pinMode(MANUAL_BTN_PIN,INPUT_PULLUP);
  restoreAutoMode();
  connectToWiFi();
  WiFi.setAutoReconnect(true);
  setupOTA();
  setupSinricPro();
}

// ================= Loop =================
void loop() {
  ArduinoOTA.handle();

  // WiFi reconnect
  if(WiFi.status()!=WL_CONNECTED){
    digitalWrite(WIFI_LED_PIN,LOW);
    if(millis()-lastReconnectAttempt>reconnectInterval){
      Serial.println("WiFi lost. Attempting reconnect...");
      WiFi.begin(ssid.c_str(),password.c_str());
      lastReconnectAttempt=millis();
    }
  } else { digitalWrite(WIFI_LED_PIN,HIGH); }

  // SinricPro reconnect
  if(!SinricPro.isConnected() && millis()-lastReconnectAttempt>reconnectInterval){
    Serial.println("SinricPro disconnected. Attempting reconnect...");
    SinricPro.begin(APP_KEY,APP_SECRET);
    lastReconnectAttempt=millis();
  }

  SinricPro.handle();

  // Manual button trigger
  if(digitalRead(MANUAL_BTN_PIN)==LOW){
    if(!btnPressed){ btnPressed=true; btnPressStart=millis();}
    else if(millis()-btnPressStart>=HOLD_TIME){
      Serial.println("Manual button held 5s: Trigger Relay2");
      pulseRelay(RELAY2_PIN);
      if(WiFi.status()==WL_CONNECTED){
        SinricProSwitch &sw2=SinricPro[DEVICE_ID_2];
        sw2.sendPowerStateEvent(true);
        delay(500);
        sw2.sendPowerStateEvent(false);
      }
      btnPressed=false;
      while(digitalRead(MANUAL_BTN_PIN)==LOW) delay(10);
    }
  } else btnPressed=false;

  dnsServer.processNextRequest();
  server.handleClient();
}