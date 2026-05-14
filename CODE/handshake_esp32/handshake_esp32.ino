#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define SD_CS 17
File pcapFile;

const uint8_t pcap_header[] = {
  0xD4, 0xC3, 0xB2, 0xA1, 0x02, 0x00, 0x04, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0xFF, 0xFF, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23, SD_CS);
  if(!SD.begin(SD_CS)) Serial.println("SD Error");
}

void loop() {
  if (Serial.available()) {
    // Prüfe auf das PKT-Signal ohne den Puffer mit readString zu leeren
    if (Serial.peek() == 'P') {
      String cmd = Serial.readStringUntil(':');
      
      if (cmd == "PKT") {
        while(Serial.available() < 2) yield();
        uint16_t len;
        Serial.readBytes((char*)&len, 2);
        
        uint8_t* buf = (uint8_t*)malloc(len);
        if (buf) {
          size_t read = 0;
          while (read < len) {
            if (Serial.available()) buf[read++] = Serial.read();
          }

          if(pcapFile) {
            uint32_t ts = millis();
            pcapFile.write((uint8_t*)&ts, 4);
            pcapFile.write((uint8_t*)&ts, 4);
            pcapFile.write((uint8_t*)&len, 4);
            pcapFile.write((uint8_t*)&len, 4);
            pcapFile.write(buf, len);
            pcapFile.flush();
          }
          free(buf);
        }
      } 
      else if (cmd == "START_CAPTURE") {
        Serial.read(); // Den Doppelpunkt nach START_CAPTURE verschlucken
        String ssid = Serial.readStringUntil('\n');
        ssid.trim();
        if(pcapFile) pcapFile.close();
        pcapFile = SD.open("/capture_" + ssid + ".pcap", FILE_WRITE);
        if(pcapFile) pcapFile.write(pcap_header, 24);
      }
    } 
    else if (Serial.peek() == 'E') {
       if (Serial.readStringUntil('\n').startsWith("END_CAPTURE")) {
         if(pcapFile) {
           pcapFile.close();
           Serial.println("Finalized.");
         }
       }
    }
    else {
      // Alles andere (Logs) einfach verwerfen, damit der Puffer leer wird
      Serial.read();
    }
  }
}
