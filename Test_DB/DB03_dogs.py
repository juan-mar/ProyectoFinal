import os, requests, json
from dotenv import load_dotenv
from DB01_login import login
load_dotenv()
BASE = os.getenv("SUPABASE_URL"); ANON = os.getenv("SUPABASE_ANON")

ADMIN_TOKEN = "PEGÁ_ACÁ_TU_ACCESS_TOKEN"

def add_dog(dog_code, name, breed=None, sex=None, birthdate=None, unit=None, notes=None, token=ADMIN_TOKEN):
    url = f"{BASE}/rest/v1/dogs"
    row = {"dog_code": dog_code, "name": name, "breed": breed, "sex": sex,
           "birthdate": birthdate, "unit": unit, "notes": notes, "active": True}
    row = {k:v for k,v in row.items() if v is not None}
    r = requests.post(url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}",
                 "Content-Type":"application/json", "Prefer":"return=representation"},
        json=row, timeout=20)
    print(r.status_code, r.text)

def list_dogs(token=ADMIN_TOKEN, only_active=True, limit=100):
    url = f"{BASE}/rest/v1/dogs"
    params = {"select":"*", "order":"created_at.desc", "limit": str(limit)}
    if only_active: params["active"] = "eq.true"
    r = requests.get(url, headers={"apikey": ANON, "Authorization": f"Bearer {token}"}, params=params, timeout=20)
    print(r.status_code); print(json.dumps(r.json(), indent=2))

if __name__ == "__main__":
    ad_token = login("lanzador_01@device.test", "lanzador_01")    
    #Ya agregados
    #add_dog("FIRU-001", "Firulais", breed="Labrador", sex="M", unit="K9", token=ad_token)
    #add_dog("LUNA-002", "Luna", breed="Pastor Alemán", sex="F", unit="K9", token=ad_token)
    #add_dog("NEWT-001", "Newton", breed="Boyero de Berna", sex="M", token=ad_token)
    #add_dog("SIMON-01", "Simón", breed="Boyero de Berna", sex="M", token=ad_token)

    #Agregar nuevos
    

    list_dogs(token = ad_token)
