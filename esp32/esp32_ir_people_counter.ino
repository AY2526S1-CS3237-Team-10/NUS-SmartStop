#include <Arduino.h>
#include <WiFi.h>
#include "ESP32MQTTClient.h"

const char* ssid = "so-taroble"; 
const char* pass = "sotaro12345"; 

const char* mqttServer = "mqtt://157.230.250.226:1883";
const char* mqttTopic = "nus-smartstop/ir-sensor/data";

// ======== PIN DEFINITIONS ========
const int IR_TX_A = 26;
const int IR_TX_B = 27;
const int IR_RX_A = 34;
const int IR_RX_B = 35;

// ======== VARIABLES ========
ESP32MQTTClient mqttClient;

volatile float peopleCount = 0;
int lastA = HIGH, lastB = HIGH;
unsigned long lastTriggerTime = 0;
const unsigned long sequenceWindow = 1000;

// ======== FUNCTIONS ========
void publishData();

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, pass);
  delay(1000);

  pinMode(IR_TX_A, OUTPUT);
  pinMode(IR_TX_B, OUTPUT);
  pinMode(IR_RX_A, INPUT);
  pinMode(IR_RX_B, INPUT);

  digitalWrite(IR_TX_A, HIGH);
  digitalWrite(IR_TX_B, HIGH);

  // Setup MQTT
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(mqttServer);
  mqttClient.setKeepAlive(30);
  mqttClient.enableLastWillMessage("nus-smartstop/lwt", "IR sensor node offline");
  mqttClient.loopStart();
  
  delay(2000);
}

void loop() {
  int currA = digitalRead(IR_RX_A);
  int currB = digitalRead(IR_RX_B);
  unsigned long now = millis();

  // Entry detection
  if (lastA == HIGH && currA == LOW) {
    lastTriggerTime = now;
    while (millis() - lastTriggerTime < sequenceWindow) {
      if (digitalRead(IR_RX_B) == LOW) {
        peopleCount++;
        Serial.println("Count: " + String(peopleCount));
        publishData();
        delay(500);
        break;
      }
    }
  }

  // Exit detection
  if (lastB == HIGH && currB == LOW) {
    lastTriggerTime = now;
    while (millis() - lastTriggerTime < sequenceWindow) {
      if (digitalRead(IR_RX_A) == LOW) {
        peopleCount--;
        if (peopleCount < 0) peopleCount = 0;
        Serial.println("Count: " + String(peopleCount));
        publishData();
        delay(500);
        break;
      }
    }
  }

  lastA = currA;
  lastB = currB;
}

void publishData() {
  if (!mqttClient.isConnected()) {
    Serial.println("MQTT not connected. Skipping publish...");
    return;
  }

  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"people_count\": %.1f, \"location\": \"busstop\"}",
           peopleCount
  );

  Serial.println("\nPublishing to MQTT:");
  Serial.println(payload);
  
  mqttClient.publish(mqttTopic, payload, 0, false);
}

void onMqttConnect(esp_mqtt_client* client) {
  Serial.println("MQTT Connected!");
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
  return ESP_OK;
}
#else
void handleMQTT(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
  mqttClient.onEventCallback(event);
}
#endif