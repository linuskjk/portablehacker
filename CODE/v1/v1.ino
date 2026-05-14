#include <Wire.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>

// DISPLAY

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PINS

const int encoderPinA = 32;
const int encoderPinB = 33;
const int encoderButton = 25;
const int batteryPin = 34;

// ENCODER

ESP32Encoder encoder;

long lastEncoderPosition = 0;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const int debounceDelay = 50;

// BLUETOOTH

BluetoothSerial SerialBT;

// STATES

enum Screen {
  HOME_SCREEN,
  WIFI_SCREEN,
  BLUETOOTH_SCREEN,
  SETTINGS_SCREEN
};

Screen currentScreen = HOME_SCREEN;

// HOME MENU

String homeMenuItems[] = {
  "WiFi Scan",
  "Bluetooth",
  "Settings"
};

const int HOME_MENU_COUNT = 3;
int selectedHomeItem = 0;

// WIFI MENU

String wifiMenuItems[] = {
  "Start Scan",
  "Stop Scan",
  "Exit"
};

const int WIFI_MENU_COUNT = 3;
int selectedWifiItem = 0;

bool wifiScanning = false;

// BLUETOOTH MENU

String bluetoothMenuItems[] = {
  "Start Bluetooth",
  "Stop Bluetooth",
  "Exit"
};

const int BLUETOOTH_MENU_COUNT = 3;
int selectedBluetoothItem = 0;

bool bluetoothEnabled = false;

// SETTINGS MENU

String settingsMenuItems[] = {
  "Invert Display",
  "Show Raw Voltage",
  "Exit"
};

const int SETTINGS_MENU_COUNT = 3;
int selectedSettingsItem = 0;

bool invertDisplay = false;
bool showRawVoltage = true;

// SETUP

void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;

  encoder.attachHalfQuad(encoderPinA, encoderPinB);
  encoder.setCount(0);

  pinMode(encoderButton, INPUT_PULLUP);

  analogReadResolution(12);

  bootScreen();
}

// LOOP

void loop() {

  handleEncoder();
  handleButton();

  drawCurrentScreen();

  delay(30);
}

// BOOT SCREEN

void bootScreen() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(10, 20);
  display.println("Welcome to the");

  display.setCursor(5, 35);
  display.println("Portable Hacker");

  display.display();

  delay(2000);
}

// ENCODER

void handleEncoder() {

  long encoderPosition = encoder.getCount() / 2;

  if (encoderPosition != lastEncoderPosition) {

    bool turnedRight = encoderPosition > lastEncoderPosition;

    switch (currentScreen) {

      case HOME_SCREEN:

        if (turnedRight) {
          selectedHomeItem++;
        } else {
          selectedHomeItem--;
        }

        if (selectedHomeItem < 0) {
          selectedHomeItem = HOME_MENU_COUNT - 1;
        }

        if (selectedHomeItem >= HOME_MENU_COUNT) {
          selectedHomeItem = 0;
        }

        break;

      case WIFI_SCREEN:

        if (turnedRight) {
          selectedWifiItem++;
        } else {
          selectedWifiItem--;
        }

        if (selectedWifiItem < 0) {
          selectedWifiItem = WIFI_MENU_COUNT - 1;
        }

        if (selectedWifiItem >= WIFI_MENU_COUNT) {
          selectedWifiItem = 0;
        }

        break;

      case BLUETOOTH_SCREEN:

        if (turnedRight) {
          selectedBluetoothItem++;
        } else {
          selectedBluetoothItem--;
        }

        if (selectedBluetoothItem < 0) {
          selectedBluetoothItem = BLUETOOTH_MENU_COUNT - 1;
        }

        if (selectedBluetoothItem >= BLUETOOTH_MENU_COUNT) {
          selectedBluetoothItem = 0;
        }

        break;

      case SETTINGS_SCREEN:

        if (turnedRight) {
          selectedSettingsItem++;
        } else {
          selectedSettingsItem--;
        }

        if (selectedSettingsItem < 0) {
          selectedSettingsItem = SETTINGS_MENU_COUNT - 1;
        }

        if (selectedSettingsItem >= SETTINGS_MENU_COUNT) {
          selectedSettingsItem = 0;
        }

        break;
    }

    lastEncoderPosition = encoderPosition;
  }
}

// BUTTON

