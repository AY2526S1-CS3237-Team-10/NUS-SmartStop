#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Audio Libraries (ESP8266Audio)
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"

// ============================
// ====== PIN DEFINITIONS =====
// ============================
// Speaker (I2S)
#define I2S_BCLK      26
#define I2S_LRCK      25
#define I2S_DOUT      27  // GPIO 27 (Avoids conflict with standard I2C pins 21/22)

// SD Card (SPI)
#define SD_CS         5
#define SD_SCK        18
#define SD_MISO       19
#define SD_MOSI       23

// Servo
#define SERVO_PIN     13

// WiFi Credentials
const char* ssid = ""; //Input WIFI or Hotspot SSID
const char* password = ""; //Input WIFI or Hotspot Password

// Server Configuration
const char* serverURL = ""; //Input Server URL with predict Endpoint

// ============================
// ====== OBJECTS & VARS ======
// ============================
// LCD: I2C address 0x27, 16x2 display
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

AudioFileSourceSD *fileSource = NULL;
AudioOutputI2S    *out = NULL;
AudioGeneratorMP3 *mp3 = NULL;

// Timing Variables
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 2000; // Update LCD & Data every 2 seconds

unsigned long lastPredictionRequest = 0;
const unsigned long PREDICTION_INTERVAL = 30000; // Request prediction every 30 seconds

// State & Data Variables
int peopleCount = 0;
float distance = 0.0;
bool voiceDetected = false;
float predictedCapacity = 0.0;  // New: Store predicted capacity from server

// System State
bool isFullCapacity = false;
int normalPlaylistIndex = 0;
bool audioAvailable = false;  // Track if audio is available
bool predictionInProgress = false;  // Track if prediction request is ongoing
unsigned long predictionStartTime = 0;
HTTPClient* asyncHttp = NULL;  // For non-blocking HTTP 

// Audio pause/resume state
bool audioWasPausedForPrediction = false; 

// ============================
// ====== AUDIO HELPER ========
// ============================
void playFile(const char* filename) {
  if (mp3->isRunning()) mp3->stop();
  
  if (fileSource != NULL) {
    delete fileSource;
    fileSource = NULL;
  }

  Serial.printf("Playing: %s\n", filename);
  fileSource = new AudioFileSourceSD(filename);
  mp3->begin(fileSource, out);
}

void handleAudioLogic() {
  // Skip if audio not available
  if (!audioAvailable) return;
  
  // 1. If MP3 is playing, keep it running
  if (mp3->isRunning()) {
    if (!mp3->loop()) { 
      mp3->stop(); // Song finished
    }
  } 
  // 2. If MP3 stopped/idle, decide what to play next
  else {
    if (isFullCapacity) {
      // Only play warning once every 10 seconds to avoid infinite loop
      static unsigned long lastWarning = 0;
      if (millis() - lastWarning >= 5000) {  // 10 second interval
        playFile("/MP3/fullcapacity.mp3");
        lastWarning = millis();
      }
    } else {
      // Normal playlist - cycle through songs
      const char* normalFiles[] = {"/MP3/bus3min.mp3", "/MP3/bus1min.mp3", "/MP3/busbreakdown.mp3"};
      playFile(normalFiles[normalPlaylistIndex]);
      normalPlaylistIndex++;
      if (normalPlaylistIndex > 2) normalPlaylistIndex = 0; 
    }
  }
}

