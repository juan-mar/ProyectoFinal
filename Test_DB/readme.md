# Pruebas con Supabase

Esta carpeta reune scripts y consultas usados para validar la base de datos de Supabase del proyecto. Incluye tanto los archivos `.sql` que definen el esquema como los scripts de Python que ejercitan los endpoints REST y RPC.

- Los scripts asumen variables de entorno `SUPABASE_URL` y `SUPABASE_ANON`, cargadas desde un archivo `.env` en la raiz de esta carpeta. Ese archivo debe contener al menos:
  ```
  SUPABASE_URL=https://<tu-instancia>.supabase.co
  SUPABASE_ANON=eyJhbGciOiJIUzI1Ni...
  ```
- El archivo `.env` no se debe versionar porque expone credenciales sensibles.

## Scripts de Python

- `DB01_login.py` - Estado: OK. Probado. Implementa el flujo de `password grant` contra Supabase Auth y devuelve el `access_token` para el usuario indicado.
- `DB02_updateUser.py` - Estado: OK. Probado. Aplica el RPC `promote_user` para cambiar el rol y datos del perfil; reutiliza el token obtenido desde `DB01`. El token debe ser de usuario admin.
- `DB03_dogs.py` - Estado: OK. Probado. Inserta y lista perros en la tabla `dogs`, normaliza campos opcionales y permite filtrar por activos. Para modificar la tabla dogs el token debe ser de usuario admin.
- `DB04_addSessions.py` - Estado: OK. Probado. Prueba los RPC `record_training` y `record_training_batch` para registrar entrenamientos individuales y en grupo, incluyendo datos de duracion, resultado, condiciones y el dispositivo que captura la sesion. Necesita el token de un dispositivo previamente agregado a la tabla `devices`.
- `DB05_readData.py` - Estado: OK. Probado. Lee sesiones directamente desde la tabla `training_sessions`, resuelve UUIDs y permite consultas por perro o entrenador sin usar RPC. Necesita token de guest/trainer/admin.
- `DB06_readDataByDogRPC.py` - Estado: OK. Probado. Aplica el RPC `sessions_by_dog_code` para listar sesiones por codigo de perro con control de orden y limite. Necesita token de guest/trainer/admin.
- `DB07_readDataByTrainerRPC.py` - Estado: Pendiente de validar. Llama al RPC `sessions_by_trainer_email`; queda sin probar hasta contar con sesiones que ya tengan `trainer_id` asignado.
- `DB08_addDevice.py` - Estado: OK. Probado. Inserta dispositivos en la tabla `devices`, resolviendo los IDs del devices previamente en tabla Auth y del owner mediante el RPC `get_user_id_by_email`. Solo el token del admin permite esta operacion.
- `DB09_addTrainerToSession.py` - Estado: OK. Probado. Obtiene el UUID del entrenador via `/auth/v1/user` con la funcion `get_user_id_from_token` del archivo `DB05` y actualiza la sesion con un `PATCH` a `training_sessions`, usando el token del entrenador a cargar a la tabla. Para buscar las sessiones se usa la funcion `sessions_by_dog_code_direct_access` tambien del archivo `DB05`.
- `supabasePoC.py` - Estado: OK. Probado. Script base con requests directos (login, lecturas y escrituras simples) usado como PoC inicial de la API REST de Supabase.

## SQL

- `00_supabasePoC.sql` - Script inicial para pruebas rápidas de la base de datos con una tabla `pings`. Incluye ejemplos básicos de inserción, consulta y verificación de tablas principales, útil para validar conectividad y operaciones antes de definir el esquema final.
- `01_supabaseDB.sql` consolida la definicion del esquema (perfiles, dispositivos, perros, sesiones) y las politicas RLS asociadas. Ejecuta el script completo desde el editor SQL de Supabase para mantener la instancia sincronizada con los cambios.

## Requirements
El archivo `requirements.txt` lista las dependencias de Python necesarias para ejecutar los scripts de esta carpeta. Incluye paquetes como `requests`, `python-dotenv` y otros requeridos para interactuar con la API de Supabase y manejar variables de entorno. Instala los paquetes con:

```bash
pip install -r requirements.txt
```
