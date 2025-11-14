#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ====== CAMERA MODEL ======
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <esp_wifi.h>
#include <esp_bt.h>

// ====== CONFIGURATION ======
const char* ssid = "Ken Phone";
const char* password = "Estri333";
const char* serverURL = "http://157.230.250.226:5000/upload";

#define CAPTURE_INTERVAL_MS 60000
#define SLEEP_DURATION_S 60

// ====== FUNCTION DECLARATIONS ======
void setupCamera();
void setupWiFi();
bool captureAndUploadPhoto();
bool uploadToServer(camera_fb_t *fb, int attemptNum);
void setupLedFlash(int pin);
void goToSleep();

// ====== SETUP ======
void setup() {
  // setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(1000); // Give some buffer time for serial to start
  // Serial.setDebugOutput(true);
  Serial.printf("CPU frequency: %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("XTAL frequency: %d MHz\n", getXtalFrequencyMhz());
  Serial.printf("APB frequency: %d MHz\n", getApbFrequency() / 1000000);
  Serial.println();
  Serial.println("=== ESP32-CAM Photo Upload (PlatformIO Version) ===");
  Serial.println("=== ESP32 Camera Photo Capture & Upload ===");
  Serial.println("CS3237 Group 10 - Image Gallery Integration");

  setupWiFi();
  setupCamera();

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  Serial.println("A: Setup complete!");
  Serial.println("B: Taking photo and uploading to Flask server...");

  bool success = captureAndUploadPhoto();
  if (success)
    Serial.println("✅ Photo uploaded successfully!");
  else
    Serial.println("❌ Failed to upload photo.");

  // Sleep logic
  Serial.println();
  Serial.println("=== Entering deep sleep mode ===");
  Serial.printf("Sleeping for %d seconds...\n", SLEEP_DURATION_S);

  delay(1000); // Give time for serial output to complete

  goToSleep();

  // esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_DURATION_S * 1000000ULL);
  // esp_deep_sleep_start();
}

void goToSleep() {
  Serial.println("\n========================================");
  Serial.println("Preparing for Deep Sleep...");
  
  // --- 1. SHUTDOWN CAMERA ---
  Serial.println("  - De-initializing camera...");
  esp_err_t cam_err = esp_camera_deinit();
  if (cam_err != ESP_OK) {
    Serial.printf("  - Camera deinit failed (0x%x)\n", cam_err);
  }
  
  // Assert powerdown pin for the camera sensor.
  // This is critical for stopping the sensor's power draw.
  Serial.println("  - Asserting camera power-down pin.");
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); // Assumes HIGH powers down the sensor

  // --- 2. SHUTDOWN RADIO (WiFi & Bluetooth) ---
  Serial.println("  - Shutting down WiFi...");
  if (WiFi.isConnected()) {
    WiFi.disconnect(true, true); // disconnect, disable autoconnect, and erase credentials
  }
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();

  Serial.println("  - Shutting down Bluetooth...");
  btStop(); // Stop Bluetooth
  esp_bt_controller_disable(); // Disable the controller

  // --- 3. SHUTDOWN RTC DOMAINS (Aggressive) ---
  // This saves the most power by turning off parts of the chip.
  Serial.println("  - Powering down RTC domains...");
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  
  // Isolate GPIO 12 (used for SD card) to prevent power leak during sleep
  // rtc_gpio_isolate(GPIO_NUM_12);

  // --- 4. CONFIGURE WAKEUP & SLEEP ---
  Serial.printf("  - Enabling timer wakeup for %d seconds.\n", SLEEP_DURATION_S);
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_DURATION_S * 1000000ULL);
  
  Serial.println("========================================");
  Serial.println("Going to sleep now...");
  Serial.flush(); // Send serial message before sleeping
  delay(100);     // Short delay to let hardware settle
  
  esp_deep_sleep_start();
}

// ====== WIFI SETUP ======
void setupWiFi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.printf("Connecting to WiFi (%s)...\n", ssid);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n⚠️ WiFi connection failed. Continuing without upload.");
  }
}

// ====== CAMERA SETUP ======
void setupCamera() {
  Serial.println("Initializing camera...");

camera_config_t config = {
  // --- All Pins (Your order was correct) ---
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,
  .pin_sscb_sda = SIOD_GPIO_NUM,
  .pin_sscb_scl = SIOC_GPIO_NUM,
  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,

  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_UXGA,

  .jpeg_quality = 12,
  .fb_count = 1,
  .fb_location = CAMERA_FB_IN_PSRAM,
  .grab_mode = CAMERA_GRAB_LATEST
};

  if (!psramFound()) {
    Serial.println("⚠️ PSRAM not found! Lowering resolution...");
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  } else {
    Serial.println("✅ PSRAM detected!");
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed (0x%x)\n", err);
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  Serial.println("✅ Camera initialized!");
}

// ====== CAPTURE + UPLOAD ======
bool captureAndUploadPhoto() {
  Serial.println("📸 Capturing photo...");
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("❌ Camera capture failed!");
    return false;
  }

  Serial.printf("Captured %dx%d image (%u bytes)\n", fb->width, fb->height, fb->len);

  bool success = false;
  for (int i = 1; i <= 3; i++) {
    if (uploadToServer(fb, i)) {
      success = true;
      break;
    }
    Serial.printf("Retrying upload... (%d/3)\n", i);
    delay(2000);
  }

  esp_camera_fb_return(fb);
  return success;
}

// ====== UPLOAD TO SERVER ======
bool uploadToServer(camera_fb_t *fb, int attemptNum) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected, skipping upload.");
    return false;
  }

  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(serverURL)) {
    Serial.println("❌ HTTP begin failed!");
    return false;
  }

  http.addHeader("X-API-Key", "Complex_Secret_Key_Group10_2025");
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("Device-ID", "esp32-smartstop-camera-001");

  int code = http.POST(fb->buf, fb->len);
  String response = http.getString();

  Serial.printf("HTTP Response Code: %d\n", code);
  // Detailed error reporting
  if (code == -1) {
    Serial.println("ERROR: Connection failed - check WiFi and server IP");
  } else if (code == -2) {
    Serial.println("ERROR: Send header failed");
  } else if (code == -3) {
    Serial.println("ERROR: Connection refused - server not running or unreachable");
    Serial.println("  - Check Flask server is running on 157.230.250.226:5000");
    Serial.println("  - Check firewall allows port 5000");
    Serial.println("  - Verify ESP32 can reach the server");
  } else if (code == -4) {
    Serial.println("ERROR: Connection lost during upload");
  } else if (code == -11) {
    Serial.println("ERROR: Request timeout");
  } else if (code >= 400) {
    Serial.printf("ERROR: Server error %d\n", code);
  } else if (code == 200) {
    Serial.println("SUCCESS: Photo uploaded!");
  }

  if (response.length()) {
    Serial.println("Server response:");
    Serial.println(response);
  }

  http.end();
  return (code == 200);
}

// ====== LED FLASH ======
void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.println("Flash LED ready");
}

// ====== LOOP ======
void loop() {
  delay(1000);  // not used; deep sleep handles periodic capture
}