void handleButton() {

  bool buttonState = digitalRead(encoderButton);

  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (buttonState == LOW && lastButtonState == HIGH) {

      // HOME
      if (currentScreen == HOME_SCREEN) {

        switch (selectedHomeItem) {

          case 0:
            currentScreen = WIFI_SCREEN;
            break;

          case 1:
            currentScreen = BLUETOOTH_SCREEN;
            break;

          case 2:
            currentScreen = SETTINGS_SCREEN;
            break;
        }
      }

      // WIFI
      else if (currentScreen == WIFI_SCREEN) {

        switch (selectedWifiItem) {

          case 0:
            wifiScanning = true;
            WiFi.mode(WIFI_STA);
            WiFi.disconnect();
            break;

          case 1:
            wifiScanning = false;
            WiFi.scanDelete();
            break;

          case 2:
            currentScreen = HOME_SCREEN;
            break;
        }
      }

      // BLUETOOTH
      else if (currentScreen == BLUETOOTH_SCREEN) {

        switch (selectedBluetoothItem) {

          case 0:

            if (!bluetoothEnabled) {
              SerialBT.begin("PortableHacker");
              bluetoothEnabled = true;
            }

            break;

          case 1:

            if (bluetoothEnabled) {
              SerialBT.end();
              bluetoothEnabled = false;
            }

            break;

          case 2:
            currentScreen = HOME_SCREEN;
            break;
        }
      }

      // SETTINGS
      else if (currentScreen == SETTINGS_SCREEN) {

        switch (selectedSettingsItem) {

          case 0:

            invertDisplay = !invertDisplay;
            display.invertDisplay(invertDisplay);

            break;

          case 1:

            showRawVoltage = !showRawVoltage;

            break;

          case 2:

            currentScreen = HOME_SCREEN;

            break;
        }
      }
    }
  }

  lastButtonState = buttonState;
}

// DRAW CURRENT SCREEN

void drawCurrentScreen() {

  switch (currentScreen) {

    case HOME_SCREEN:
      drawHomeScreen();
      break;

    case WIFI_SCREEN:
      drawWifiScreen();
      break;

    case BLUETOOTH_SCREEN:
      drawBluetoothScreen();
      break;

    case SETTINGS_SCREEN:
      drawSettingsScreen();
      break;
  }
}

// BATTERY

float getBatteryVoltage() {

  int raw = analogRead(batteryPin);

  float voltage = (raw / 4095.0) * 3.3 * 2.0;

  return voltage;
}

void drawBatteryIcon() {

  float voltage = getBatteryVoltage();

  int batteryPercent = map(voltage * 100, 330, 420, 0, 100);

  if (batteryPercent > 100) batteryPercent = 100;
  if (batteryPercent < 0) batteryPercent = 0;

  display.drawRect(102, 0, 22, 10, SSD1306_WHITE);
  display.fillRect(124, 2, 2, 6, SSD1306_WHITE);

  int fillWidth = map(batteryPercent, 0, 100, 0, 18);

  display.fillRect(104, 2, fillWidth, 6, SSD1306_WHITE);
}

// HOME SCREEN

void drawHomeScreen() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("Portable Hacker");

  drawBatteryIcon();

  for (int i = 0; i < HOME_MENU_COUNT; i++) {

    display.setCursor(0, 18 + (i * 14));

    if (i == selectedHomeItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }

    display.println(homeMenuItems[i]);
  }

  display.display();
}

// WIFI SCREEN

void drawWifiScreen() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("WiFi Scanner");

  drawBatteryIcon();

  for (int i = 0; i < WIFI_MENU_COUNT; i++) {

    display.setCursor(0, 16 + (i * 12));

    if (i == selectedWifiItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }

    display.println(wifiMenuItems[i]);
  }

  if (wifiScanning) {

    int networks = WiFi.scanNetworks();

    display.setCursor(0, 52);

    display.print(networks);
    display.println(" networks");
  }

  display.display();
}

// BLUETOOTH SCREEN

void drawBluetoothScreen() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("Bluetooth");

  drawBatteryIcon();

  for (int i = 0; i < BLUETOOTH_MENU_COUNT; i++) {

    display.setCursor(0, 16 + (i * 12));

    if (i == selectedBluetoothItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }

    display.println(bluetoothMenuItems[i]);
  }

  display.setCursor(0, 52);

  if (bluetoothEnabled) {
    display.println("Bluetooth ON");
  } else {
    display.println("Bluetooth OFF");
  }

  display.display();
}

// SETTINGS SCREEN

void drawSettingsScreen() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("Settings");

  drawBatteryIcon();

  for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {

    display.setCursor(0, 16 + (i * 12));

    if (i == selectedSettingsItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }

    display.print(settingsMenuItems[i]);

    if (i == 0) {

      display.print(": ");

      if (invertDisplay) {
        display.print("ON");
      } else {
        display.print("OFF");
      }
    }

    if (i == 1) {

      display.print(": ");

      if (showRawVoltage) {
        display.print("ON");
      } else {
        display.print("OFF");
      }
    }
  }

  if (showRawVoltage) {

    float voltage = getBatteryVoltage();

    display.setCursor(0, 54);

    display.print(voltage, 2);
    display.println("V");
  }

  display.display();
}