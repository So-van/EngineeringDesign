#define CAMERA_MODEL_XIAO_ESP32S3   // VERY IMPORTANT: must be BEFORE includes

#include "esp_camera.h"
#include "camera_pins.h"

// ===== USER SETTINGS =====
#define USE_BUILTIN_LED 21          // Yellow LED on XIAO ESP32S3
const int MOTION_THRESHOLD = 25;    // Larger = less sensitive
const int PRESENT_FRAMES = 1;       // Only need 1 frame for presence (since 1 capture per loop)
const int ABSENT_FRAMES = 1;
const int FRAME_INTERVAL_MS = 10000; // 10 seconds (set 60000 for 1 minute)

// ===== INTERNAL STATE =====
uint8_t* prevBuf = nullptr;
size_t prevLen = 0;
bool firstFrame = true;

bool humanPresent = false;

// =========================================
// CAMERA INITIALIZATION
// =========================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

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
  config.pixel_format = PIXFORMAT_GRAYSCALE;  // Fixed size, stable
  config.frame_size = FRAMESIZE_QQVGA;        // 160×120
  config.jpeg_quality = 12;
  config.fb_count = 1;

  config.fb_location = CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  return true;
}

// =========================================
// FRAME DIFFERENCE (motion estimation)
// =========================================
float computeAverageDiff(const uint8_t* curr, const uint8_t* prev, size_t len) {
  const int step = 32;  // sample every 32 bytes
  long sum = 0;
  long n = 0;

  for (size_t i = 0; i < len; i += step) {
    int d = curr[i] - prev[i];
    if (d < 0) d = -d;
    sum += d;
    n++;
  }

  if (n == 0) return 0;
  return (float)sum / n;
}

// =========================================
// PRESENCE LOGIC
// =========================================
void updatePresence(float diff) {
  if (diff > MOTION_THRESHOLD) {
    if (!humanPresent) {
      humanPresent = true;
      digitalWrite(USE_BUILTIN_LED, HIGH);
      Serial.println(">>> PRESENCE DETECTED");
    }
  } else {
    if (humanPresent) {
      humanPresent = false;
      digitalWrite(USE_BUILTIN_LED, LOW);
      Serial.println(">>> AREA IS QUIET");
    }
  }
}

// =========================================
// SETUP
// =========================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(USE_BUILTIN_LED, OUTPUT);
  digitalWrite(USE_BUILTIN_LED, LOW);

  Serial.println("\nCamera Presence Detector Starting...");

  if (!initCamera()) {
    Serial.println("Camera init failed. Restarting...");
    delay(3000);
    ESP.restart();
  }
}

// =========================================
// MAIN LOOP
// =========================================
void loop() {

  // Reinitialize camera each cycle (important for long intervals)
  esp_camera_deinit();
  delay(50);
  initCamera();
  delay(50);

  // Grab frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(FRAME_INTERVAL_MS);
    return;
  }

  // First frame → store and skip
  if (firstFrame) {
    prevLen = fb->len;
    prevBuf = (uint8_t*)malloc(prevLen);
    memcpy(prevBuf, fb->buf, prevLen);
    firstFrame = false;

    Serial.println("First frame stored.");
    esp_camera_fb_return(fb);
    delay(FRAME_INTERVAL_MS);
    return;
  }

  // Compute motion
  float diff = computeAverageDiff(fb->buf, prevBuf, fb->len);
  Serial.print("avgDiff = ");
  Serial.println(diff);

  updatePresence(diff);

  // Save current as previous
  memcpy(prevBuf, fb->buf, fb->len);

  esp_camera_fb_return(fb);

  delay(FRAME_INTERVAL_MS);
}
