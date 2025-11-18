#include <Arduino.h>
#include <WiFi.h>
#include "ESP32MQTTClient.h"

const char* ssid = ""; 
const char* pass = ""; 
const char* mqttServer = "mqtt://157.230.250.226:1883";
const char* mqttTopic = "nus-smartstop/ultrasonic/data";

const int SENSOR_COUNT = 3;
const int trigPins[SENSOR_COUNT] = {5, 22, 13};
const int echoPins[SENSOR_COUNT] = {18, 23, 14};
const String SECTION_NAMES[SENSOR_COUNT] = {"LEFT", "CENTER", "RIGHT"};

const float MIN_DISTANCE = 0.0; // cm
const int READ_INTERVAL = 3000; 

// Moving average parameters
const int MOVING_AVG_SIZE = 3;  // Number of samples in the moving average
float sensorBuffer[SENSOR_COUNT][MOVING_AVG_SIZE]; 
int bufferIndex[SENSOR_COUNT] = {0};

float baseline[SENSOR_COUNT];       // baseline floor distances
float triggerThreshold[SENSOR_COUNT]; // baseline - buffer
const float BUFFER = 0.2;           // buffer in cm below baseline to prevent false positives

float sensorDistances[SENSOR_COUNT];
bool sensorStates[SENSOR_COUNT];
float density = 0.0;

ESP32MQTTClient mqttClient;

float readSensor(int index);
void readAllSensors();
void publishData();
int occupiedVal(bool state);
void calculateDensity();
void calibrateSensors();
float movingAverage(int sensorIndex, float newReading);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  delay(500);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    digitalWrite(trigPins[i], LOW);
    for (int j = 0; j < MOVING_AVG_SIZE; j++) sensorBuffer[i][j] = 0;
  }

  // Setup MQTT
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(mqttServer);
  mqttClient.setKeepAlive(30);
  mqttClient.enableLastWillMessage("nus-smartstop/lwt", "Ultrasonic node offline");
  mqttClient.loopStart();
  delay(2000);

  calibrateSensors(); // calibrate baseline and set thresholds
}

void loop() {
  readAllSensors();
  calculateDensity();
  publishData();
  delay(READ_INTERVAL);
}

void calibrateSensors() {
  Serial.println("\nCALIBRATION MODE");
  delay(4000); 

  for (int i = 0; i < SENSOR_COUNT; i++) {
    float total = 0;
    const int samples = 5;
    Serial.print("Calibrating ");
    Serial.println(SECTION_NAMES[i]);

    for (int j = 0; j < samples; j++) {
      float d = readSensor(i);
      total += d;
      delay(400);
    }
    baseline[i] = total / samples;
    triggerThreshold[i] = baseline[i] - BUFFER;

    for (int k = 0; k < MOVING_AVG_SIZE; k++) sensorBuffer[i][k] = baseline[i];

    Serial.print("Baseline for ");
    Serial.print(SECTION_NAMES[i]);
    Serial.print(": ");
    Serial.print(baseline[i]);
    Serial.print(" cm, Trigger threshold: ");
    Serial.println(triggerThreshold[i]);
  }
  Serial.println("Calibration complete!");
}

void readAllSensors() {
  Serial.println("\nReading sensors");

  for (int i = 0; i < SENSOR_COUNT; i++) {

    delay(80);

    float rawReading = readSensor(i);
    sensorDistances[i] = movingAverage(i, rawReading);
    sensorStates[i] = (sensorDistances[i] > 0 && sensorDistances[i] <= triggerThreshold[i]);

    delay(60);
  }

  Serial.println("\nSensor Readings");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(SECTION_NAMES[i]);
    Serial.print(": ");
    Serial.print(sensorDistances[i]);
    Serial.print(" cm -> ");
    Serial.println(sensorStates[i] ? "OCCUPIED" : "EMPTY");
  }
}


float movingAverage(int sensorIndex, float newReading) {
  sensorBuffer[sensorIndex][bufferIndex[sensorIndex]] = newReading;
  bufferIndex[sensorIndex] = (bufferIndex[sensorIndex] + 1) % MOVING_AVG_SIZE;

  float sum = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) sum += sensorBuffer[sensorIndex][i];
  return sum / MOVING_AVG_SIZE;
}

float readSensor(int i) {
  const int samples = 3;
  float total = 0;
  int validCount = 0;

  for (int s = 0; s < samples; s++) {
    delay(5);
    digitalWrite(trigPins[i], LOW);
    delayMicroseconds(3);
    digitalWrite(trigPins[i], HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPins[i], LOW);
    long duration = pulseIn(echoPins[i], HIGH, 30000);

    if (duration > 0) {
      float distance = duration * 0.034 / 2.0;
      total += distance;
      validCount++;
    }

    delay(40);  
  }

  if (validCount == 0) return 0;
  return total / validCount;
}

int occupiedVal(bool state) {
  return state ? 1 : 0;
}

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
    Serial.println("MQTT not connected. Skipping publish");
    return;
  }

  char payload[400];
  snprintf(payload, sizeof(payload),
           "{\"sensors\": {"
           "\"LEFT\": {\"distance\": %.2f, \"occupied\": %d}, "
           "\"CENTER\": {\"distance\": %.2f, \"occupied\": %d}, "
           "\"RIGHT\": {\"distance\": %.2f, \"occupied\": %d}}, "
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
