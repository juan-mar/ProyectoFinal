# Pruebas con Supabase

Esta carpeta reune scripts y consultas usados para validar la base de datos de Supabase del proyecto. Incluye tanto los archivos `.sql` que definen el esquema como los scripts de Python que ejercitan los endpoints REST y RPC.

- Los scripts asumen variables de entorno `SUPABASE_URL` y `SUPABASE_ANON`, cargadas desde un archivo `.env` en la raiz de esta carpeta. Ese archivo debe contener al menos:
  ```
  SUPABASE_URL=https://<tu-instancia>.supabase.co
  SUPABASE_ANON=eyJhbGciOiJIUzI1Ni...
  ```
- El archivo `.env` no se debe versionar porque expone credenciales sensibles.

## Scripts de Python

- `DB01_login.py` - Estado: OK. Funciono. Implementa el flujo de `password grant` contra Supabase Auth y devuelve el `access_token` para el usuario indicado.
- `DB02_updateUser.py` - Estado: OK. Funciono. Consume el RPC `promote_user` para cambiar el rol y datos del perfil; reutiliza el token obtenido desde `DB01`.
- `DB03_dogs.py` - Estado: OK. Funciono. Inserta y lista perros en la tabla `dogs`, normaliza campos opcionales y permite filtrar por activos.
- `DB04_addSessions.py` - Estado: OK. Funciono. Prueba los RPC `record_training` y `record_training_batch` para registrar entrenamientos individuales y en lote, incluyendo datos de duracion, resultado, condiciones y el dispositivo que captura la sesion.
- `DB05_readData.py` - Estado: OK. Funciono. Lee sesiones directamente desde la tabla `training_sessions`, resuelve UUIDs y permite consultas por perro o entrenador sin usar RPC.
- `DB06_readDataByDogRPC.py` - Estado: OK. Funciono. Consume el RPC `sessions_by_dog_code` para listar sesiones por codigo de perro con control de orden y limite.
- `DB07_readDataByTrainerRPC.py` - Estado: Pendiente de validar. Llama al RPC `sessions_by_trainer_email`; queda sin probar hasta contar con sesiones que ya tengan `trainer_id` asignado.
- `DB08_addDevice.py` - Estado: Pendiente de validar. Inserta dispositivos en la tabla `devices`, resolviendo los IDs de uploader y owner mediante el RPC `get_user_id_by_email`.
- `DB09_addTrainerToSession.py` - Estado: OK. Funciono. Obtiene el UUID del entrenador via `/auth/v1/user` y actualiza la sesion con un `PATCH` a `training_sessions`, usando un token con permisos (device para escribir, trainer o admin para leer/verificar).
- `supabasePoC.py` - Estado: OK. Funciono. Script base con requests directos (login, lecturas y escrituras simples) usado como PoC inicial de la API REST de Supabase.

## SQL

- `01_supabaseDB.sql` consolida la definicion del esquema (perfiles, dispositivos, perros, sesiones) y las politicas RLS asociadas. Ejecuta el script completo desde el editor SQL de Supabase para mantener la instancia sincronizada con los cambios.
