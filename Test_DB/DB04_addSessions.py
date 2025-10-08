import os
import requests
from dotenv import load_dotenv
from DB01_login import login

load_dotenv()
BASE = os.getenv('SUPABASE_URL')
ANON = os.getenv('SUPABASE_ANON')

def record_training(
    dog_code,
    started_at_iso,
    duration_s,
    result,
    conditions,
    typ,
    co_trainer_id=None,
    device_id=None,
    token=None,
):
    if token is None:
        raise ValueError('Necesitas pasar un access token valido en el parametro token')

    url = f"{BASE}/rest/v1/rpc/record_training"
    payload = {
        'p_dog_code': dog_code,
        'p_started_at': started_at_iso,
        'p_duration_s': duration_s,
        'p_result': result,
        'p_conditions': conditions,
        'p_type': typ,
        'p_co_trainer_id': co_trainer_id,
        'p_device_id': device_id,
    }

    r = requests.post(
        url,
        headers={
            'apikey': ANON,
            'Authorization': f'Bearer {token}',
            'Content-Type': 'application/json',
            'Prefer': 'return=representation',
        },
        json=payload,
        timeout=20,
    )
    print(r.status_code, r.text)


if __name__ == '__main__':
    trainer_token = login('trainer1@demo.test', 'trainer1234')
    
    # Ejemplo de uso de record_training, Ya cargado a la base de datos
    record_training(
        dog_code='FIRU-001',
        started_at_iso='2025-10-04T14:12:00Z',
        duration_s=320,
        result='success',
        conditions={'temp': 18.2, 'wind': 'NE 8km/h'},
        typ={'scent': 'Explosivos', 'mode': 'en linea'},
        device_id='esp32-campo-01',
        token=trainer_token
    )

    # Agregar otro registro
