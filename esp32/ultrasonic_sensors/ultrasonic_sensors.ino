#include <Arduino.h>
#include <WiFi.h>
#include "ESP32MQTTClient.h"

const char* ssid = "YOUR_WIFI_SSID"; 
const char* pass = "YOUR_WIFI_PASSWORD"; 
const char* mqttServer = "mqtt://YOUR_MQTT_BROKER_IP:1883";
const char* mqttTopic = "nus-smartstop/ultrasonic/data";

const int SENSOR_COUNT = 3;
const int trigPins[SENSOR_COUNT] = {5, 22, 13};
const int echoPins[SENSOR_COUNT] = {18, 23, 14};
const String SECTION_NAMES[SENSOR_COUNT] = {"LEFT", "CENTER", "RIGHT"};

const int MIN_DISTANCE = 200; //20cm for testing purposes
const int MAX_DISTANCE = 400; //in cm
const int READ_INTERVAL = 2000; 

int sensorDistances[SENSOR_COUNT];
bool sensorStates[SENSOR_COUNT]; // true = occupied
float density = 0.0; // overall bus stop density

ESP32MQTTClient mqttClient;

int readSensor(int index);
void readAllSensors();
void publishData();
int occupiedVal(bool state);
void calculateDensity();

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  delay(1000);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    digitalWrite(trigPins[i], LOW);
  }

  // Setup MQTT
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(mqttServer);
  mqttClient.setKeepAlive(30);
  mqttClient.enableLastWillMessage("nus-smartstop/lwt", "Ultrasonic node offline");
  mqttClient.loopStart();
  delay(2000);  
}

void loop() {
  readAllSensors();
  calculateDensity();
  publishData();
  delay(READ_INTERVAL);
}

void readAllSensors() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    sensorDistances[i] = readSensor(i);
    sensorStates[i] = (sensorDistances[i] >= MIN_DISTANCE && sensorDistances[i] <= MAX_DISTANCE); 
  }

  Serial.println("\n--- Sensor Readings ---");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(SECTION_NAMES[i]);
    Serial.print(": ");
    if (sensorDistances[i] == 0) {
      Serial.println("[NO SIGNAL]");
    } else {
      Serial.print(sensorDistances[i]);
      Serial.print(" cm -> ");
      Serial.println(sensorStates[i] ? "OCCUPIED" : "EMPTY");
    }
  }
}

int readSensor(int i) {
  digitalWrite(trigPins[i], LOW);
  delayMicroseconds(2);
  digitalWrite(trigPins[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPins[i], LOW);

  long duration = pulseIn(echoPins[i], HIGH, 30000); 
  if (duration == 0) return 0; // nothing sensed
  return duration * 0.034 / 2;  // convert to cm
}

int occupiedVal(bool state) {
  return state ? 1 : 0;
}

//added a density number for how crowded the bus stop is
void calculateDensity() {
  int triggeredCount = 0;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (sensorStates[i]) triggeredCount++;
  }
  density = triggeredCount / (float)SENSOR_COUNT; 
  Serial.print("Bus stop density: ");
  Serial.println(density);
}


void publishData() {
  if (!mqttClient.isConnected()) {
    Serial.println("MQTT not connected. Skipping publish...");
    return;
  }

  char payload[400];
  snprintf(payload, sizeof(payload),
           "{\"sensors\": {"
           "\"LEFT\": {\"distance\": %d, \"occupied\": %d}, "
           "\"CENTER\": {\"distance\": %d, \"occupied\": %d}, "
           "\"RIGHT\": {\"distance\": %d, \"occupied\": %d}}, "
           "\"density\": %.2f}",
           sensorDistances[0], occupiedVal(sensorStates[0]),
           sensorDistances[1], occupiedVal(sensorStates[1]),
           sensorDistances[2], occupiedVal(sensorStates[2]),
           density
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
