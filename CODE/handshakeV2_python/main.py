import serial, struct, time, sys
import os

PORT = 'COM24'
BAUD = 921600
FILENAME = "handshake.pcap"

def run():
    ser = None
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        time.sleep(2)
        ser.write(b"SCAN\n")
        
        nets = []
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("NET:"):
                p = line.replace("NET:", "").split("|")
                nets.append(p)
                print(f"[{p[0]}] SSID: {p[1]} (Ch: {p[2]})")
            if "SCAN_DONE" in line: break

        if not nets: return
        idx = input("\nWLAN Nummer: ")
        
        target_ch = nets[int(idx)][2]
        
        ser.write(f"START:{target_ch}\n".encode())
        print(f"Warte auf Handshake auf Kanal {target_ch}...")

        with open(FILENAME, "wb") as f:
            f.write(struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 105))
            f.flush()
            count = 0
            while True:
                size_data = ser.read(2)
                if len(size_data) < 2: continue
                
                size = struct.unpack("<H", size_data)[0]
                if 0 < size < 2500:
                    pkt = ser.read(size)
                    if len(pkt) == size:
                        clean_pkt = pkt[12:] # ESP-Header weg
                        c_size = len(clean_pkt)
                        t = time.time()
                        
                        # Paket schreiben
                        f.write(struct.pack("<IIII", int(t), int((t-int(t))*1000000), c_size, c_size))
                        f.write(clean_pkt)
                        
                        # HARD FLUSH: Zwingt Windows, die Datei sofort zu aktualisieren
                        f.flush()
                        os.fsync(f.fileno()) 
                        
                        count += 1
                        print(f"[!] Paket {count} gesichert und auf Disk geschrieben!")
    except KeyboardInterrupt:
        print(f"\nFertig. {count} Pakete in {FILENAME}")
    except Exception as e:
        print(f"\nFehler: {e}")
    finally:
        if ser: ser.close()

if __name__ == "__main__": run()
