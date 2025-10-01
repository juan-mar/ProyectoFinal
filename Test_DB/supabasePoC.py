import os, json, requests
from datetime import datetime
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

def insert_multiple(pings: list):
    url = f"{SUPABASE_URL}/rest/v1/pings"
    r = requests.post(url, headers=headers, data=json.dumps(pings), timeout=20)
    r.raise_for_status()
    return r.json()

def patch_ping(ping_id: str, new_value: float):
    url = f"{SUPABASE_URL}/rest/v1/pings?id=eq.{ping_id}"
    payload = {"value": new_value}

    try:
        r = requests.patch(
            url,
            headers={**headers, "Content-Type": "application/json", "Prefer":"return=representation"},
            json=payload,
            timeout=20
        )
        r.raise_for_status()  # lanza excepción si no es 2xx
        return r.json() if r.text else {}
    except requests.HTTPError as e:
        print("Error en PATCH")
        print("Status code:", e.response.status_code)
        print("Respuesta del servidor:", e.response.text)
        raise  # vuelve a lanzar la excepción por si querés que corte la ejecución

def patch_ping_wrong_field(ping_id):
    url = f"{SUPABASE_URL}/rest/v1/pings?id=eq.{ping_id}"
    payload = {"wrong_field": "foo"}  # este campo no existe en la tabla
    r = requests.patch(url, headers={**headers, "Content-Type":"application/json"}, json=payload, timeout=20)
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

def get_last(device_id):
    params = {"select":"*", "device_id":f"eq.{device_id}", "order":"created_at.desc", "limit":"1"}
    r = requests.get(f"{SUPABASE_URL}/rest/v1/pings", headers=headers, params=params)
    r.raise_for_status()
    return r.json()[0] if r.json() else None


if __name__ == "__main__":
    #print("Insertando...")
    #row = insert_ping("esp32-dev-01", 20)
    #print("Insert OK:", row)
    #print("Insertando...")
    #row = insert_ping("esp32-dev-01", 30)
    #print("Insert OK:", row)
    #print("Insertando...")
    #row = insert_ping("esp32-dev-01", 50)
    #print("Insert OK:", row)
    #my_id = row[0]["id"]
    #print("ID guardado:", my_id)    
#
    #print("Leyendo últimos...")
    #rows = get_last_pings("esp32-dev-01", limit=5)
    #print("Rows:", rows)

    # Crear lista de múltiples mediciones para esp32-dev-02
#    mediciones_dev02 = [
#        {"device_id": "esp32-dev-02", "value": 25.5},
#        {"device_id": "esp32-dev-02", "value": 30.2},
#        {"device_id": "esp32-dev-02", "value": 28.7},
#        {"device_id": "esp32-dev-02", "value": 32.1},
#        {"device_id": "esp32-dev-02", "value": 27.9}
#    ]
#    
#    print("Insertando múltiples mediciones para esp32-dev-02...")
#    rows = insert_multiple(mediciones_dev02)
#    print(f"Insert múltiple OK: {len(rows)} filas insertadas")
#    
#    print("Leyendo último de esp32-dev-01...")
#    row = get_last("esp32-dev-01")
#    print("Last esp32-dev-01:", row)
#    
    print("Leyendo últimas de esp32-dev-02...")
    rows = get_last_pings("esp32-dev-02", limit=2)
    print("Last esp32-dev-02:", rows)

    if rows:
        first_ping_id = rows[0]['id']
        #first_ping_id = "11111111-2222-3333-4444-555555555555"
        print(f"Patching ping ID {first_ping_id} to new value 99.9")
        try:
            patched = patch_ping_wrong_field(first_ping_id)
            print("Patched:", patched)
        except Exception as e:
            print("Se produjo una excepción:", e)