#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include "driver/gpio.h"

// ================= WIFI / MQTT ================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* MQTT_HOST = "YOUR_MQTT_BROKER_IP";
const uint16_t MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient mqtt(espClient);

const char* TOPIC_IR    = "nus-smartstop/ir-sensor/data";
const char* TOPIC_VOICE = "nus-smartstop/voice";
const char* DEVICE_ID   = "esp32-smartstop-01";

unsigned long lastPubMs = 0;
const unsigned long PUB_INTERVAL = 1000;

// ================ IR PEOPLE COUNT =============
// --- SET 1 (Bus Entrance) ---
const int IR_TX_A = 26;
const int IR_TX_B = 27;
const int IR_RX_A = 33;
const int IR_RX_B = 25;

// --- SET 2 (Bus Stop Entrance) ---
const int IR_TX_C = 21;
const int IR_TX_D = 18;
const int IR_RX_C = 19;
const int IR_RX_D = 17;

volatile int32_t peopleCount = 0;     // atomic on ESP32
const uint32_t SEQ_WINDOW = 1000;     // Increased slightly to allow slower walking
constexpr gpio_int_type_t EDGE_TYPE = GPIO_INTR_NEGEDGE; 

// Structure to hold event data
typedef struct { 
  uint8_t sensor; // 'A', 'B', 'C', or 'D'
  uint32_t t_ms; 
} IrEdgeEvent;

QueueHandle_t irQueue;

// State tracking variables for Door 1
volatile uint32_t tA_fall_ms = 0, tB_fall_ms = 0;
// State tracking variables for Door 2
volatile uint32_t tC_fall_ms = 0, tD_fall_ms = 0;

// ================== ULTRASONIC =================
float distance = 0.0; // placeholder

// ================== I2S / FFT ==================
// INMP441 pins
#define I2S_WS   15
#define I2S_SD   32
#define I2S_SCK  14
#define I2S_PORT I2S_NUM_0

#define N_SAMPLES   512
#define SAMPLE_RATE 16000.0

int32_t rawBuf[N_SAMPLES];
double  vReal[N_SAMPLES], vImag[N_SAMPLES];
ArduinoFFT FFT(vReal, vImag, N_SAMPLES, SAMPLE_RATE);

bool  voiceActive = false;
float ratioEMA = 0.0f;
unsigned long lastVoicePrint = 0;

const double QUIET_RMS_MIN = 80000.0;
const float  RATIO_ON = 0.60f, RATIO_OFF = 0.50f;
const float  VOICING_ON_DB = 2.5f, VOICING_OFF_DB = 1.5f;
const float  SFM_ON_MAX = 0.65f, SFM_OFF_MIN = 0.80f;
const uint8_t K_ON = 2, K_OFF = 4;

// ------------- WiFi / MQTT helpers -------------
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(200); }
}

void ensureMqtt() {
  if (mqtt.connected()) return;
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    mqtt.connect(DEVICE_ID);
    if (!mqtt.connected()) delay(500);
  }
}

// ------------- I2S (mic) setup -----------------
void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = (int)SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_start(I2S_PORT);
}

// ------------- IR ISR handlers -----------------
void IRAM_ATTR send_ir_event(char id) {
    IrEdgeEvent ev = { (uint8_t)id, (uint32_t)millis() };
    BaseType_t hpw = pdFALSE;
    xQueueSendFromISR(irQueue, &ev, &hpw);
    if (hpw) portYIELD_FROM_ISR();
}

static void IRAM_ATTR isr_ir_a(void* arg) { send_ir_event('A'); }
static void IRAM_ATTR isr_ir_b(void* arg) { send_ir_event('B'); }
static void IRAM_ATTR isr_ir_c(void* arg) { send_ir_event('C'); }
static void IRAM_ATTR isr_ir_d(void* arg) { send_ir_event('D'); }

// ------------- IR GPIO init --------------------
void setupIRGPIO() {
  pinMode(IR_TX_A, OUTPUT); digitalWrite(IR_TX_A, HIGH);
  pinMode(IR_TX_B, OUTPUT); digitalWrite(IR_TX_B, HIGH);
  pinMode(IR_RX_A, INPUT_PULLUP);
  pinMode(IR_RX_B, INPUT_PULLUP);

  pinMode(IR_TX_C, OUTPUT); digitalWrite(IR_TX_C, HIGH);
  pinMode(IR_TX_D, OUTPUT); digitalWrite(IR_TX_D, HIGH);
  pinMode(IR_RX_C, INPUT_PULLUP);
  pinMode(IR_RX_D, INPUT_PULLUP);

  gpio_config_t cfg = {};
  cfg.pin_bit_mask = (1ULL << IR_RX_A) | (1ULL << IR_RX_B) | (1ULL << IR_RX_C) | (1ULL << IR_RX_D);
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = EDGE_TYPE;
  gpio_config(&cfg);

  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)IR_RX_A, isr_ir_a, nullptr);
  gpio_isr_handler_add((gpio_num_t)IR_RX_B, isr_ir_b, nullptr);
  gpio_isr_handler_add((gpio_num_t)IR_RX_C, isr_ir_c, nullptr);
  gpio_isr_handler_add((gpio_num_t)IR_RX_D, isr_ir_d, nullptr);
}

