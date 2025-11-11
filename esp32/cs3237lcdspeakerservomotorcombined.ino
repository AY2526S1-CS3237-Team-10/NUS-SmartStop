#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <ESP32Servo.h>

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

// State & Data Variables (From Code 1)
int peopleCount = 0;
float distance = 0.0;
bool voiceDetected = false;

// System State
bool isFullCapacity = false;
int normalPlaylistIndex = 0; 

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
  // 1. If MP3 is playing, keep it running
  if (mp3->isRunning()) {
    if (!mp3->loop()) { 
      mp3->stop(); // Song finished
    }
  } 
  // 2. If MP3 stopped/idle, decide what to play next
  else {
    if (isFullCapacity) {
      playFile("/MP3/fullcapacity.mp3");
    } else {
      const char* normalFiles[] = {"/MP3/bus3min.mp3", "/MP3/bus1min.mp3", "/MP3/busbreakdown.mp3"};
      playFile(normalFiles[normalPlaylistIndex]);
      normalPlaylistIndex++;
      if (normalPlaylistIndex > 2) normalPlaylistIndex = 0; 
    }
  }
}

// ============================
// ====== SETUP =============
// ============================
void setup() {
  Serial.begin(115200);

  // 1. Init LCD (Code 1 Syntax)
  Wire.begin(); // Defaults to SDA=21, SCL=22 on ESP32
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartStop Init");

  // 2. Init SD Card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 25000000)) {
    Serial.println("SD Init Failed!");
    lcd.setCursor(0, 1); 
    lcd.print("SD Fail");
    while (true) delay(10);
  }

  // 3. Init Audio
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  mp3 = new AudioGeneratorMP3();

  // 4. Init Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0); // Start closed

  delay(1000);
  lcd.clear();
}

// ============================
// ====== LOOP ================
// ============================
void loop() {
  // --- IMPORTANT: Audio loop must run fast and often ---
  handleAudioLogic();

  // --- NON-BLOCKING UPDATE (Every 2 Seconds) ---
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();

    // 1. GENERATE DATA (From Code 1)
    // We simulate sensors here. 
    peopleCount = random(0, 12);        // Mock people count (0 to 12)
    distance = random(20, 200) / 10.0;  // Mock distance
    voiceDetected = random(0, 2);       // Mock voice

    // 2. DETERMINE STATE BASED ON DATA
    // Let's say if people > 8, the bus stop is "Full"
    bool newState = (peopleCount > 8);

    // 3. HANDLE STATE CHANGE (Servo Logic)
    if (newState != isFullCapacity) {
        isFullCapacity = newState;
        if (isFullCapacity) {
            Serial.println("State: FULL -> Servo 90");
            myServo.write(90); 
            // Force immediate warning
            playFile("/MP3/fullcapacity.mp3"); 
        } else {
            Serial.println("State: NORMAL -> Servo 0");
            myServo.write(0);
            // Stop warning immediately, let handleAudioLogic pick next normal song
            if (mp3->isRunning()) mp3->stop(); 
        }
    }

    // 4. UPDATE LCD (Using Code 1 Syntax)
    lcd.clear();
    
    // Line 1: Crowd Info
    lcd.setCursor(0, 0);
    lcd.print("Ppl:");
    lcd.print(peopleCount);
    lcd.print(" Dist:");
    lcd.print(distance, 1); 

    // Line 2: Noise & Level
    lcd.setCursor(0, 1);
    if (voiceDetected) {
      lcd.print("Noise:Y ");
    } else {
      lcd.print("Noise:N ");
    }

    // Simple status indicator
    if (isFullCapacity) {
        lcd.print(" [FULL]");
    } else {
        lcd.print(" [OK]  ");
    }

    // Debug Serial
    Serial.printf("Ppl: %d | Dist: %.1f | Full: %d\n", peopleCount, distance, isFullCapacity);
  }
}