// ============================
// ====== PREDICTION API ======
// ============================
void startPredictionRequest() {
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping prediction");
    return;
  }
  
  if (predictionInProgress) {
    Serial.println("Prediction already in progress, skipping");
    return;
  }
  
  Serial.println("Starting capacity prediction request...");
  Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
  
  // Pause audio if playing
  if (audioAvailable && mp3->isRunning()) {
    Serial.println("Pausing audio for prediction...");
    mp3->stop();
    audioWasPausedForPrediction = true;
  }
  
  // Create HTTP client
  asyncHttp = new HTTPClient();
  asyncHttp->setTimeout(10000);  // 10 seconds (cache should respond in <1s)
  
  // Make request
  if (!asyncHttp->begin(serverURL)) {
    Serial.println("HTTP client init failed");
    delete asyncHttp;
    asyncHttp = NULL;
    return;
  }
  
  asyncHttp->setReuse(false);  // Disable connection reuse to avoid keepalive issues
  
  predictionInProgress = true;
  predictionStartTime = millis();
  
  // Start the GET request (non-blocking via streaming)
  int httpCode = asyncHttp->GET();
  
  if (httpCode != 200) {
    Serial.printf("HTTP Error: %d\n", httpCode);
    
    if (httpCode == -1) {
      Serial.println("  -> Connection failed");
    } else if (httpCode == -11) {
      Serial.println("  -> Timeout (server took too long)");
    }
    
    asyncHttp->end();
    delete asyncHttp;
    asyncHttp = NULL;
    predictionInProgress = false;
    
    // Resume audio if it was paused
    if (audioWasPausedForPrediction) {
      audioWasPausedForPrediction = false;
      Serial.println("Prediction failed - audio will resume in next loop");
    }
    
    return;
  }
  
  // Get response
  String response = asyncHttp->getString();
  asyncHttp->end();
  delete asyncHttp;
  asyncHttp = NULL;
  predictionInProgress = false;
  
  unsigned long duration = millis() - predictionStartTime;
  Serial.printf("Response received in %lu ms\n", duration);
  
  // Parse JSON
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("JSON Parse Error: ");
    Serial.println(error.c_str());
    
    // Resume audio if it was paused
    if (audioWasPausedForPrediction) {
      audioWasPausedForPrediction = false;
      Serial.println("JSON error - audio will resume in next loop");
    }
    
    return;
  }
  
  // Extract data
  bool success = doc["success"];
  float capacity = doc["capacity"];
  
  if (success && capacity >= 0) {
    predictedCapacity = capacity;
    Serial.printf("Predicted Capacity: %.1f people\n", predictedCapacity);
    
    // Update isFullCapacity based on prediction
    bool newState = (predictedCapacity > 15);
    
    if (newState != isFullCapacity) {
      isFullCapacity = newState;
      if (isFullCapacity) {
        Serial.println("State: FULL -> Servo 90");
        myServo.write(90); 
        if (audioAvailable) {
          // Stop current audio and play warning (don't resume old audio)
          if (mp3->isRunning()) mp3->stop();
          playFile("/MP3/fullcapacity.mp3");
          audioWasPausedForPrediction = false;  // Clear flag - playing warning now
        }
      } else {
        Serial.println("State: NORMAL -> Servo 0");
        myServo.write(0);
        if (audioAvailable && mp3->isRunning()) {
          mp3->stop();
        }
        // Let audio resume naturally in next handleAudioLogic() call
        if (audioWasPausedForPrediction) {
          audioWasPausedForPrediction = false;
          Serial.println("Resuming normal audio...");
        }
      }
    } else {
      // State didn't change - resume audio if it was paused
      if (audioWasPausedForPrediction) {
        audioWasPausedForPrediction = false;
        Serial.println("No state change - audio will resume in next loop");
      }
    }
  } else {
    Serial.println("Prediction failed or no data");
    predictedCapacity = -1.0;
    
    // Resume audio if it was paused
    if (audioWasPausedForPrediction) {
      audioWasPausedForPrediction = false;
      Serial.println("Prediction failed - audio will resume in next loop");
    }
  }
}

// ============================
// ====== SETUP =============
// ============================
void setup() {
  Serial.begin(115200);

  // 1. Init LCD
  Wire.begin(); // Defaults to SDA=21, SCL=22 on ESP32
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartStop Init");

  // 2. Connect to WiFi
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  WiFi.begin(ssid, password);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 1);
    lcd.print("WiFi OK         ");
  } else {
    Serial.println("\nWiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("WiFi Fail       ");
  }
  delay(1000);

  // 3. Init SD Card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 25000000)) {
    Serial.println("SD Init Failed! Audio disabled");
    lcd.setCursor(0, 1); 
    lcd.print("No Audio        ");
    audioAvailable = false;
    delay(2000);
  } else {
    Serial.println("SD Init OK");
    audioAvailable = true;
  }

  // 4. Init Audio (only if SD is available)
  if (audioAvailable) {
    out = new AudioOutputI2S();
    out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    mp3 = new AudioGeneratorMP3();
    Serial.println("Audio Init OK");
  }

  // 5. Init Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0); // Start closed

  delay(1000);
  lcd.clear();
}

// ============================
// ====== LOOP ================
// ============================
void loop() {
  handleAudioLogic();

  // --- PREDICTION REQUEST (Every 30 Seconds) ---
  // Only start if not already in progress
  if (!predictionInProgress && millis() - lastPredictionRequest >= PREDICTION_INTERVAL) {
    lastPredictionRequest = millis();
    startPredictionRequest();
  }

  // --- NON-BLOCKING UPDATE (Every 2 Seconds) ---
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();

    // 1. GENERATE DATA 
    // We simulate sensors here. 
    peopleCount = random(0, 12);        // Mock people count (0 to 12)
    distance = random(20, 200) / 10.0;  // Mock distance
    voiceDetected = random(0, 2);       // Mock voice

    // 2. UPDATE LCD 
    lcd.clear();
    
    // Line 1: Predicted Capacity
    lcd.setCursor(0, 0);
    if (predictedCapacity >= 0) {
      lcd.print("Cap:");
      lcd.print((int)predictedCapacity);
      lcd.print(" ");
    } else {
      lcd.print("Cap:-- ");
    }

    // Line 2: Predicted Status
    lcd.setCursor(0, 1);
    // Simple status indicator
    if (isFullCapacity) {
        lcd.print("[FULL]");
    } else {
        lcd.print("[OK]  ");
    }

    // Debug Serial
    Serial.printf("Ppl: %d | Dist: %.1f | Cap: %.1f | Full: %d\n", 
                  peopleCount, distance, predictedCapacity, isFullCapacity);
  }
}