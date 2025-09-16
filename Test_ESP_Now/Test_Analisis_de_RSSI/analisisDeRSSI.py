import serial
import serial.tools.list_ports
import time

# Helper para listar puertos disponibles
def listar_puertos():
    puertos = serial.tools.list_ports.comports()
    if not puertos:
        print("No se detectaron puertos seriales.")
    for puerto in puertos:
        print(f"{puerto.device} - {puerto.description}")

def main():
    listar_puertos()  # Opcional, para elegir el puerto correcto

    # Sustituye 'COM5' por el puerto donde está tu ESP32
    puerto = "COM5"
    baudrate = 115200  # Ajusta si tu firmware usa otra velocidad

    try:
        with serial.Serial(port=puerto, baudrate=baudrate, timeout=1) as ser:
            print(f"Escuchando en {puerto} a {baudrate} baudios...")
            while True:
                linea = ser.readline().decode(errors="ignore").strip()
                if not linea:
                    continue

                print(f"Mensaje recibido: {linea}")

                # Si el ESP32 manda algo tipo "RSSI:-67", podemos extraerlo:
                if "RSSI" in linea:
                    try:
                        etiqueta, valor = linea.split(":", 1)
                        rssi = int(valor)
                        print(f"RSSI recibido: {rssi} dBm")
                    except ValueError:
                        print("Formato de RSSI inesperado; línea completa:", linea)

                time.sleep(0.01)
    except serial.SerialException as err:
        print(f"No se pudo abrir el puerto {puerto}: {err}")

if __name__ == "__main__":
    main()
