#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ========== Configuration Section ========== //
// WiFi Credentials
const char* ssid = "iPhone";
const char* password = "sovan123";

// Camera Pins (ESP32-S3)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39
#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13

// Motion Detection Settings
const uint8_t MOTION_THRESHOLD = 20;    // Sensitivity (5-30)
const unsigned long MOTION_TIMEOUT = 5000; // Clear after 5s
const unsigned long CAPTURE_INTERVAL = 200; // Web refresh rate (ms)

// ========== Global Variables ========== //
WebServer server(80);
bool motionDetected = false;
unsigned long lastMotionTime = 0;
unsigned long lastCaptureTime = 0;

// ========== Function Prototypes ========== //
void setupCamera();
void connectWiFi();
bool detectMotion(camera_fb_t* current, camera_fb_t* previous);
void handleCapture();
void handleRoot();
void handleMotion();
void startCameraServer();

// ========== Main Functions ========== //
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n\nInitializing ESP32-S3 Camera");

  connectWiFi();
  setupCamera();
  startCameraServer();

  Serial.println("System Ready");
  Serial.print("Stream URL: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  
  // Optional: Add actions when motion is detected
  if (motionDetected && millis() - lastMotionTime < MOTION_TIMEOUT) {
    // Example: Trigger GPIO, send notification, etc.
  }
}

// ========== Camera Functions ========== //
void setupCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Lower resolution for better performance
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 10;
  config.fb_count = 2; // Double buffer for motion detection

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    ESP.restart();
  }

  // Optional camera settings
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0-6 (0 = no effect)
}

// ========== Motion Detection ========== //
bool detectMotion(camera_fb_t* current, camera_fb_t* previous) {
  if (!current || !previous || current->len != previous->len) return false;

  uint32_t diff = 0;
  const uint32_t threshold = (current->len * MOTION_THRESHOLD) / 100;
  
  // Compare frames (skip first bytes which may contain metadata)
  for (size_t i = 64; i < current->len; i++) {
    diff += abs(current->buf[i] - previous->buf[i]);
    if (diff > threshold) return true;
  }
  return false;
}

// ========== Web Server Functions ========== //
void handleCapture() {
  static camera_fb_t* prevFrame = NULL;
  
  if (millis() - lastCaptureTime < CAPTURE_INTERVAL) {
    if (prevFrame) {
      server.send_P(200, "image/jpeg", (char*)prevFrame->buf, prevFrame->len);
      return;
    }
  }
  lastCaptureTime = millis();

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    server.send(500, "text/plain", "Camera error");
    return;
  }

  // Motion detection
  if (prevFrame) {
    if (detectMotion(fb, prevFrame)) {
      motionDetected = true;
      lastMotionTime = millis();
      Serial.println("Motion detected!");
    } else if (millis() - lastMotionTime > MOTION_TIMEOUT) {
      motionDetected = false;
    }
    esp_camera_fb_return(prevFrame);
  }
  prevFrame = fb;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "image/jpeg", (char*)fb->buf, fb->len);
}

void handleRoot() {
  server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32-S3 Human Detection</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
        #status { font-size: 24px; font-weight: bold; padding: 10px; }
        .motion { color: #4CAF50; background: #E8F5E9; }
        .no-motion { color: #F44336; background: #FFEBEE; }
        img { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 4px; }
        .container { max-width: 640px; margin: 0 auto; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>ESP32-S3 Human Detection</h1>
        <div id="status" class="no-motion">NO MOTION</div>
        <img id="stream" src="/capture">
        <p>IP: )rawliteral" + String(WiFi.localIP().toString()) + R"rawliteral(</p>
      </div>
      <script>
        const statusDiv = document.getElementById('status');
        const img = document.getElementById('stream');
        
        function updateStream() {
          img.src = '/capture?' + Date.now();
          fetch('/motion').then(r => r.text()).then(status => {
            statusDiv.textContent = status;
            statusDiv.className = status.includes('DETECTED') ? 'motion' : 'no-motion';
          });
        }
        
        setInterval(updateStream, )rawliteral" + String(CAPTURE_INTERVAL) + R"rawliteral();
        updateStream();
      </script>
    </body>
    </html>
  )rawliteral");
}

void handleMotion() {
  String status = (motionDetected && (millis() - lastMotionTime < MOTION_TIMEOUT)) 
                 ? "MOTION DETECTED" : "NO MOTION";
  server.send(200, "text/plain", status);
}

// ========== Network Functions ========== //
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect. Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void startCameraServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/motion", HTTP_GET, handleMotion);
  
  // Handle 404 errors
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  Serial.println("HTTP server started");
}