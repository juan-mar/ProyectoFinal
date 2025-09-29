import os, json, requests
from dotenv import load_dotenv

load_dotenv()

SUPABASE_URL = os.getenv("SUPABASE_URL")
SUPABASE_ANON = os.getenv("SUPABASE_ANON")

assert SUPABASE_URL and SUPABASE_ANON, "Faltan SUPABASE_URL o SUPABASE_ANON en .env"

headers = {
    "apikey": SUPABASE_ANON,
    "Authorization": f"Bearer {SUPABASE_ANON}",
    "Content-Type": "application/json",
    "Prefer": "return=representation"   # que devuelva la fila insertada
}

def insert_ping(device_id: str, value: float):
    url = f"{SUPABASE_URL}/rest/v1/pings"
    payload = {"device_id": device_id, "value": value}
    r = requests.post(url, headers=headers, data=json.dumps(payload), timeout=20)
    r.raise_for_status()
    return r.json()

def get_last_pings(device_id: str, limit=5):
    url = f"{SUPABASE_URL}/rest/v1/pings"
    params = {
        "select": "*",
        "device_id": f"eq.{device_id}",
        "order": "created_at.desc",
        "limit": str(limit)
    }
    r = requests.get(url, headers=headers, params=params, timeout=20)
    r.raise_for_status()
    return r.json()

if __name__ == "__main__":
    print("Insertando...")
    row = insert_ping("esp32-dev-01", 42.5)
    print("Insert OK:", row)

    print("Leyendo últimos...")
    rows = get_last_pings("esp32-dev-01", limit=5)
    print("Rows:", rows)
