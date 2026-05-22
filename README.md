# Portable Hacker

Portable Hacker is a handheld ESP32-based network utility device with a custom PCB, OLED display, rotary encoder navigation, battery monitoring, WiFi scanning, Bluetooth scanning, and a easy to adapt firmware UI.
<img width="2250" height="3450" alt="portablehacker_zine_v2" src="https://github.com/user-attachments/assets/d89e4207-22be-45c8-999d-ab69f8de36cb" />

## The project is designed to be:
- portable
- modular
- expandable
- easy to modify
- beginner-friendly Hardware

### Why does this exist?
- I created this project to learn how to design PCBs and how to implement communication between microcontrollers.

# Things you need

 - [ESP32 preferably with antenna support](https://de.aliexpress.com/item/1005006140555903.html)
 - [ESP8266 NodeMCU V3](https://de.aliexpress.com/item/1005006889833004.html)
 - [0.91" Oled display (I2C)](https://de.aliexpress.com/item/1005011907100931.html)
 - [Rotary encoder](https://no.rs-online.com/web/p/mechanical-rotary-encoders/1675389)
 - [PCB]()
 - [2x100k Ohm Resistors](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_100_kohm_0207_0_6_w_1_-11458)
 - [1x0.1uF capacitor](https://funduinoshop.com/bauelemente/passive-bauelemente/kondensatoren/keramikkondensator-0.1uf-104pf-50v)


# Tools you need
- Soldering Iron
- Solder wire
- 3D-Printer




# Installation

1. Assemble the PCB
2. Solder all components
3. Connect the OLED display
4. Connect the battery
5. Upload the firmware by uploading v1.ino to the esp32 and the handshakeV2_esp8266.ino to the ESP8266 you can do both by using Arduino IDE
6. Power on the device

  

# Features

- ESP32 powered
- OLED UI with menu system
- Rotary encoder navigation
- Battery voltage monitoring
- WiFi network scanner
- Bluetooth scanner
- WiFi deauther
- Bluetooth Spammer
- Modular firmware architecture
- Rechargeable LiPo battery support
- Custom PCB
- 3D printable enclosure support


# Hardware

## Required Components

| Component | Description |
|---|---|
| ESP32 Dev Board | Preferably with external antenna support |
| 0.91\" OLED Display | I2C SSD1306 |
| Rotary Encoder | With push button |
| LiPo Battery | 3.7V rechargeable |
| TP4056 | LiPo charging module |
| 2x 100kΩ Resistors | Battery voltage divider |
| 0.1µF Capacitor | ADC smoothing |
| Custom PCB | Designed in KiCad |



# Battery Measurement Circuit

The battery voltage is measured using a voltage divider connected to GPIO34.

```text
VBAT ── R1 ──+── GPIO34
             |
             +── C1 ── GND
             |
             R2
             |
            GND
```
## Values

| Component | Value |
|---|---|
| R1 | 100kΩ |
| R2 | 100kΩ |
| C1 | 0.1µF |
# Pin Layout

## OLED Display

| OLED Pin | ESP32 Pin |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3V |
| GND | GND |

---

## Rotary Encoder

| Encoder Pin | ESP32 Pin |
|---|---|
| A | GPIO32 |
| B | GPIO33 |
| SW | GPIO25 |
| + | 3.3V |
| GND | GND |

---

## Battery Monitoring

| Signal | ESP32 Pin |
|---|---|
| BAT_SENSE | GPIO34 |


# Images
<img width="1059" height="715" alt="image" src="https://github.com/user-attachments/assets/0374bb60-89d3-4ded-9852-709c38e33cfb" />

<img width="500" height="380" alt="all" src="https://github.com/user-attachments/assets/e6787641-1f35-4863-b597-963330568270" />
<img width="800" height="500" alt="PCB_botton" src="https://github.com/user-attachments/assets/8722b195-f6f3-40ca-a0be-ca86b06d9a85" />
<img width="800" height="500" alt="PCB_top" src="https://github.com/user-attachments/assets/f06ebb7f-a434-4ea0-8a82-d48e4f9d0af6" />
<img width="800" height="400" alt="Case_snap_together" src="https://github.com/user-attachments/assets/849a8647-68e8-45dd-acf2-459302a06cb2" />





## Authors

- [@linuskjk](https://www.github.com/linuskjk)


## License

[![MIT License](https://img.shields.io/badge/License-MIT-green.svg)](https://choosealicense.com/licenses/mit/)
