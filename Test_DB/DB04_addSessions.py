import json
import os
import requests
from dotenv import load_dotenv
from DB01_login import login


load_dotenv()
BASE = os.getenv('SUPABASE_URL')
ANON = os.getenv('SUPABASE_ANON')

DEVICE_TOKEN = "PEGAR_TOKEN_DEVICE"
DEVICE_CODE = "PEGAR_DEVICE_CODE"  # por ejemplo ESP32-001

def record_one(
        dog_code,
        started_at,
        duration_s,
        result,
        conditions,
        typ,
        device_code=DEVICE_CODE,
        co_trainer_id=None,
        device_token=DEVICE_TOKEN):
    url = f"{BASE}/rest/v1/rpc/record_training"
    payload = {
        "p_dog_code": dog_code,
        "p_started_at": started_at,
        "p_duration_s": duration_s,
        "p_result": result,
        "p_conditions": conditions,
        "p_type": typ,
        "p_device_code": device_code,
        "p_co_trainer_id": co_trainer_id,
    }
    response = requests.post(
        url,
        headers={
            "apikey": ANON,
            "Authorization": f"Bearer {device_token}",
            "Content-Type": "application/json",
            "Prefer": "return=representation",
        },
        json=payload,
        timeout=20,
    )
    print(response.status_code)
    try:
        print(json.dumps(response.json(), indent=2))
    except Exception:
        print(response.text)

def record_batch(sessions, device_token=DEVICE_TOKEN):
    if not isinstance(sessions, list):
        raise ValueError("sessions debe ser una lista de diccionarios")

    payload = []
    for index, session in enumerate(sessions, start=1):
        try:
            payload.append({
                "p_dog_code": session["dog_code"],
                "p_started_at": session["started_at"],
                "p_duration_s": session["duration_s"],
                "p_result": session.get("result"),
                "p_conditions": session.get("conditions"),
                "p_type": session.get("type"),
                "p_device_code": session["device_code"],
                "p_co_trainer_id": session.get("co_trainer_id"),
            })
        except KeyError as exc:
            raise KeyError(f"Falta la clave obligatoria {exc!s} en la sesion #{index}") from exc

    url = f"{BASE}/rest/v1/rpc/record_training_batch"
    response = requests.post(
        url,
        headers={
            "apikey": ANON,
            "Authorization": f"Bearer {device_token}",
            "Content-Type": "application/json",
        },
        json={"p_items": payload},
        timeout=30,
    )
    print(response.status_code)
    try:
        print(json.dumps(response.json(), indent=2))
    except Exception:
        print(response.text)


if __name__ == '__main__':
    device_token = login('lanzador_01@device.test', 'lanzador_01')
    device_code = 'ESP32-001'

    # Ejemplo individual (descomentar para probar la version simple)
    # record_one(
    #     dog_code='LUNA-002',
    #     started_at='2025-10-12T10:30:00Z',
    #     duration_s=33,
    #     result='success',
    #     conditions={'temp': 22.0, 'wind': 'SO 10km/h'},
    #     typ={'scent': 'Explosivos', 'mode': 'con distraccion de comida'},
    #     device_code=device_code,
    #     device_token=device_token,
    # )

    # Ejemplo batch usando record_training_batch(jsonb)
#    sessions = [
#        {
#            "dog_code": "NEWT-001",
#            "started_at": "2025-10-12T10:12:00Z",
#            "duration_s": 157,
#            "result": "success",
#            "conditions": {"temp": 17.1},
#            "type": {"scent": "Explosivos", "mode": "basico"},
#            "device_code": device_code,
#        },
#        {
#            "dog_code": "SIMON-01",
#            "started_at": "2025-10-12T10:35:00Z",
#            "duration_s": 420,
#            "result": "fail",
#            "conditions": {"temp": 25.0},
#            "type": {"scent": "Narcoticos", "mode": "ambiental"},
#            "device_code": device_code,
#            "co_trainer_id": None,
#        },
#    ]
#
#    record_batch(sessions, device_token=device_token)

#    sessions = [
#        {
#            "dog_code": "SIMON-01",
#            "started_at": "2025-10-12T10:17:00Z",
#            "duration_s": 111,
#            "result": "success",
#            "conditions": {"temp": 19.1},
#            "type": {"scent": "Narcoticos", "mode": "hard"},
#            "device_code": device_code,
#        },
#        {
#            "dog_code": "SIMON-01",
#            "started_at": "2025-10-12T10:57:00Z",
#            "duration_s": 222,
#            "result": "success",
#            "conditions": {"temp": 12.0},
#            "type": {"scent": "Narcoticos", "mode": "hard"},
#            "device_code": device_code,
#            "co_trainer_id": None,
#        },
#    ]
#
#    record_batch(sessions, device_token=device_token)

   