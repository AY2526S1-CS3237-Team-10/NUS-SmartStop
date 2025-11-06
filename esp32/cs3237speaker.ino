#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"

// MAX98357A pins
#define I2S_BCLK  26
#define I2S_LRCK  25
#define I2S_DATA  22

// SD pins (HSPI)
#define SD_CS     5
#define SD_SCK    18
#define SD_MISO   19
#define SD_MOSI   23

AudioFileSourceSD *fileSource;
AudioOutputI2S    *out;
AudioGeneratorMP3 *mp3;

// ======== SELECT WHICH AUDIO TO PLAY ========
// Change this value to 1, 2, 3, or 4
int audioSelect = 3;  
// ============================================

void setup() {
  Serial.begin(115200);

  // SD on HSPI
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 25000000)) {
    Serial.println("SD init failed");
    while (true) delay(10);
  }

  // Choose file path based on variable
  const char* filePath;
  switch (audioSelect) {
    case 1: filePath = "/MP3/bus3min.mp3"; break;
    case 2: filePath = "/MP3/bus1min.mp3"; break;
    case 3: filePath = "/MP3/fullcapacity.mp3"; break;
    case 4: filePath = "/MP3/busbreakdown.mp3"; break;
    default: filePath = "/MP3/bus3min.mp3"; break;  // fallback
  }

  Serial.printf("Playing file: %s\n", filePath);

  // Audio
  fileSource = new AudioFileSourceSD(filePath);
  out        = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DATA);
  mp3        = new AudioGeneratorMP3();

  if (!mp3->begin(fileSource, out)) {
    Serial.println("MP3 begin failed");
    while (true) delay(10);
  }
}

void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    delay(1000);
  }
}
