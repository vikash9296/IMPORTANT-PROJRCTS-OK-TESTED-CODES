/************************************************************************************
 *  ESP32 WiFi + Blynk + Captive Portal + OTA + Reset WiFi
 *  Final Stable Code (with Auto-Reconnect + Captive Redirect)
 *  By: ChatGPT + VIKAS KUMAR
 ************************************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL3LBL5g6Ft"
#define BLYNK_TEMPLATE_NAME "WIFI MOTOR STARTER"
#define BLYNK_AUTH_TOKEN "1qdqld7CwpW9Tqbyiw9tchsdc5qbTMqi"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp32.h>

BlynkTimer timer;
Preferences preferences;

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

char auth[] = BLYNK_AUTH_TOKEN;
String ssid, pass;

#define button1_pin 26
#define button2_pin 25
#define button3_pin 33
#define button4_pin 32

#define relay1_pin 19
#define relay2_pin 21
#define relay3_pin 22
#define relay4_pin 23

#define LED_BUILTIN 2  // WiFi status LED

int relay1_state = 0;
int relay2_state = 0;
int relay3_state = 0;
int relay4_state = 0;

#define button1_vpin V1
#define button2_vpin V2
#define button3_vpin V3
#define button4_vpin V4

// ---------- BLYNK ----------
BLYNK_CONNECTED() {
  Blynk.syncVirtual(button1_vpin, button2_vpin, button3_vpin, button4_vpin);
}

BLYNK_WRITE(button1_vpin) { relay1_state = param.asInt(); digitalWrite(relay1_pin, relay1_state); }
BLYNK_WRITE(button2_vpin) { relay2_state = param.asInt(); digitalWrite(relay2_pin, relay2_state); }
BLYNK_WRITE(button3_vpin) { relay3_state = param.asInt(); digitalWrite(relay3_pin, relay3_state); }
BLYNK_WRITE(button4_vpin) { relay4_state = param.asInt(); digitalWrite(relay4_pin, relay4_state); }

// ---------- RELAY CONTROL ----------
void control_relay(int relay) {
  switch (relay) {
    case 1: relay1_state = !relay1_state; digitalWrite(relay1_pin, relay1_state); break;
    case 2: relay2_state = !relay2_state; digitalWrite(relay2_pin, relay2_state); break;
    case 3: relay3_state = !relay3_state; digitalWrite(relay3_pin, relay3_state); break;
    case 4: relay4_state = !relay4_state; digitalWrite(relay4_pin, relay4_state); break;
  }
  delay(50);
}

void listen_push_buttons() {
  if (digitalRead(button1_pin) == LOW) { delay(200); control_relay(1); Blynk.virtualWrite(button1_vpin, relay1_state); }
  if (digitalRead(button2_pin) == LOW) { delay(200); control_relay(2); Blynk.virtualWrite(button2_vpin, relay2_state); }
  if (digitalRead(button3_pin) == LOW) { delay(200); control_relay(3); Blynk.virtualWrite(button3_vpin, relay3_state); }
  if (digitalRead(button4_pin) == LOW) { delay(200); control_relay(4); Blynk.virtualWrite(button4_vpin, relay4_state); }
}

// ---------- CAPTIVE PORTAL ----------
void startCaptivePortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_SETUP");   // Open network (no password)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", []() {
    String html = "<html><body><h2>ESP32 WiFi Setup</h2>"
                  "<form method='post' action='/save'>"
                  "SSID:<input name='ssid'><br>"
                  "Password:<input name='pass'><br>"
                  "<input type='submit' value='Save'>"
                  "</form><br>"
                  "<form method='post' action='/reset'>"
                  "<input type='submit' value='Reset WiFi'>"
                  "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/save", []() {
    ssid = server.arg("ssid");
    pass = server.arg("pass");
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    server.send(200, "text/html", "<h3>Saved! Restarting ESP...</h3>");
    delay(2000);
    ESP.restart();
  });

  server.on("/reset", []() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/html", "<h3>WiFi credentials cleared! Restarting...</h3>");
    delay(2000);
    ESP.restart();
  });

  server.on("/status", []() {
    String html = "<html><body><h2>ESP32 Status</h2>";
    html += "SSID: " + ssid + "<br>";
    html += "Relay1: " + String(relay1_state) + "<br>";
    html += "Relay2: " + String(relay2_state) + "<br>";
    html += "Relay3: " + String(relay3_state) + "<br>";
    html += "Relay4: " + String(relay4_state) + "<br>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  // Redirect unknown requests (fixes captive portal auto-popup)
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Captive Portal Started (Open Network + Reset Option)");
}

// ---------- WIFI CONNECT ----------
bool connectWiFi() {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid == "") return false;

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    digitalWrite(LED_BUILTIN, HIGH);
    return true;
  } else {
    return false;
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  pinMode(button1_pin, INPUT_PULLUP);
  pinMode(button2_pin, INPUT_PULLUP);
  pinMode(button3_pin, INPUT_PULLUP);
  pinMode(button4_pin, INPUT_PULLUP);

  pinMode(relay1_pin, OUTPUT);
  pinMode(relay2_pin, OUTPUT);
  pinMode(relay3_pin, OUTPUT);
  pinMode(relay4_pin, OUTPUT);

  digitalWrite(relay1_pin, HIGH);
  digitalWrite(relay2_pin, HIGH);
  digitalWrite(relay3_pin, HIGH);
  digitalWrite(relay4_pin, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  if (!connectWiFi()) {
    startCaptivePortal();
  } else {
    Blynk.config(auth);
    Blynk.connect();
    ArduinoOTA.begin();
  }
}

// ---------- LOOP ----------
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
    ArduinoOTA.handle();
  } else {
    // Captive portal running
    dnsServer.processNextRequest();
    server.handleClient();

    // Auto reconnect mechanism
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {  // retry every 10s
      lastReconnect = millis();
      Serial.println("WiFi lost... retrying");
      connectWiFi();
    }
  }
  timer.run();
  listen_push_buttons();
}