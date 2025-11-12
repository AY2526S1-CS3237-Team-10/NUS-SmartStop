#include "Arduino.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ... [Keep your same Pin Definitions here] ...
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

int pictureNumber = 0;

// --- NEW FUNCTION: Read/Write picture number to SD ---
void getPictureNumber() {
  fs::FS &fs = SD_MMC;
  File file = fs.open("/picture_count.txt", FILE_READ);
  if (!file) {
    Serial.println("No picture_count.txt found, starting from 0.");
    pictureNumber = 0;
  } else {
    String s = file.readStringUntil('\n');
    pictureNumber = s.toInt();
    Serial.printf("Found existing picture count: %d\n", pictureNumber);
    file.close();
  }
}

void savePictureNumber() {
  fs::FS &fs = SD_MMC;
  File file = fs.open("/picture_count.txt", FILE_WRITE);
  if (file) {
    file.print(pictureNumber);
    file.close();
  }
}
// ----------------------------------------------------


void initCamera() {
    // ... [Keep your EXACT same camera init code here] ...
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // --- IMAGE QUALITY SETTINGS ---
  // FRAMESIZE_UXGA (1600x1200) is okay for SD card saving (it has enough buffer)
  // but FRAMESIZE_SVGA (800x600) is faster to write.
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA; //
    config.jpeg_quality = 10; // 0-63, lower is higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);   // 1 = Flip vertically (upside down)
    s->set_exposure_ctrl(s, 1);
  }
}

void initSDCard() {
    // ... [Keep your EXACT same SD card init code here] ...
      // Start SD card using 1-bit mode (saves pins if needed, but slightly slower)
  // or just SD_MMC.begin() for standard 4-bit mode.
  // 1-bit mode is often more stable on cheaper boards:
  if(!SD_MMC.begin("/sdcard", true)){ 
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
  Serial.println("SD Card initialized.");
}

void takeSavePhoto() {
  camera_fb_t * fb = NULL;
  
  // 1. Take Picture
  Serial.println("Taking picture...");
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // 2. Generate File Path
  pictureNumber++; // Increment first
  String path = "/image" + String(pictureNumber) + ".jpg";
  Serial.printf("Picture file name: %s\n", path.c_str());

  // 3. Save to SD Card
  fs::FS &fs = SD_MMC;
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len); 
    Serial.printf("Saved: %s, Size: %u bytes\n", path.c_str(), fb->len);
    
    // --- NEW: Save the new number immediately ---
    savePictureNumber(); 
  }
  file.close();
  esp_camera_fb_return(fb);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32-CAM SD Card Capture ---");

  initCamera();
  initSDCard();
  
  // --- NEW: Read the last number on boot ---
  getPictureNumber();
}

void loop() {
  takeSavePhoto();
  Serial.println("Waiting 5 seconds...");
  delay(5000);
}