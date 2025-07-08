#include <WiFi.h>
#include <esp_now.h>

#define PIN_LED 7

// Updated callback for new ESP-NOW API
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  // Print sender's MAC address
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  Serial.print("Received from: ");
  Serial.println(macStr);

  // Print received data
  Serial.print("Data: ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)incomingData[i]);
  }
  Serial.println();

  // Blink LED to indicate reception
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

  WiFi.mode(WIFI_STA);  // Required for ESP-NOW

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Register the updated receive callback
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("ESP-NOW receiver ready.");
}

void loop() {
  // No logic here; reception is handled via callback
}