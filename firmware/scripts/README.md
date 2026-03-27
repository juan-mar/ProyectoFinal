# Scripts de soporte

Esta carpeta contiene utilidades para compresion de frontend, captura de RSSI por UART y visualizacion en tiempo real.

## Archivos

- `comprimir.py`
  - Comprime los archivos de `data_develop/` y genera `.gz` en `data/` para usar con el filesystem de PlatformIO.

- `rssi_uart_live.py`
  - Escucha un puerto COM en vivo con lineas de protocolo UART tipo `DAT`, `EVT`, `CFG`.
  - Por defecto guarda en `scripts/rssi_csv` relativo a la ubicacion del script (no al directorio actual de ejecucion).
  - Separa muestras por modo y guarda por corrida en:
    - `scripts/rssi_csv/<YYYYmmdd_HHMMSS>/<YYYYmmdd_HHMMSS>_calib.csv`
    - `scripts/rssi_csv/<YYYYmmdd_HHMMSS>/<YYYYmmdd_HHMMSS>_train.csv`
    - `scripts/rssi_csv/<YYYYmmdd_HHMMSS>/<YYYYmmdd_HHMMSS>_cfg.csv`
    - `scripts/rssi_csv/<YYYYmmdd_HHMMSS>/<YYYYmmdd_HHMMSS>_evt.csv`
  - Guarda ademas muestras de modo desconocido en `scripts/rssi_csv/unknown_mode_latest.csv`.
    - Este archivo se sobreescribe en cada corrida del script.
  - Grafica en tiempo real `rssi_t1` vs `cmp_t1` y el filtro Python (`py_t1`).
  - Estima distancia (`py_dist_m`) a partir de `py_t1` (filtro Python) usando `media_calib` de `CFG` como RSSI de referencia a la distancia de calibracion indicada por comando.
  - Soporta filtro fallback (`--filter`) solo si llega linea legacy sin `cmp_t1`.

- `kalman_scalar.py`
  - Implementacion modular de Kalman 1D (random walk) con salida paso a paso de estados.

- `rssi_csv_postprocess.py`
  - Procesa CSV(s) de RSSI offline, aplica Kalman Python y genera dos graficos:
    - `RSSI sin filtrar`
    - `RSSI crudo vs CMP ESP vs Kalman Python`
  - Tambien genera un CSV procesado con columnas `raw_rssi`, `esp_cmp`, `python_filter`.
  - Opcionalmente convierte a distancia (`distance_m`) con control por comando:
    - habilitar/deshabilitar conversion
    - elegir fuente RSSI (`raw`, `python`, `esp`)
    - definir distancia de calibracion y exponente de perdida.

- `requirements.txt`
  - Dependencias Python congeladas (`pip freeze`) para este entorno.

## Requisitos

- Python 3.9+ (recomendado)
- Entorno virtual local: `.venv_firmware_tools`

## Instalacion de dependencias

Desde la raiz del proyecto:

```powershell
python -m venv .venv_firmware_tools
.\.venv_firmware_tools\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r scripts/requirements.txt
```

Si ya tenes el entorno creado, solo activa e instala:

```powershell
.\.venv_firmware_tools\Scripts\Activate.ps1
pip install -r scripts/requirements.txt
```

## Uso rapido

### 1) Captura y grafico en vivo (ultimos 100 puntos)

```powershell
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --window 100
```

Formato esperado de lineas UART (por posicion):

- `DAT,123,train,-67,-65.2`
- `EVT,125,IN`
- `CFG,0,2.0,9.0,-60.0,100.0,-70,16,-63,-68,OUT`

### 2) Captura en vivo con Kalman configurable

```powershell
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --filter kalman --window 120 --kalman-q 2.0 --kalman-r 9.0 --kalman-x0 -60 --kalman-p0 100
```

Si queres que el filtro Kalman Python use automaticamente los parametros enviados por firmware en lineas `CFG`, usa:

```powershell
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --filter kalman --kalman-source fw-cfg --window 120
```

En ese modo, los parametros `--kalman-q/--kalman-r/--kalman-x0/--kalman-p0` se usan como fallback hasta recibir el primer `CFG`.

Si calibraste a otra distancia distinta de 1 m, podes indicarlo por comando. El modelo usado es log-distance con exponente `n`:

```powershell
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --filter kalman --kalman-source fw-cfg --calibration-distance-m 2.5 --path-loss-exp 2.0
```

Con eso, `media_calib` se interpreta como RSSI de referencia a `2.5 m` (o el valor que indiques).

Columnas de cada CSV por modo:

- `pc_time_iso`, `millis`, `tipo`, `rssi_t1`, `cmp_t1`, `py_t1`, `py_dist_m`

Con eso podes graficar despues tanto la señal como la evolucion interna del filtro.

### 3) Postproceso offline de CSV (dos graficos)

```powershell
python scripts/rssi_csv_postprocess.py --input rssi_live.csv --kalman-q 2.0 --kalman-r 9.0 --kalman-x0 -60 --kalman-p0 100
```

Tambien podes procesar en lote toda la carpeta de corridas:

```powershell
python scripts/rssi_csv_postprocess.py --input scripts/rssi_csv --kalman-q 2.0 --kalman-r 9.0 --kalman-x0 -60 --kalman-p0 100

Con conversion a distancia activada (por defecto usa la salida del filtro Python):

```powershell
python scripts/rssi_csv_postprocess.py --input scripts/rssi_csv --distance-conversion on --distance-source python --calibration-distance-m 2.5 --path-loss-exp 2.0
```

Si no queres conversion, deja el default:

```powershell
python scripts/rssi_csv_postprocess.py --input scripts/rssi_csv --distance-conversion off
```
```

En modo carpeta, el script busca y procesa:

- `*_calib.csv`
- `*_train.csv`
- `unknown_mode_latest.csv`

Y genera los outputs al lado de cada CSV encontrado.

Salidas por defecto (en la misma carpeta del CSV de entrada):

- `<input>_raw.png`
- `<input>_raw_vs_kalman.png`
- `<input>_processed.csv`

## Notas

- Para salir del entorno virtual:

```powershell
deactivate
```
