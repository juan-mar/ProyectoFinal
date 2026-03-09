import os
import gzip
import shutil

origen = 'data_develop'
destino = 'data'

# 1. Creamos la carpeta 'data' si no existe
if not os.path.exists(destino):
    os.makedirs(destino)

# 2. Limpiamos archivos viejos en 'data' para no acumular basura
for f in os.listdir(destino):
    ruta_f = os.path.join(destino, f)
    if os.path.isfile(ruta_f):
        os.remove(ruta_f)

# 3. Comprimimos todo lo que haya en data_develop
for archivo in os.listdir(origen):
    ruta_origen = os.path.join(origen, archivo)
    
    # Solo comprimimos si es un archivo (ignoramos subcarpetas)
    if os.path.isfile(ruta_origen):
        # Le agregamos la extensión .gz
        ruta_destino = os.path.join(destino, archivo + '.gz')
        
        with open(ruta_origen, 'rb') as f_in:
            with gzip.open(ruta_destino, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)
                
        print(f"Comprimido: {archivo} -> {archivo}.gz")

print("\n¡Listo! Archivos actualizados. Ya puedes hacer el 'Upload Filesystem Image' en PlatformIO.")