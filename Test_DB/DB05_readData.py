import json
import os
import uuid

import requests
from dotenv import load_dotenv

from DB01_login import login

load_dotenv()
BASE = os.getenv("SUPABASE_URL")
ANON = os.getenv("SUPABASE_ANON")

TOKEN = "PEGA_AQUI_TU_ACCESS_TOKEN"  # guest/trainer/admin
def _get_uuid_from_trainer(token):
    """Get the UUID of the user associated with the given token."""
    url = f"{BASE}/rest/v1/users"
    params = {
        "select": "id",
        "limit": "1",
    }
    response = requests.get(
        url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}"},
        params=params,
        timeout=20,
    )
    response.raise_for_status()
    data = response.json()
    if not data:
        raise ValueError("Token no valido")
    return data[0]["id"]

def get_user_id_from_token(token):
    url = f"{BASE}/auth/v1/user"
    resp = requests.get(
        url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}"},
        timeout=20,
    )
    resp.raise_for_status()
    return resp.json()["id"]


def _dog_uuid_from_code(dog_code, token):
    """Resolve a human readable dog_code to its UUID primary key."""
    url = f"{BASE}/rest/v1/dogs"
    params = {
        "select": "id,dog_code",
        "dog_code": f"eq.{dog_code}",
        "limit": "1",
    }
    response = requests.get(
        url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}"},
        params=params,
        timeout=20,
    )
    response.raise_for_status()
    data = response.json()
    if not data:
        raise ValueError(f"No existe un perro con codigo {dog_code!r}")
    return data[0]["id"]

# Listar sesiones por perro, usando su UUID, si no lo tiene entonces lo consulta. Accede directo a la tabla training_sessions
def sessions_by_dog_code_direct_access(dog_reference, limit=50, token=TOKEN):
    try:
        uuid.UUID(str(dog_reference))
        dog_id = str(dog_reference)
    except (ValueError, TypeError):
        dog_id = _dog_uuid_from_code(dog_reference, token)

    url = f"{BASE}/rest/v1/training_sessions"
    params = {
        "select": "*,dogs(name,dog_code)",
        "dog_id": f"eq.{dog_id}",
        "order": "started_at.desc",
        "limit": str(limit),
    }
    response = requests.get(
        url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}"},
        params=params,
        timeout=20,
    )
    print(response.status_code)
    response.raise_for_status()
    print(json.dumps(response.json(), indent=2))
    return response.json()

# Listar sesiones por entrenador, usando su email. Accede directo a la tabla training_sessions
def sessions_by_trainer(trainer_id, limit=50, token=TOKEN):
    url = f"{BASE}/rest/v1/training_sessions"
    params = {
        "select": "*,dogs(name,dog_code)",
        "trainer_id": f"eq.{trainer_id}",
        "order": "started_at.desc",
        "limit": str(limit),
    }
    response = requests.get(
        url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}"},
        params=params,
        timeout=20,
    )
    print(response.status_code)
    response.raise_for_status()
    print(json.dumps(response.json(), indent=2))


if __name__ == "__main__":
    # Primero obten el access_token con login() y pegalo en TOKEN mas arriba
    tr_token = login("trainer1@demo.test", "trainer1234")
    gs_token = login("guest2@demo.test", "guest2")

    # Completa uno de estos y comenta el otro:
    sessions_by_dog_code_direct_access("SIMON-01", limit=20, token=gs_token)
