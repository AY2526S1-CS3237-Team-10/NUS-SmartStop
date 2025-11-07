/*
 * NUS SmartStop - Integrated Sensors System
 * 
 * This file integrates:
 * - Speaker (MAX98357A) with MP3 playback
 * - Microphone (INMP441) with voice detection
 * - IR Sensors for people counting
 * - Ultrasonic Sensors for occupancy detection
 * - WiFi and MQTT connectivity
 * 
 * Pin Configuration (conflict-free):
 * - Speaker (MAX98357A I2S):
 *   - I2S_BCLK: GPIO 26
 *   - I2S_LRCK: GPIO 25
 *   - I2S_DATA: GPIO 22
 * - Speaker SD Card (HSPI):
 *   - SD_CS: GPIO 5
 *   - SD_SCK: GPIO 18
 *   - SD_MISO: GPIO 19
 *   - SD_MOSI: GPIO 23
 * - Microphone (INMP441 I2S):
 *   - I2S_WS: GPIO 15
 *   - I2S_SD: GPIO 32
 *   - I2S_SCK: GPIO 14
 * - IR Sensors:
 *   - IR_TX_A: GPIO 33 (changed from 26 to avoid conflict)
 *   - IR_TX_B: GPIO 27
 *   - IR_RX_A: GPIO 34
 *   - IR_RX_B: GPIO 35
 * - Ultrasonic Sensors:
 *   - TRIG_0: GPIO 2  (changed from 5)
 *   - ECHO_0: GPIO 4  (changed from 18)
 *   - TRIG_1: GPIO 16 (changed from 22)
 *   - ECHO_1: GPIO 17 (changed from 23)
 *   - TRIG_2: GPIO 12 (changed from 13)
 *   - ECHO_2: GPIO 13 (changed from 14)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"
#include "ESP32MQTTClient.h"

// ==================== WiFi Configuration ====================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ==================== MQTT Configuration ====================
const char* MQTT_HOST = "157.230.250.226";
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID = "esp32-integrated-01";

// MQTT Topics
const char* TOPIC_VOICE = "nus-smartstop/voice";
const char* TOPIC_IR_SENSOR = "nus-smartstop/ir-sensor/data";
const char* TOPIC_ULTRASONIC = "nus-smartstop/ultrasonic/data";
const char* TOPIC_COMMAND = "nus-smartstop/command/esp32-integrated-01";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP32MQTTClient mqttClientAlt;

// ==================== Speaker (MAX98357A) Configuration ====================
#define SPEAKER_I2S_BCLK  26
#define SPEAKER_I2S_LRCK  25
#define SPEAKER_I2S_DATA  22
#define SD_CS     5
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

AudioFileSourceSD *fileSource = nullptr;
AudioOutputI2S *audioOut = nullptr;
AudioGeneratorMP3 *mp3 = nullptr;
bool speakerEnabled = false;
int audioSelect = 3;  // Default audio: 1=bus3min, 2=bus1min, 3=fullcapacity, 4=busbreakdown

// ==================== Microphone (INMP441) Configuration ====================
#define MIC_I2S_WS   15
#define MIC_I2S_SD   32
#define MIC_I2S_SCK  14
#define MIC_I2S_PORT I2S_NUM_0
#define N_SAMPLES   1024
#define SAMPLE_RATE 16000.0

int32_t micRawBuf[N_SAMPLES];
double micVReal[N_SAMPLES], micVImag[N_SAMPLES];
ArduinoFFT FFT(micVReal, micVImag, N_SAMPLES, SAMPLE_RATE);

// Voice detection parameters
const double QUIET_RMS_MIN = 80000.0;
const float RATIO_ON = 0.60f;
const float RATIO_OFF = 0.50f;
const float VOICING_ON_DB = 2.5f;
const float VOICING_OFF_DB = 1.5f;
const float SFM_ON_MAX = 0.65f;
const float SFM_OFF_MIN = 0.80f;
const uint8_t K_ON = 2;
const uint8_t K_OFF = 4;

bool voiceActive = false;
uint8_t upCnt = 0, downCnt = 0;
float ratioEMA = 0.0f;

// ==================== IR Sensor Configuration ====================
#define IR_TX_A 33  // Changed from 26
#define IR_TX_B 27
#define IR_RX_A 34
#define IR_RX_B 35

volatile float peopleCount = 0;
int lastIrA = HIGH, lastIrB = HIGH;
unsigned long lastTriggerTime = 0;
const unsigned long sequenceWindow = 1000;

// IR sensor state machine for non-blocking detection
enum IrState { IR_IDLE, IR_WAIT_A_FOR_B, IR_WAIT_B_FOR_A };
IrState irState = IR_IDLE;
unsigned long irStateStartTime = 0;
const unsigned long IR_DEBOUNCE_DELAY = 500;

// ==================== Ultrasonic Sensor Configuration ====================
const int SENSOR_COUNT = 3;
const int trigPins[SENSOR_COUNT] = {2, 16, 12};   // Changed pins
const int echoPins[SENSOR_COUNT] = {4, 17, 13};   // Changed pins
const String SECTION_NAMES[SENSOR_COUNT] = {"LEFT", "CENTER", "RIGHT"};
const int MIN_DISTANCE = 200;  // 20cm
const int MAX_DISTANCE = 400;  // 400cm
const int READ_INTERVAL = 2000;

int sensorDistances[SENSOR_COUNT];
bool sensorStates[SENSOR_COUNT];
float density = 0.0;

// ==================== Timing Variables ====================
unsigned long lastMicPublish = 0;
unsigned long lastUltrasonicRead = 0;
unsigned long lastNetworkCheck = 0;
unsigned long lastPrintTime = 0;
const unsigned long MIC_PUB_INTERVAL = 1000;
const unsigned long ULTRASONIC_INTERVAL = 2000;
const unsigned long NETWORK_CHECK_INTERVAL = 5000;
const unsigned long PRINT_INTERVAL = 250;

// ==================== Function Declarations ====================
void setupWifi();
void setupMQTT();
void setupSpeaker();
void setupMicrophone();
void setupIRSensors();
void setupUltrasonicSensors();
void ensureWifi();
void ensureMqtt();
void loopSpeaker();
void loopMicrophone();
void loopIRSensors();
void loopUltrasonicSensors();
void publishVoiceData();
void publishIRData();
void publishUltrasonicData();
int readUltrasonicSensor(int index);
void calculateDensity();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void printNetStatus();

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== NUS SmartStop - Integrated Sensors ===");
  Serial.println("Initializing all systems...\n");

  // Initialize WiFi
  setupWifi();
  
  // Initialize MQTT
  setupMQTT();
  
  // Initialize Speaker
  setupSpeaker();
  
  // Initialize Microphone
  setupMicrophone();
  
  // Initialize IR Sensors
  setupIRSensors();
  
  // Initialize Ultrasonic Sensors
  setupUltrasonicSensors();
  
  Serial.println("\n=== Initialization Complete ===");
  Serial.println("System ready!");
}

// ==================== Main Loop ====================
void loop() {
  unsigned long now = millis();
  
  // Maintain MQTT connection
  mqttClient.loop();
  
  // Network health check
  if (now - lastNetworkCheck >= NETWORK_CHECK_INTERVAL) {
    lastNetworkCheck = now;
    if (WiFi.status() != WL_CONNECTED) ensureWifi();
    if (!mqttClient.connected()) ensureMqtt();
    printNetStatus();
  }
  
  // Speaker loop (if playing)
  loopSpeaker();
  
  // Microphone loop (voice detection)
  loopMicrophone();
  
  // IR Sensors loop (people counting)
  loopIRSensors();
  
  // Ultrasonic Sensors loop (occupancy detection)
  if (now - lastUltrasonicRead >= ULTRASONIC_INTERVAL) {
    lastUltrasonicRead = now;
    loopUltrasonicSensors();
  }
}

// ==================== WiFi Functions ====================
void setupWifi() {
  Serial.println("Setting up WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
  }
}

// ==================== MQTT Functions ====================
void setupMQTT() {
  Serial.println("Setting up MQTT...");
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  ensureMqtt();
}

void ensureMqtt() {
  if (mqttClient.connected()) return;
  
  Serial.print("Connecting to MQTT...");
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    if (mqttClient.connect(DEVICE_ID)) {
      Serial.println(" connected!");
      mqttClient.subscribe(TOPIC_COMMAND);
    } else {
      Serial.print(".");
      delay(1000);
      attempts++;
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Handle commands
  if (message == "PLAY_AUDIO_1") {
    audioSelect = 1;
    playAudio(audioSelect);
  } else if (message == "PLAY_AUDIO_2") {
    audioSelect = 2;
    playAudio(audioSelect);
  } else if (message == "PLAY_AUDIO_3") {
    audioSelect = 3;
    playAudio(audioSelect);
  } else if (message == "PLAY_AUDIO_4") {
    audioSelect = 4;
    playAudio(audioSelect);
  } else if (message == "RESET_COUNT") {
    peopleCount = 0;
    Serial.println("People count reset to 0");
  }
}

void printNetStatus() {
  Serial.print("WiFi: ");
  Serial.print((WiFi.status() == WL_CONNECTED) ? "OK " : "DISCONNECTED ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" RSSI=");
    Serial.print(WiFi.RSSI());
  }
  Serial.print("  |  MQTT: ");
  Serial.println(mqttClient.connected() ? "OK" : "DISCONNECTED");
}

// ==================== Speaker Functions ====================
void setupSpeaker() {
  Serial.println("Setting up Speaker (MAX98357A)...");
  
  // Initialize SPI for SD card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS, SPI, 25000000)) {
    Serial.println("SD card initialization failed - speaker disabled");
    speakerEnabled = false;
    return;
  }
  
  Serial.println("SD card initialized");
  speakerEnabled = true;
  
  // Audio will be initialized when needed to play
}

void loopSpeaker() {
  if (!speakerEnabled) return;
  
  if (mp3 != nullptr && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("Audio playback finished");
    }
  }
}

void playAudio(int audioNum) {
  if (!speakerEnabled) {
    Serial.println("Speaker not available");
    return;
  }
  
  // Stop current playback if any
  if (mp3 != nullptr && mp3->isRunning()) {
    mp3->stop();
  }
  
  // Clean up previous instances
  if (mp3 != nullptr) delete mp3;
  if (audioOut != nullptr) delete audioOut;
  if (fileSource != nullptr) delete fileSource;
  
  // Select audio file
  const char* filePath;
  switch (audioNum) {
    case 1: filePath = "/MP3/bus3min.mp3"; break;
    case 2: filePath = "/MP3/bus1min.mp3"; break;
    case 3: filePath = "/MP3/fullcapacity.mp3"; break;
    case 4: filePath = "/MP3/busbreakdown.mp3"; break;
    default: filePath = "/MP3/bus3min.mp3"; break;
  }
  
  Serial.printf("Playing audio: %s\n", filePath);
  
  // Initialize audio playback
  fileSource = new AudioFileSourceSD(filePath);
  audioOut = new AudioOutputI2S();
  audioOut->SetPinout(SPEAKER_I2S_BCLK, SPEAKER_I2S_LRCK, SPEAKER_I2S_DATA);
  mp3 = new AudioGeneratorMP3();
  
  if (!mp3->begin(fileSource, audioOut)) {
    Serial.println("Failed to start audio playback");
  }
}

// ==================== Microphone Functions ====================
void setupMicrophone() {
  Serial.println("Setting up Microphone (INMP441)...");
  
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = (int)SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pins = {
    .bck_io_num = MIC_I2S_SCK,
    .ws_io_num = MIC_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_I2S_SD
  };
  
  i2s_driver_install(MIC_I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(MIC_I2S_PORT, &pins);
  i2s_start(MIC_I2S_PORT);
  
  Serial.println("Microphone initialized");
}

void loopMicrophone() {
  // Read audio samples
  size_t bytesRead = 0;
  i2s_read(MIC_I2S_PORT, (void*)micRawBuf, sizeof(micRawBuf), &bytesRead, portMAX_DELAY);
  int nRead = bytesRead / sizeof(int32_t);
  if (nRead <= 0) return;
  
  // Apply high-pass filter
  static double hp_y = 0.0, hp_x1 = 0.0;
  const double alphaHP = 0.997;
  for (int i = 0; i < nRead; i++) {
    int32_t s = micRawBuf[i] >> 8;
    double x = (double)s;
    hp_y = alphaHP * (hp_y + x - hp_x1);
    hp_x1 = x;
    micVReal[i] = hp_y;
    micVImag[i] = 0.0;
  }
  
  // Perform FFT
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
  
  // Calculate energy
  double totalE = 0.0, voiceE = 0.0;
  for (int i = 1; i < N_SAMPLES/2; i++) {
    double f = (i * SAMPLE_RATE) / N_SAMPLES;
    double p = micVReal[i] * micVReal[i];
    totalE += p;
    if (f >= 120 && f <= 3400) voiceE += p;
  }
  
  double rms = sqrt(totalE / (N_SAMPLES/2));
  if (rms < QUIET_RMS_MIN) {
    if (voiceActive) {
      voiceActive = false;
      upCnt = 0;
    }
    ratioEMA *= 0.9f;
    return;
  }
  
  double ratio = (totalE > 0.0) ? (voiceE / totalE) : 0.0;
  ratioEMA = 0.25f * ratio + 0.75f * ratioEMA;
  
  // Calculate voicing and spectral flatness measure
  double lowE = 0, midE = 0;
  for (int i = 1; i < N_SAMPLES/2; i++) {
    double f = (i * SAMPLE_RATE) / N_SAMPLES;
    double p = micVReal[i] * micVReal[i];
    if (f >= 80 && f <= 300) lowE += p;
    if (f >= 500 && f <= 1500) midE += p;
  }
  
  const double eps = 1e-9;
  double voicingDb = 10.0 * log10((lowE + eps) / (midE + eps));
  voicingDb = max(-20.0, min(20.0, voicingDb));
  
  double sumLog = 0.0, sumLin = 0.0;
  int nb = 0;
  for (int i = 1; i < N_SAMPLES/2; i++) {
    double f = (i * SAMPLE_RATE) / N_SAMPLES;
    if (f < 200 || f > 3400) continue;
    double p = micVReal[i] * micVReal[i] + eps;
    sumLog += log(p);
    sumLin += p;
    nb++;
  }
  
  double sfm = 1.0;
  if (nb > 0) {
    double geom = exp(sumLog / nb);
    double arith = sumLin / nb;
    sfm = (arith > 0) ? (geom / arith) : 1.0;
  }
  
  // Debounce logic for voice activity detection
  int onVotes = 0;
  onVotes += (ratio >= RATIO_ON);
  onVotes += (voicingDb >= VOICING_ON_DB);
  onVotes += (sfm <= SFM_ON_MAX);
  bool onGate = (onVotes >= 2) && (rms >= QUIET_RMS_MIN);
  bool offGate = (ratio <= RATIO_OFF) || (voicingDb <= VOICING_OFF_DB) || (sfm >= SFM_OFF_MIN) || (rms < QUIET_RMS_MIN);
  
  if (!voiceActive) {
    if (onGate) {
      if (++upCnt >= K_ON) {
        voiceActive = true;
        downCnt = 0;
      }
    } else {
      upCnt = 0;
    }
  } else {
    if (offGate) {
      if (++downCnt >= K_OFF) {
        voiceActive = false;
        upCnt = 0;
      }
    } else {
      downCnt = 0;
    }
  }
  
  // Print status periodically
  unsigned long now = millis();
  if (now - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = now;
    Serial.printf("MIC: ratio=%.2f voicing=%.2f dB sfm=%.2f rms=%.0f state=%s\n",
                  ratio, voicingDb, sfm, rms, voiceActive ? "YES" : "no");
  }
  
  // Publish voice data
  if (now - lastMicPublish >= MIC_PUB_INTERVAL) {
    lastMicPublish = now;
    publishVoiceData();
  }
}

void publishVoiceData() {
  if (!mqttClient.connected()) return;
  
  char msg[96];
  snprintf(msg, sizeof(msg),
           "{\"deviceId\":\"%s\",\"voice\":%s}",
           DEVICE_ID, voiceActive ? "true" : "false");
  mqttClient.publish(TOPIC_VOICE, msg, false);
}

// ==================== IR Sensor Functions ====================
void setupIRSensors() {
  Serial.println("Setting up IR Sensors...");
  
  pinMode(IR_TX_A, OUTPUT);
  pinMode(IR_TX_B, OUTPUT);
  pinMode(IR_RX_A, INPUT);
  pinMode(IR_RX_B, INPUT);
  
  digitalWrite(IR_TX_A, HIGH);
  digitalWrite(IR_TX_B, HIGH);
  
  Serial.println("IR sensors initialized");
}

void loopIRSensors() {
  int currA = digitalRead(IR_RX_A);
  int currB = digitalRead(IR_RX_B);
  unsigned long now = millis();
  
  // Non-blocking state machine for IR sensor detection
  switch (irState) {
    case IR_IDLE:
      // Entry detection: A triggered (person approaching from A side)
      if (lastIrA == HIGH && currA == LOW) {
        irState = IR_WAIT_A_FOR_B;
        irStateStartTime = now;
      }
      // Exit detection: B triggered (person approaching from B side)
      else if (lastIrB == HIGH && currB == LOW) {
        irState = IR_WAIT_B_FOR_A;
        irStateStartTime = now;
      }
      break;
      
    case IR_WAIT_A_FOR_B:
      // Waiting for B to trigger after A (entry sequence)
      if (digitalRead(IR_RX_B) == LOW) {
        // Entry detected
        peopleCount++;
        Serial.printf("IR: Person entered. Count: %.0f\n", peopleCount);
        publishIRData();
        irState = IR_IDLE;
        // Small delay to prevent immediate re-triggering (non-blocking via state timing)
        irStateStartTime = now;
      } else if (now - irStateStartTime > sequenceWindow) {
        // Timeout - no B trigger, reset to idle
        irState = IR_IDLE;
      }
      break;
      
    case IR_WAIT_B_FOR_A:
      // Waiting for A to trigger after B (exit sequence)
      if (digitalRead(IR_RX_A) == LOW) {
        // Exit detected
        peopleCount--;
        if (peopleCount < 0) peopleCount = 0;
        Serial.printf("IR: Person exited. Count: %.0f\n", peopleCount);
        publishIRData();
        irState = IR_IDLE;
        // Small delay to prevent immediate re-triggering (non-blocking via state timing)
        irStateStartTime = now;
      } else if (now - irStateStartTime > sequenceWindow) {
        // Timeout - no A trigger, reset to idle
        irState = IR_IDLE;
      }
      break;
  }
  
  // Debounce: prevent re-triggering too quickly after detection
  if (irState == IR_IDLE && now - irStateStartTime < IR_DEBOUNCE_DELAY) {
    // Still in debounce period, don't start new detection
    lastIrA = currA;
    lastIrB = currB;
    return;
  }
  
  lastIrA = currA;
  lastIrB = currB;
}

void publishIRData() {
  if (!mqttClient.connected()) return;
  
  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"people_count\": %.1f, \"location\": \"busstop\"}",
           peopleCount);
  
  mqttClient.publish(TOPIC_IR_SENSOR, payload);
}

// ==================== Ultrasonic Sensor Functions ====================
void setupUltrasonicSensors() {
  Serial.println("Setting up Ultrasonic Sensors...");
  
  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    digitalWrite(trigPins[i], LOW);
  }
  
  Serial.println("Ultrasonic sensors initialized");
}

void loopUltrasonicSensors() {
  // Read all sensors
  for (int i = 0; i < SENSOR_COUNT; i++) {
    sensorDistances[i] = readUltrasonicSensor(i);
    sensorStates[i] = (sensorDistances[i] >= MIN_DISTANCE && sensorDistances[i] <= MAX_DISTANCE);
  }
  
  // Calculate density
  calculateDensity();
  
  // Print readings
  Serial.println("\n--- Ultrasonic Sensor Readings ---");
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
  Serial.printf("Bus stop density: %.2f\n", density);
  
  // Publish data
  publishUltrasonicData();
}

int readUltrasonicSensor(int index) {
  digitalWrite(trigPins[index], LOW);
  delayMicroseconds(2);
  digitalWrite(trigPins[index], HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPins[index], LOW);
  
  long duration = pulseIn(echoPins[index], HIGH, 30000);
  if (duration == 0) return 0;
  return duration * 0.034 / 2;
}

void calculateDensity() {
  int triggeredCount = 0;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (sensorStates[i]) triggeredCount++;
  }
  density = triggeredCount / (float)SENSOR_COUNT;
}

void publishUltrasonicData() {
  if (!mqttClient.connected()) return;
  
  char payload[400];
  snprintf(payload, sizeof(payload),
           "{\"sensors\": {"
           "\"LEFT\": {\"distance\": %d, \"occupied\": %d}, "
           "\"CENTER\": {\"distance\": %d, \"occupied\": %d}, "
           "\"RIGHT\": {\"distance\": %d, \"occupied\": %d}}, "
           "\"density\": %.2f}",
           sensorDistances[0], sensorStates[0] ? 1 : 0,
           sensorDistances[1], sensorStates[1] ? 1 : 0,
           sensorDistances[2], sensorStates[2] ? 1 : 0,
           density);
  
  mqttClient.publish(TOPIC_ULTRASONIC, payload);
}
