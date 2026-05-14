# Portable Hacker

Portable Hacker is a handheld ESP32-based network utility device with a custom PCB, OLED display, rotary encoder navigation, battery monitoring, WiFi scanning, Bluetooth scanning, and a modular firmware UI.

The project is designed to be:
- portable
- modular
- expandable
- easy to modify
- beginner-friendly Hardware

## Things you need

 - [ESP32 preferably wwith antenna support](https://awesomeopensource.com/project/elangosundar/awesome-README-templates)
 - [91" Oled display (I2C)](https://github.com/matiassingers/awesome-readme)
 - [Rotary encoder](https://bulldogjob.com/news/449-how-to-write-a-good-readme-for-your-github-project)
  - [PCB](https://bulldogjob.com/news/449-how-to-write-a-good-readme-for-your-github-project)


# Tools you need
- Soldering Iron
- Solder wire
- 3D-Printer




# Installation

1. Assemble the PCB
2. Solder all components
3. Connect the OLED display
4. Connect the battery
5. Upload the firmware
6. Power on the device# Features

- ESP32 powered
- OLED UI with menu system
- Rotary encoder navigation
- Battery voltage monitoring
- WiFi network scanner
- Bluetooth scanner
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
| BAT_SENSE | GPIO34 |# Future Plans

- BLE device details
- SD card logging
- Packet monitoring
- RGB status LEDs
- Better UI animations
- Battery percentage calibration
- Sleep mode
- USB-C charging
- Fully custom ESP32 PCB



## Authors

- [@linuskjk](https://www.github.com/linuskjk)


## License

[![MIT License](https://img.shields.io/badge/License-MIT-green.svg)](https://choosealicense.com/licenses/mit/)
