import json
import os
import requests
from dotenv import load_dotenv
from DB01_login import login
from DB05_readData import get_user_id_from_token
from DB05_readData import sessions_by_dog_code_direct_access

load_dotenv()
BASE = os.getenv('SUPABASE_URL')
ANON = os.getenv('SUPABASE_ANON')

DEVICE_TOKEN = "PEGAR_TOKEN_DEVICE"
DEVICE_CODE = "PEGAR_DEVICE_CODE"  # por ejemplo ESP32-001

def assign_trainer(session_id, trainer_uuid, token):
    url = f"{BASE}/rest/v1/training_sessions"
    response = requests.patch(
        url,
        params={"id": f"eq.{session_id}"},
        headers={
            "apikey": ANON,
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Prefer": "return=representation",
        },
        json={"trainer_id": trainer_uuid},
        timeout=20,
    )
    response.raise_for_status()
    return response.json()


if __name__ == '__main__':
    device_token = login('lanzador_01@device.test', 'lanzador_01')
    device_code = 'ESP32-001'

    tr_token = login("trainer1@demo.test", "trainer1234")
    trainer_uuid = get_user_id_from_token(tr_token)
    
    sessions = sessions_by_dog_code_direct_access("NEWT-001", 3, tr_token)

    # Asignar un entrenador a una sesion creada sin entrenador
    response = assign_trainer(sessions[1]["id"], trainer_uuid, tr_token)
    print(response)