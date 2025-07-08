#include <WiFi.h>
#include <esp_now.h>

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.peer_addr[0] = 0xFF; // Make sure it's a broadcast address
  peerInfo.peer_addr[1] = 0xFF;
  peerInfo.peer_addr[2] = 0xFF;
  peerInfo.peer_addr[3] = 0xFF;
  peerInfo.peer_addr[4] = 0xFF;
  peerInfo.peer_addr[5] = 0xFF;

  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }
}

void loop() {
  const char *msg = "Fuck you !";
  esp_now_send(broadcastAddress, (uint8_t *)msg, strlen(msg));
  Serial.println("Broadcast sent!");
  delay(2000);
}