// ================== FreeRTOS tasks =============
// IR task with COOLDOWN LOGIC added
void IRTask(void* pv) {
  IrEdgeEvent ev;
  const uint32_t DEBOUNCE_MS = 50;     // Ignore super short glitches
  const uint32_t COUNT_COOLDOWN = 1500; // 1.5 seconds blocked after a valid count
  
  // Debounce timestamps
  static uint32_t lastA_ms = 0, lastB_ms = 0;
  static uint32_t lastC_ms = 0, lastD_ms = 0;

  // Cooldown timestamps (When was the last successful count?)
  static uint32_t lastCountTime1 = 0;
  static uint32_t lastCountTime2 = 0;

  for (;;) {
    if (xQueueReceive(irQueue, &ev, pdMS_TO_TICKS(50)) == pdTRUE) {
      uint32_t now = ev.t_ms;

      // ================= DOOR 1 LOGIC (A & B) =================
      if (ev.sensor == 'A' || ev.sensor == 'B') {
        
        // If we just counted someone here < 1.5s ago, ignore EVERYTHING for Door 1
        if (now - lastCountTime1 < COUNT_COOLDOWN) {
            // Reset sequence triggers to prevent latent firing
            tA_fall_ms = 0;
            tB_fall_ms = 0;
            continue; 
        }

        if (ev.sensor == 'A') {
          if (now - lastA_ms < DEBOUNCE_MS) continue;
          lastA_ms = now;
          tA_fall_ms = now;

          // Check Exit Sequence (B -> A)
          if (tB_fall_ms && (tA_fall_ms >= tB_fall_ms) && (tA_fall_ms - tB_fall_ms <= SEQ_WINDOW)) {
            peopleCount--;
            if (peopleCount < 0) peopleCount = 0;
            Serial.printf("Door 1 Exit (B->A). Count=%ld\n", (long)peopleCount);
            
            // SUCCESS! Set cooldown and reset triggers
            lastCountTime1 = now;
            tA_fall_ms = tB_fall_ms = 0; 
          }
        } else { // Sensor B
          if (now - lastB_ms < DEBOUNCE_MS) continue;
          lastB_ms = now;
          tB_fall_ms = now;

          // Check Entry Sequence (A -> B)
          if (tA_fall_ms && (tB_fall_ms >= tA_fall_ms) && (tB_fall_ms - tA_fall_ms <= SEQ_WINDOW)) {
            peopleCount++;
            Serial.printf("Door 1 Entry (A->B). Count=%ld\n", (long)peopleCount);
            
            // SUCCESS! Set cooldown and reset triggers
            lastCountTime1 = now;
            tA_fall_ms = tB_fall_ms = 0; 
          }
        }
      }

      // ================= DOOR 2 LOGIC (C & D) =================
      else if (ev.sensor == 'C' || ev.sensor == 'D') {

        // If we just counted someone here < 1.5s ago, ignore EVERYTHING for Door 2
        if (now - lastCountTime2 < COUNT_COOLDOWN) {
            tC_fall_ms = 0;
            tD_fall_ms = 0;
            continue; 
        }

        if (ev.sensor == 'C') {
          if (now - lastC_ms < DEBOUNCE_MS) continue;
          lastC_ms = now;
          tC_fall_ms = now;

          // Check Exit Sequence (D -> C)
          if (tD_fall_ms && (tC_fall_ms >= tD_fall_ms) && (tC_fall_ms - tD_fall_ms <= SEQ_WINDOW)) {
            peopleCount--;
            if (peopleCount < 0) peopleCount = 0;
            Serial.printf("Door 2 Exit (D->C). Count=%ld\n", (long)peopleCount);
            
            // SUCCESS! Set cooldown and reset triggers
            lastCountTime2 = now;
            tC_fall_ms = tD_fall_ms = 0; 
          }
        } else { // Sensor D
          if (now - lastD_ms < DEBOUNCE_MS) continue;
          lastD_ms = now;
          tD_fall_ms = now;

          // Check Entry Sequence (C -> D)
          if (tC_fall_ms && (tD_fall_ms >= tC_fall_ms) && (tD_fall_ms - tC_fall_ms <= SEQ_WINDOW)) {
            peopleCount++;
            Serial.printf("Door 2 Entry (C->D). Count=%ld\n", (long)peopleCount);
            
            // SUCCESS! Set cooldown and reset triggers
            lastCountTime2 = now;
            tC_fall_ms = tD_fall_ms = 0; 
          }
        }
      }
    }

    // Cleanup stale triggers (Timeout)
    uint32_t now2 = millis();
    if (tA_fall_ms && (now2 - tA_fall_ms > SEQ_WINDOW)) tA_fall_ms = 0;
    if (tB_fall_ms && (now2 - tB_fall_ms > SEQ_WINDOW)) tB_fall_ms = 0;
    if (tC_fall_ms && (now2 - tC_fall_ms > SEQ_WINDOW)) tC_fall_ms = 0;
    if (tD_fall_ms && (now2 - tD_fall_ms > SEQ_WINDOW)) tD_fall_ms = 0;
  }
}

