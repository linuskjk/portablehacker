#include <ESP8266WiFi.h>
extern "C" {
  #include "user_interface.h"
}

void sniffer_callback(uint8_t *buf, uint16_t len) {
  if (len < 40) return;

  bool isRealEapol = false;
  
  for (int i = 20; i < 40; i++) {
    if (buf[i] == 0x88 && buf[i+1] == 0x8e) {
      isRealEapol = true;
      break;
    }
  }
  if (buf[12] == 0x80) isRealEapol = false;

  if (isRealEapol) {
    Serial.write((uint8_t*)&len, 2);
    Serial.write(buf, len);
  }
}


void setup() {
  Serial.begin(921600);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SCAN") {
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; ++i) {
        Serial.printf("NET:%d|%s|%d\n", i, WiFi.SSID(i).c_str(), WiFi.channel(i));
      }
      Serial.println("SCAN_DONE");
    } else if (cmd.startsWith("START:")) {
      int ch = cmd.substring(6).toInt();
      wifi_promiscuous_enable(0);
      wifi_set_promiscuous_rx_cb(sniffer_callback);
      wifi_set_channel(ch);
      wifi_promiscuous_enable(1);
      Serial.println("SNIFFER_STARTED");
      while (true) { yield(); }
    }
  }
}
