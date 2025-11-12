#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

//
// ESP32 Camera Photo Capture & Upload to Flask Server
// This code captures photos as JPG files and uploads them to a Flask server
// Optimized for CS3237 Group 10 Image Gallery
//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE  // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_CAMS3_UNIT  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Configuration
// ===========================
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// Flask Server Configuration
const char* serverURL = "http://YOUR_FLASK_SERVER_IP:5000/upload";  // Update this with your Flask server IP

// Photo capture settings
#define CAPTURE_INTERVAL 60000  // Capture interval in milliseconds (60 seconds = 1 minute)
#define SLEEP_DURATION 60       // Deep sleep duration in seconds (60 seconds = 1 minute)
#define BUTTON_PIN 0           // GPIO0 (Boot button) for manual capture

// Function declarations
void setupCamera();
void setupWiFi();
bool captureAndUploadPhoto();
bool uploadToServer(camera_fb_t * fb, int attemptNum);
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("=== ESP32 Camera Photo Capture & Upload ===");
  Serial.println("CS3237 Group 10 - Image Gallery Integration");

  // A: Complete setup
  Serial.println("A: Starting setup...");
  
  // Setup camera
  setupCamera();
  
  // Setup WiFi
  setupWiFi();
  
  // Setup LED flash
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  Serial.println("A: Setup complete!");
  Serial.println("B: Taking photo and uploading to Flask server...");
  
  // Take ONE photo during setup and upload to Flask server
  bool success = captureAndUploadPhoto();
  
  if (success) {
    Serial.println("Photo uploaded successfully to Flask server!");
    Serial.println("Check your CS3237 Group 10 Image Gallery");
  } else {
    Serial.println("Failed to upload photo to Flask server");
  }
  
  Serial.println();
  Serial.println("=== Entering deep sleep mode ===");
  Serial.printf("Sleep duration: %d seconds (1 minute)\n", SLEEP_DURATION);
  Serial.println("ESP32 will wake up automatically in 1 minute to capture next photo");
  Serial.println("Or press reset button to wake up immediately");
  
  delay(1000); // Give time for serial output to complete
  
  // Configure deep sleep to wake up after 1 minute (60 seconds)
  // Convert seconds to microseconds (1 second = 1,000,000 microseconds)
  uint64_t sleepTimeMicros = SLEEP_DURATION * 1000000ULL;
  
  esp_sleep_enable_timer_wakeup(sleepTimeMicros);
  
  Serial.println("Going to sleep now...");
  Serial.flush(); // Ensure all serial data is sent before sleep
  
  // Enter deep sleep - will wake up after 60 seconds
  esp_deep_sleep_start();
}

void setupCamera() {
  Serial.println("Initializing camera...");
  
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
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for photo capture
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  
  // Optimize for photo quality
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_UXGA);  // Higher resolution for photos
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

  Serial.println("Camera initialized successfully!");
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println();
    Serial.println("WiFi connection failed - continuing without WiFi");
  }
}

void loop() {
  // Photo was uploaded during setup
  // Reset ESP32 to capture and upload another photo
  delay(1000);
}

bool captureAndUploadPhoto() {
  Serial.println("Taking picture...");
  
  // Capture photo
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }
  
  Serial.printf("Picture captured - Size: %u bytes\n", fb->len);
  Serial.printf("Dimensions: %dx%d\n", fb->width, fb->height);
  
  // Retry logic - attempt upload up to 3 times
  const int MAX_RETRIES = 3;
  bool uploadSuccess = false;
  
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    if (attempt > 1) {
      Serial.printf("\n--- Retry attempt %d/%d ---\n", attempt, MAX_RETRIES);
      delay(2000); // Wait 2 seconds before retry
    }
    
    uploadSuccess = uploadToServer(fb, attempt);
    
    if (uploadSuccess) {
      break; // Success - exit retry loop
    }
    
    if (attempt < MAX_RETRIES) {
      Serial.println("Upload failed, will retry...");
    }
  }
  
  esp_camera_fb_return(fb);
  
  if (!uploadSuccess) {
    Serial.printf("\nFailed to upload after %d attempts\n", MAX_RETRIES);
  }
  
  return uploadSuccess;
}

bool uploadToServer(camera_fb_t * fb, int attemptNum) {
  Serial.printf("Uploading to Flask server (attempt %d)...\n", attemptNum);
  
  // Verify WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: WiFi disconnected before upload");
    return false;
  }
  
  HTTPClient http;
  http.setTimeout(30000); // 30 second timeout
  
  if (!http.begin(serverURL)) {
    Serial.println("HTTP client begin failed");
    return false;
  }
  
  // Set headers for raw JPEG upload
  // Flask server expects: Content-Type: image/jpeg and optional Device-ID header
  http.addHeader("Content-Type", "image/jpeg");
  
  // Use MAC address as device ID for unique filenames on server
  String deviceId = WiFi.macAddress();
  deviceId.replace(":", ""); // Remove colons from MAC address
  http.addHeader("Device-ID", "ESP32CAM_" + deviceId);
  
  if (attemptNum == 1) {
    Serial.println("Device ID: ESP32CAM_" + deviceId);
  }
  
  // POST raw JPEG bytes
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  // Get response
  String response = http.getString();
  
  Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
  
  // Detailed error reporting
  if (httpResponseCode == -1) {
    Serial.println("ERROR: Connection failed - check WiFi and server IP");
  } else if (httpResponseCode == -2) {
    Serial.println("ERROR: Send header failed");
  } else if (httpResponseCode == -3) {
    Serial.println("ERROR: Connection refused - server not running or unreachable");
    Serial.println("  - Check Flask server is running on YOUR_SERVER_IP:5000");
    Serial.println("  - Check firewall allows port 5000");
    Serial.println("  - Verify ESP32 can reach the server");
  } else if (httpResponseCode == -4) {
    Serial.println("ERROR: Connection lost during upload");
  } else if (httpResponseCode == -11) {
    Serial.println("ERROR: Request timeout");
  } else if (httpResponseCode >= 400) {
    Serial.printf("ERROR: Server error %d\n", httpResponseCode);
  } else if (httpResponseCode == 200) {
    Serial.println("SUCCESS: Photo uploaded!");
  }
  
  if (response.length() > 0) {
    Serial.println("Server response:");
    Serial.println(response);
  }
  
  http.end();
  
  // Check if upload was successful (HTTP 200)
  return (httpResponseCode == 200);
}

void setupLedFlash(int pin) {
  // Simple LED setup for flash
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.println("LED Flash setup complete");
}
