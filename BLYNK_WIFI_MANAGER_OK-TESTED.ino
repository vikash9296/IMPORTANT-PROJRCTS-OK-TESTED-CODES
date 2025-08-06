/************************************************************************************
 *  Modified by: ChatGPT + VIKASH KUMAR
 *  Added: WiFiManager for dynamic WiFi configuration
 ************************************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL3LBL5g6Ft"
#define BLYNK_TEMPLATE_NAME "WIFI MOTOR STARTER"
#define BLYNK_AUTH_TOKEN "1qdqld7CwpW9Tqbyiw9tchsdc5qbTMqi"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>  // Add this library

BlynkTimer timer;
char auth[] = BLYNK_AUTH_TOKEN;

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

BLYNK_CONNECTED() {
  Blynk.syncVirtual(button1_vpin);
  Blynk.syncVirtual(button2_vpin);
  Blynk.syncVirtual(button3_vpin);
  Blynk.syncVirtual(button4_vpin);
}

BLYNK_WRITE(button1_vpin) {
  relay1_state = param.asInt();
  digitalWrite(relay1_pin, relay1_state);
}

BLYNK_WRITE(button2_vpin) {
  relay2_state = param.asInt();
  digitalWrite(relay2_pin, relay2_state);
}

BLYNK_WRITE(button3_vpin) {
  relay3_state = param.asInt();
  digitalWrite(relay3_pin, relay3_state);
}

BLYNK_WRITE(button4_vpin) {
  relay4_state = param.asInt();
  digitalWrite(relay4_pin, relay4_state);
}

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

  // WiFiManager
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);  // auto exit config portal after 3 minutes
  if (!wm.autoConnect("ESP32_SETUP", "12345678")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  Serial.println("WiFi Connected!");
  digitalWrite(LED_BUILTIN, HIGH);

  Blynk.config(auth);
  Blynk.connect();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  Blynk.run();
  timer.run();
  listen_push_buttons();
}

void listen_push_buttons() {
  if (digitalRead(button1_pin) == LOW) {
    delay(200);
    control_relay(1);
    Blynk.virtualWrite(button1_vpin, relay1_state);
  } else if (digitalRead(button2_pin) == LOW) {
    delay(200);
    control_relay(2);
    Blynk.virtualWrite(button2_vpin, relay2_state);
  } else if (digitalRead(button3_pin) == LOW) {
    delay(200);
    control_relay(3);
    Blynk.virtualWrite(button3_vpin, relay3_state);
  } else if (digitalRead(button4_pin) == LOW) {
    delay(200);
    control_relay(4);
    Blynk.virtualWrite(button4_vpin, relay4_state);
  }
}

void control_relay(int relay) {
  switch (relay) {
    case 1:
      relay1_state = !relay1_state;
      digitalWrite(relay1_pin, relay1_state);
      break;
    case 2:
      relay2_state = !relay2_state;
      digitalWrite(relay2_pin, relay2_state);
      break;
    case 3:
      relay3_state = !relay3_state;
      digitalWrite(relay3_pin, relay3_state);
      break;
    case 4:
      relay4_state = !relay4_state;
      digitalWrite(relay4_pin, relay4_state);
      break;
  }
  delay(50);
}