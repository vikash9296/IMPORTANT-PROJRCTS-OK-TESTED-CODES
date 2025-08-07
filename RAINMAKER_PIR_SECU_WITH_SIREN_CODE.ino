/*************************************************************************************************
 *  Created By:  VIKASH KUMAR
 *  
 *  YouTube Video: https://youtu.be/hjTTUl0yP94
 ***********************************************************************************************/

#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

// BLE Provisioning
const char *service_name = "VIKASH_BLE";
const char *pop = "12345678";

// GPIO definitions
#define WIFI_LED      2
#define RESET_PIN     0
#define PIR_PIN       4
#define BUZZER_PIN    21
#define BIG_SIREN_PIN 22  // Your external siren

// PIR Sensor state
boolean PIR_STATE_NEW = LOW;
boolean PIR_STATE_OLD = LOW;

// Buzzer state
boolean BUZZER_STATE = false;
unsigned long buzzer_timer = 0;

// Big Siren logic
unsigned long motion_start_time = 0;
bool motion_timer_started = false;
bool BIG_SIREN_STATE = false;

// Device names
char device1[] = "SecuritySwitch";
char device2[] = "BigSirenSwitch";

// RainMaker Devices
static Switch SecuritySwitch(device1, NULL);
static Switch BigSirenSwitch(device2, NULL);

bool SECURITY_STATE = false;
uint32_t chipId = 0;

/****************************************************************************************************
 * Provisioning Events
*****************************************************************************************************/
void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      printQR(service_name, pop, "ble");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      printQR(service_name, pop, "softap");
#endif
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("Connected to Wi-Fi!");
      digitalWrite(WIFI_LED, HIGH);
      break;
  }
}

/****************************************************************************************************
 * RainMaker Write Callback
*****************************************************************************************************/
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(device_name, device1) == 0) {
    Serial.printf("SecuritySwitch = %s\n", val.val.b ? "true" : "false");

    if (strcmp(param_name, "Power") == 0) {
      SECURITY_STATE = val.val.b;
      param->updateAndReport(val);

      if (SECURITY_STATE)
        esp_rmaker_raise_alert("Security is ON");
      else
        esp_rmaker_raise_alert("Security is OFF");
    }
  }

  if (strcmp(device_name, device2) == 0) {
    Serial.printf("BigSirenSwitch = %s\n", val.val.b ? "true" : "false");

    if (strcmp(param_name, "Power") == 0) {
      BIG_SIREN_STATE = val.val.b;
      param->updateAndReport(val);
      digitalWrite(BIG_SIREN_PIN, BIG_SIREN_STATE ? HIGH : LOW);
    }
  }
}

/****************************************************************************************************
 * Setup Function
*****************************************************************************************************/
void setup() {
  Serial.begin(115200);

  pinMode(RESET_PIN, INPUT);
  pinMode(WIFI_LED, OUTPUT); digitalWrite(WIFI_LED, LOW);

  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(PIR_PIN, INPUT);
  pinMode(BIG_SIREN_PIN, OUTPUT); digitalWrite(BIG_SIREN_PIN, LOW);

  Node my_node = RMaker.initNode("PIR_Security_Alarm");

  SecuritySwitch.addCb(write_callback);
  BigSirenSwitch.addCb(write_callback);

  my_node.addDevice(SecuritySwitch);
  my_node.addDevice(BigSirenSwitch);

  RMaker.enableOTA(OTA_USING_PARAMS);
  RMaker.enableTZService();
  RMaker.enableSchedule();

  for (int i = 0; i < 17; i = i + 8)
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;

  Serial.printf("Chip ID: %d Service Name: %s\n", chipId, service_name);

  RMaker.start();

  WiFi.onEvent(sysProvEvent);

#if CONFIG_IDF_TARGET_ESP32
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#endif

  SecuritySwitch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
  BigSirenSwitch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
}

/****************************************************************************************************
 * Loop Function
*****************************************************************************************************/
void loop() {
  // Handle reset button
  if (digitalRead(RESET_PIN) == LOW) {
    delay(100);
    int startTime = millis();
    while (digitalRead(RESET_PIN) == LOW) delay(50);
    int endTime = millis();

    if ((endTime - startTime) > 10000) {
      Serial.println("Factory Reset");
      RMakerFactoryReset(2);
    }
    else if ((endTime - startTime) > 3000) {
      Serial.println("WiFi Reset");
      RMakerWiFiReset(2);
    }
  }

  // WiFi LED status
  if (WiFi.status() != WL_CONNECTED)
    digitalWrite(WIFI_LED, LOW);
  else
    digitalWrite(WIFI_LED, HIGH);

  detectMotion();
  controlBuzzer();
}

/****************************************************************************************************
 * Detect Motion Function
*****************************************************************************************************/
void detectMotion() {
  if (SECURITY_STATE == true) {
    PIR_STATE_OLD = PIR_STATE_NEW;
    PIR_STATE_NEW = digitalRead(PIR_PIN);

    if (PIR_STATE_OLD == LOW && PIR_STATE_NEW == HIGH) {
      Serial.println("Motion detected!");
      esp_rmaker_raise_alert("Security Alert! Motion detected.");
      digitalWrite(BUZZER_PIN, HIGH);
      BUZZER_STATE = true;
      buzzer_timer = millis();

      // Start motion timer
      motion_start_time = millis();
      motion_timer_started = true;
    }

    // Continue HIGH -> siren trigger after 60s
    if (PIR_STATE_NEW == HIGH && motion_timer_started) {
      if ((millis() - motion_start_time) >= 60000 && BIG_SIREN_STATE == false) {
        Serial.println("Big Siren Activated!");
        digitalWrite(BIG_SIREN_PIN, HIGH);
        BIG_SIREN_STATE = true;
        BigSirenSwitch.updateAndReportParam("Power", true);
      }
    }

    // Reset timer if motion ends
    if (PIR_STATE_NEW == LOW) {
      motion_timer_started = false;
      motion_start_time = 0;
    }
  }
}

/****************************************************************************************************
 * Buzzer Control Function
*****************************************************************************************************/
void controlBuzzer() {
  if (BUZZER_STATE == true) {
    if (millis() - buzzer_timer > 5000) {
      digitalWrite(BUZZER_PIN, LOW);
      BUZZER_STATE = false;
      buzzer_timer = 0;
    }
  }
}