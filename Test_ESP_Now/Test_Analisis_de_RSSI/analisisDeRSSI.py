import time
from pathlib import Path

import pandas as pd
import serial

PUERTO = "COM4"
BAUDRATE = 115200
SALIDA_CSV = Path("rssi_log.csv")
BLOQUE = 100  # cuántas filas acumular antes de escribir

def guardar_bloque(rows):
    df = pd.DataFrame(rows, columns=["timestamp", "rssi"])
    df.to_csv(
        SALIDA_CSV,
        mode="a",
        header=not SALIDA_CSV.exists(),  # escribe encabezado solo la primera vez
        index=False,
    )

def main():
    buffer = []
    timerInit = time.time()

    with serial.Serial(PUERTO, BAUDRATE, timeout=1) as ser:
        print(f"Escuchando {PUERTO}...")
        while True:
            linea = ser.readline().decode(errors="ignore").strip()
            if not linea:
                continue

            print(f"Mensaje recibido: {linea}")
            rssi = None

            if "RSSI" in linea:
                try:
                    _, valor = linea.split(":", 1)
                    rssi = int(valor)
                    print(f"RSSI recibido: {rssi} dBm")
                except ValueError:
                    print("Formato de RSSI inesperado:", linea)

            buffer.append((time.time()-timerInit, rssi))

            if len(buffer) >= BLOQUE:
                guardar_bloque(buffer)
                buffer.clear()

            time.sleep(0.01)

if __name__ == "__main__":
    try:
        main()
    finally:
        # Escribe lo que quede pendiente si sales con Ctrl+C
        if 'buffer' in locals() and buffer:
            guardar_bloque(buffer)