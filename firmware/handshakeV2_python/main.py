import argparse
import os
import struct
import sys
import time

import serial
from serial.tools import list_ports

BAUD = 921600
FILENAME = "handshake.pcap"


def resolve_port(explicit_port):
    if explicit_port:
        return explicit_port

    env_port = os.environ.get("PORT")
    if env_port:
        return env_port

    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found. Pass --port or set PORT.")

    if len(ports) == 1:
        return ports[0].device

    print("Available serial ports:")
    for index, port in enumerate(ports):
        description = port.description or "Unknown device"
        print(f"  [{index}] {port.device} - {description}")

    while True:
        choice = input("Select port number: ").strip()
        if choice.isdigit() and int(choice) < len(ports):
            return ports[int(choice)].device


def parse_network_line(line):
    payload = line.replace("NET:", "", 1)
    parts = payload.split("|")
    if len(parts) < 3:
        return None
    net_id, ssid, channel = parts[:3]
    mac = parts[3] if len(parts) > 3 else ""
    return {
        "id": net_id,
        "ssid": ssid,
        "channel": channel,
        "mac": mac,
    }

def run():
    parser = argparse.ArgumentParser(description="Capture handshake packets from the ESP8266 helper.")
    parser.add_argument("--port", help="Serial port for the ESP8266 (for example COM5 or /dev/ttyUSB0).")
    parser.add_argument("--output", default=FILENAME, help="PCAP file to write.")
    args = parser.parse_args()

    ser = None
    count = 0
    try:
        port = resolve_port(args.port)
        ser = serial.Serial(port, BAUD, timeout=1)
        time.sleep(2)
        ser.reset_input_buffer()
        ser.write(b"SCAN\n")
        
        nets = []
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("NET:"):
                net = parse_network_line(line)
                if net:
                    nets.append(net)
                    extra = f", MAC: {net['mac']}" if net["mac"] else ""
                    print(f"[{net['id']}] SSID: {net['ssid']} (Ch: {net['channel']}{extra})")
            if "SCAN_DONE" in line:
                break

        if not nets:
            print("No networks returned by the ESP8266 helper.")
            return

        idx = input("\nWLAN Nummer: ")

        target_ch = nets[int(idx)]["channel"]
        
        ser.write(f"START:{target_ch}\n".encode())
        print(f"Warte auf Handshake auf Kanal {target_ch}...")

        with open(args.output, "wb") as f:
            f.write(struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 105))
            f.flush()
            while True:
                size_data = ser.read(2)
                if len(size_data) < 2:
                    continue
                
                size = struct.unpack("<H", size_data)[0]
                if 0 < size < 2500:
                    pkt = ser.read(size)
                    if len(pkt) == size:
                        clean_pkt = pkt[12:]
                        c_size = len(clean_pkt)
                        t = time.time()
                        
                        f.write(struct.pack("<IIII", int(t), int((t-int(t))*1000000), c_size, c_size))
                        f.write(clean_pkt)
                        
                        f.flush()
                        os.fsync(f.fileno())
                        
                        count += 1
                        print(f"[!] Paket {count} gesichert und auf Disk geschrieben!")
    except KeyboardInterrupt:
        print(f"\nFertig. {count} Pakete in {args.output}")
    except Exception as e:
        print(f"\nFehler: {e}")
    finally:
        if ser: ser.close()

if __name__ == "__main__":
    run()