void VoiceTask(void* pv) {
  for (;;) {
    size_t br = 0;
    i2s_read(I2S_PORT, (void*)rawBuf, sizeof(rawBuf), &br, portMAX_DELAY);
    int nRead = br / sizeof(int32_t);
    if (nRead <= 0) { vTaskDelay(1); continue; }

    static double hp_y = 0.0, hp_x1 = 0.0;
    const double alphaHP = 0.997;
    for (int i = 0; i < nRead; i++) {
      int32_t s = rawBuf[i] >> 8;
      double x = (double)s;
      hp_y = alphaHP * (hp_y + x - hp_x1);
      hp_x1 = x;
      vReal[i] = hp_y;
      vImag[i] = 0.0;
    }

    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    double totalE = 0.0, voiceE = 0.0;
    for (int i = 1; i < N_SAMPLES/2; i++) {
      double f = (i * SAMPLE_RATE) / N_SAMPLES;
      double p = vReal[i] * vReal[i];
      totalE += p;
      if (f >= 120 && f <= 3400) voiceE += p;
    }
    double rms   = sqrt(totalE / (N_SAMPLES/2));
    double ratio = (totalE > 0.0) ? (voiceE / totalE) : 0.0;
    ratioEMA = 0.25f * ratio + 0.75f * ratioEMA;

    if (rms < QUIET_RMS_MIN) {
      voiceActive = false;
    } else {
      double lowE = 0.0, midE = 0.0;
      for (int i = 1; i < N_SAMPLES/2; i++) {
        double f = (i * SAMPLE_RATE) / N_SAMPLES;
        double p = vReal[i] * vReal[i];
        if (f >= 80  && f <= 300)  lowE += p;
        if (f >= 500 && f <= 1500) midE += p;
      }
      double voicingDb = 10.0 * log10((lowE + 1e-9) / (midE + 1e-9));

      double sumLog = 0.0, sumLin = 0.0; int nb = 0;
      for (int i = 1; i < N_SAMPLES/2; i++) {
        double f = (i * SAMPLE_RATE) / N_SAMPLES;
        if (f < 200 || f > 3400) continue;
        double p = vReal[i] * vReal[i] + 1e-9;
        sumLog += log(p); sumLin += p; nb++;
      }
      double sfm = 1.0;
      if (nb > 0) {
        double geom  = exp(sumLog / nb);
        double arith = sumLin / nb;
        sfm = (arith > 0) ? (geom / arith) : 1.0;
      }

      static uint8_t upCnt = 0, downCnt = 0;
      bool onGate  = (ratio >= RATIO_ON  && voicingDb >= VOICING_ON_DB && sfm <= SFM_ON_MAX);
      bool offGate = (ratio <= RATIO_OFF || voicingDb <= VOICING_OFF_DB || sfm >= SFM_OFF_MIN);

      if (!voiceActive) {
        if (onGate) { if (++upCnt >= K_ON) { voiceActive = true;  downCnt = 0; } }
        else upCnt = 0;
      } else {
        if (offGate) { if (++downCnt >= K_OFF) { voiceActive = false; upCnt = 0; } }
        else downCnt = 0;
      }
    }

    if (millis() - lastVoicePrint > 1000) {
      lastVoicePrint = millis();
      Serial.printf("Voice=%s  Count=%ld\n", voiceActive ? "YES" : "NO", (long)peopleCount);
    }
    taskYIELD(); 
  }
}

void publishData() {
  if (!mqtt.connected()) return;
  int32_t count_snapshot = peopleCount;
  char msg[128];
  snprintf(msg, sizeof(msg),
  "{\"deviceId\":\"%s\",\"people_count\":%ld,\"voice\":%s}",
  DEVICE_ID, (long)count_snapshot, voiceActive ? "true" : "false");

  mqtt.publish(TOPIC_IR, msg, false);
}

void setup() {
  Serial.begin(921600); 
  
  irQueue = xQueueCreate(32, sizeof(IrEdgeEvent));
  setupIRGPIO();
  setupI2S();

  ensureWifi();
  ensureMqtt();

  xTaskCreatePinnedToCore(IRTask,    "IRTask",    2048, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(VoiceTask, "VoiceTask", 4096, nullptr, 2, nullptr, 0);
}

void loop() {
  mqtt.loop();
  ensureWifi();
  ensureMqtt();

  unsigned long now = millis();
  if (now - lastPubMs >= PUB_INTERVAL) {
    lastPubMs = now;
    publishData();
  }
  delay(1); 
}