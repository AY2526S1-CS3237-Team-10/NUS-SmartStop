/*
 * Smart Bus Stop - ESP32 Main Code
 * ESP32 DOIT DevKit V1
 * 
 * Features:
 * - Camera capture and upload
 * - Ultrasonic sensor for distance measurement
 * - MQTT communication
 * - Audio playback via speaker
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server endpoints
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* flask_server = "http://YOUR_FLASK_SERVER_IP:5000";

// Device configuration
const char* device_id = "esp32_001";
const char* location = "bus_stop_01";

// MQTT topics (using nus-smartstop prefix)
const char* topic_sensors = "nus-smartstop/sensors/ultrasonic";
const char* topic_camera = "nus-smartstop/camera";
const char* topic_command = "nus-smartstop/command/esp32_001";

// Pin definitions for ESP32-CAM or external camera module
// Adjust based on your specific ESP32 board and camera module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Ultrasonic sensor pins
#define TRIG_PIN 12
#define ECHO_PIN 14

// Speaker/Buzzer pin
#define SPEAKER_PIN 13

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void setup() {
  Serial.begin(115200);
  Serial.println("Smart Bus Stop - Initializing...");
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  
  // Connect to WiFi
  setup_wifi();
  
  // Initialize camera
  setup_camera();
  
  // Setup MQTT
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqtt_callback);
  
  Serial.println("Initialization complete");
}

void loop() {
  // Reconnect MQTT if disconnected
  if (!mqtt_client.connected()) {
    reconnect_mqtt();
  }
  mqtt_client.loop();
  
  // Read ultrasonic sensor
  float distance = read_ultrasonic();
  
  // Send sensor data via MQTT
  send_sensor_data(distance);
  
  // Capture and upload image periodically
  static unsigned long lastCapture = 0;
  if (millis() - lastCapture > 60000) { // Every 60 seconds
    capture_and_upload_image();
    lastCapture = millis();
  }
  
  delay(5000); // 5 second delay between readings
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Image quality settings
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  Serial.println("Camera initialized");
}

float read_ultrasonic() {
  // Clear trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo
  long duration = pulseIn(ECHO_PIN, HIGH);
  
  // Calculate distance in cm
  float distance = duration * 0.034 / 2;
  
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  return distance;
}

void send_sensor_data(float distance) {
  // Create JSON payload with deviceId (not device_id) for Telegraf
  String payload = "{";
  payload += "\"deviceId\":\"" + String(device_id) + "\",";
  payload += "\"location\":\"" + String(location) + "\",";
  payload += "\"distance\":" + String(distance) + ",";
  payload += "\"timestamp\":" + String(millis());
  payload += "}";
  
  // Publish to MQTT
  mqtt_client.publish(topic_sensors, payload.c_str());
  Serial.println("Sensor data published");
}

void capture_and_upload_image() {
  Serial.println("Capturing image...");
  
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  // Upload to Flask server
  HTTPClient http;
  String upload_url = String(flask_server) + "/api/upload";
  
  http.begin(upload_url);
  http.addHeader("Content-Type", "multipart/form-data; boundary=SmartStop");
  
  String body = "--SmartStop\r\n";
  body += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";
  
  // Send HTTP request
  http.POST(body);
  
  // Clean up
  esp_camera_fb_return(fb);
  http.end();
  
  Serial.println("Image uploaded");
  
  // Notify via MQTT with deviceId (not device_id) for Telegraf
  String mqtt_payload = "{";
  mqtt_payload += "\"deviceId\":\"" + String(device_id) + "\",";
  mqtt_payload += "\"location\":\"" + String(location) + "\",";
  mqtt_payload += "\"event_type\":\"capture\",";
  mqtt_payload += "\"image_count\":1";
  mqtt_payload += "}";
  
  mqtt_client.publish(topic_camera, mqtt_payload.c_str());
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Handle commands (e.g., play sound, take picture)
  if (message == "BEEP") {
    play_sound();
  } else if (message == "CAPTURE") {
    capture_and_upload_image();
  }
}

void reconnect_mqtt() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqtt_client.connect(device_id)) {
      Serial.println("connected");
      // Subscribe to command topic (using nus-smartstop prefix)
      mqtt_client.subscribe(topic_command);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void play_sound() {
  // Simple beep using PWM
  tone(SPEAKER_PIN, 1000, 500); // 1kHz for 500ms
  Serial.println("Playing sound");
}
