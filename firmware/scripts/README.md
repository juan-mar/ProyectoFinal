# Scripts de soporte

Esta carpeta contiene utilidades para compresion de frontend, captura de RSSI por UART y visualizacion en tiempo real.

## Archivos

- `comprimir.py`
  - Comprime los archivos de `data_develop/` y genera `.gz` en `data/` para usar con el filesystem de PlatformIO.

- `read_rssi_uart_bin.py`
  - Pide un dump binario de RSSI por UART (comando `&`), lo decodifica y exporta a CSV.

- `rssi_uart_live.py`
  - Escucha un puerto COM en vivo, guarda datos en CSV y grafica en tiempo real los ultimos N puntos (por defecto, 100).

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

### 1) Captura binaria puntual (dump a CSV)

```powershell
python scripts/read_rssi_uart_bin.py --port COM6 --baud 115200 --output rssi_dump.csv
```

### 2) Captura y grafico en vivo (ultimos 100 puntos)

```powershell
python scripts/rssi_uart_live.py --port COM6 --baud 115200 --csv rssi_live.csv --window 100
```

## Notas

- Para salir del entorno virtual:

```powershell
deactivate
```
