import csv
import numpy as np
import matplotlib.pyplot as plt

# ----------------------------
# Leer CSV y extraer segunda columna
# ----------------------------
archivo = "rssi_log.csv"
columna2 = []

with open(archivo, newline='', encoding='utf-8') as f:
    lector = csv.reader(f)
    next(lector)  # descomentar si el CSV tiene encabezado
    for fila in lector:
        if len(fila) >= 2:
            try:
                columna2.append(float(fila[1]))
            except ValueError:
                pass

datos = np.array(columna2)

# ----------------------------
# Implementación tipo ESP32
# ----------------------------
Q = 0.001
R = 2.3
x_est = -50.0
P = 1.0

def kalmanUpdate(medida):
    global x_est, P, Q, R

    # Predicción
    x_pred = x_est
    P_pred = P + Q

    # Ganancia de Kalman
    K = P_pred / (P_pred + R)

    # Actualización
    x_est = x_pred + K * (medida - x_pred)
    P = (1 - K) * P_pred

    return x_est

# Aplicar filtro con la función estilo ESP32
datos_filtrados_func = [kalmanUpdate(m) for m in datos]

# ----------------------------
# Implementación vectorizada (numpy) para comparar
# ----------------------------
def kalman_filter_numpy(data, Q=0.001, R=2.3):
    n = len(data)
    x_est = np.zeros(n)
    P = np.zeros(n)

    x_est[0] = data[0]
    P[0] = 1.0

    for k in range(1, n):
        x_pred = x_est[k-1]
        P_pred = P[k-1] + Q

        K = P_pred / (P_pred + R)
        x_est[k] = x_pred + K * (data[k] - x_pred)
        P[k] = (1 - K) * P_pred

    return x_est

datos_filtrados_numpy = kalman_filter_numpy(datos)

# ----------------------------
# Graficar
# ----------------------------
plt.figure(figsize=(12,6))
plt.plot(datos, label="Datos originales", alpha=0.5)
plt.plot(datos_filtrados_func, label="Kalman estilo ESP32", linewidth=2)
plt.plot(datos_filtrados_numpy, "--", label="Kalman vectorizado numpy")
plt.legend()
plt.title("Comparación: Datos originales vs Kalman (ESP32 vs numpy)")
plt.xlabel("Índice de muestra")
plt.ylabel("Valor")
plt.grid(True)
plt.show()

