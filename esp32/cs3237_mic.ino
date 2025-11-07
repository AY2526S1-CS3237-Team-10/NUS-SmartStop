#include <Arduino.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---------- WiFi / MQTT ----------
const char* WIFI_SSID = "test";
const char* WIFI_PASS = "abcdefgh";
const char* MQTT_HOST = "157.230.250.226";
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID = "esp32-mic-01";
const char* TOPIC_VOICE   = "nus-smartstop/voice";
const char* TOPIC_FEATURE = "nus-smartstop/audio/features";

WiFiClient espClient;
PubSubClient mqtt(espClient);
unsigned long lastPubMs = 0;
const unsigned long PUB_EVERY_MS = 1000;

// ---------- INMP441 pins ----------
#define I2S_WS   15
#define I2S_SD   32
#define I2S_SCK  14
#define I2S_PORT I2S_NUM_0

// ---------- FFT ----------
#define N_SAMPLES   1024
#define SAMPLE_RATE 16000.0
int32_t rawBuf[N_SAMPLES];
double  vReal[N_SAMPLES], vImag[N_SAMPLES];
ArduinoFFT FFT(vReal, vImag, N_SAMPLES, SAMPLE_RATE);

// ---------- Detection thresholds ----------
const double QUIET_RMS_MIN = 80000.0;
const float  RATIO_ON      = 0.60f;
const float  RATIO_OFF     = 0.50f;
const float  VOICING_ON_DB = 2.5f;
const float  VOICING_OFF_DB= 1.5f;
const float  SFM_ON_MAX    = 0.65f;
const float  SFM_OFF_MIN   = 0.80f;
const uint8_t K_ON         = 2;
const uint8_t K_OFF        = 4;
const unsigned long PRINT_MS = 250;

bool voiceActive = false;
uint8_t upCnt = 0, downCnt = 0;
float ratioEMA = 0.0f;
unsigned long lastPrint = 0;

// ---------- Networking helpers ----------
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void ensureMqtt() {
  if (mqtt.connected()) return;
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  Serial.print("Connecting to MQTT...");
  while (!mqtt.connected()) {
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println(" connected.");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

// ---------- Network status printer ----------
void printNetStatus() {
  Serial.print("WiFi: ");
  Serial.print((WiFi.status()==WL_CONNECTED) ? "OK " : "DISCONNECTED ");
  if (WiFi.status()==WL_CONNECTED) {
    Serial.print("IP=");
    Serial.print(WiFi.localIP());
    Serial.print(" RSSI=");
    Serial.print(WiFi.RSSI());
  }
  Serial.print("  |  MQTT: ");
  Serial.println(mqtt.connected() ? "OK" : "DISCONNECTED");
}

// ---------- I2S setup ----------
void setupI2S() {
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
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_start(I2S_PORT);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  setupI2S();
  ensureWifi();
  ensureMqtt();
  Serial.println("Boot complete.");
  printNetStatus();
}

// ---------- Loop ----------
void loop() {
  // --- Read samples ---
  size_t br = 0;
  i2s_read(I2S_PORT, (void*)rawBuf, sizeof(rawBuf), &br, portMAX_DELAY);
  int nRead = br / sizeof(int32_t);
  if (nRead <= 0) return;

  // --- High-pass filter (~100 Hz) ---
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

  // --- FFT ---
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // --- Energy ---
  double totalE = 0.0, voiceE = 0.0;
  for (int i = 1; i < N_SAMPLES/2; i++) {
    double f = (i * SAMPLE_RATE) / N_SAMPLES;
    double p = vReal[i] * vReal[i];
    totalE += p;
    if (f >= 120 && f <= 3400) voiceE += p;
  }
  double rms = sqrt(totalE / (N_SAMPLES/2));
  if (rms < QUIET_RMS_MIN) {
    if (voiceActive) { voiceActive = false; upCnt = 0; }
    ratioEMA *= 0.9f;
    static unsigned long t = 0; if (millis()-t > PRINT_MS) { t = millis(); Serial.println("quiet floor -> state=no"); }
  }
  double ratio = (totalE > 0.0) ? (voiceE / totalE) : 0.0;
  ratioEMA = 0.25f * ratio + 0.75f * ratioEMA;

  // --- Voicing / SFM ---
  double lowE=0, midE=0;
  for (int i=1;i<N_SAMPLES/2;i++){
    double f=(i*SAMPLE_RATE)/N_SAMPLES;
    double p=vReal[i]*vReal[i];
    if(f>=80&&f<=300)lowE+=p;
    if(f>=500&&f<=1500)midE+=p;
  }
  const double eps=1e-9;
  double voicingDb=10.0*log10((lowE+eps)/(midE+eps));
  voicingDb=max(-20.0,min(20.0,voicingDb));
  double sumLog=0.0,sumLin=0.0;int nb=0;
  for(int i=1;i<N_SAMPLES/2;i++){
    double f=(i*SAMPLE_RATE)/N_SAMPLES;
    if(f<200||f>3400)continue;
    double p=vReal[i]*vReal[i]+eps;
    sumLog+=log(p);sumLin+=p;nb++;
  }
  double sfm=1.0;
  if(nb>0){
    double geom=exp(sumLog/nb);
    double arith=sumLin/nb;
    sfm=(arith>0)?(geom/arith):1.0;
  }

  // --- Debounce logic ---
  int onVotes=0;
  onVotes+=(ratio>=RATIO_ON);
  onVotes+=(voicingDb>=VOICING_ON_DB);
  onVotes+=(sfm<=SFM_ON_MAX);
  bool onGate=(onVotes>=2)&&(rms>=QUIET_RMS_MIN);
  bool offGate=(ratio<=RATIO_OFF)||(voicingDb<=VOICING_OFF_DB)||(sfm>=SFM_OFF_MIN)||(rms<QUIET_RMS_MIN);

  if(!voiceActive){
    if(onGate){if(++upCnt>=K_ON){voiceActive=true;downCnt=0;}}
    else upCnt=0;
  }else{
    if(offGate){if(++downCnt>=K_OFF){voiceActive=false;upCnt=0;}}
    else downCnt=0;
  }

  unsigned long now=millis();
  if(now-lastPrint>=PRINT_MS){
    lastPrint=now;
    Serial.printf("ratio=%.2f  voicing=%.2f dB  sfm=%.2f  rms=%.0f  state=%s (↑%u ↓%u)\n",
                  ratio, voicingDb, sfm, rms, voiceActive?"YES":"no", upCnt, downCnt);
  }

  // --- MQTT + WiFi heartbeat ---
  mqtt.loop();
  static unsigned long lastNet=0;
  if(millis()-lastNet>5000){
    lastNet=millis();
    if(WiFi.status()!=WL_CONNECTED)ensureWifi();
    if(!mqtt.connected())ensureMqtt();
    printNetStatus();
  }

  // --- MQTT publish ---
  if(millis()-lastPubMs>=PUB_EVERY_MS){
    lastPubMs=millis();
    char msg1[96];
    snprintf(msg1,sizeof(msg1),
      "{\"deviceId\":\"%s\",\"voice\":%s}",DEVICE_ID,voiceActive?"true":"false");
    mqtt.publish(TOPIC_VOICE,msg1,false);
  }
}
