# Portable Hacker

A portable security testing and development platform based on ESP32/ESP8266 microcontrollers.

## Project Structure

- **CODE/** - Firmware and application code
  - `ble_spam/` - BLE advertising spam tool
  - `captivePortal/` - Captive portal implementation
  - `deauth/` - WiFi deauthentication attacks
  - `handshake_*` - WiFi handshake capture utilities
  - `PCB/` - KiCad PCB design and libraries

- **CASE/** - 3D models and case design files
  - Various format outputs (3MF, F3D, STEP, STL, USDZ, OBJ)

- **PCB/** - PCB design files and component models

## Hardware

- Based on ESP32 DevKit V1
- OLED display integration
- Various sensor and module connectors

## Features

- WiFi security testing tools
- BLE scanning and exploitation
- Handshake capture and analysis
- Captive portal framework

## Building

Each code subdirectory contains its own build instructions. Refer to the specific README in each module for details.

## Case Design

3D case models are provided in multiple formats for compatibility with different design and manufacturing tools.

---

**Note:** This is a security research and educational tool. Use responsibly and only on networks/devices you own or have permission to test.
