import os, requests, json
from dotenv import load_dotenv
from DB01_login import login
load_dotenv()
BASE=os.getenv("SUPABASE_URL"); ANON=os.getenv("SUPABASE_ANON")

ADMIN_TOKEN="PEGAR_TOKEN_ADMIN"

def get_user_id_by_email(email, admin_token=ADMIN_TOKEN):
    url=f"{BASE}/rest/v1/rpc/get_user_id_by_email"
    r=requests.post(url,
        headers={"apikey":ANON,"Authorization":f"Bearer {admin_token}",
                 "Content-Type":"application/json"},
        json={"p_email":email}, timeout=20)
    r.raise_for_status()
    return r.json()  # uuid o null

def add_device(device_code, name, uploader_email, owner_email=None, admin_token=ADMIN_TOKEN):
    uploader_id = get_user_id_by_email(uploader_email, admin_token=admin_token)
    if not uploader_id:
        raise RuntimeError(f"uploader no encontrado: {uploader_email}")
    owner_id = get_user_id_by_email(owner_email, admin_token=admin_token) if owner_email else None

    url=f"{BASE}/rest/v1/devices"
    row={"device_code":device_code,"name":name,"uploader_user_id":uploader_id,"owner_id":owner_id}
    r=requests.post(url,
        headers={"apikey":ANON,"Authorization":f"Bearer {admin_token}",
                 "Content-Type":"application/json","Prefer":"return=representation"},
        json=row, timeout=20)
    print(r.status_code); print(r.text)

def list_devices(admin_token=ADMIN_TOKEN):
    url=f"{BASE}/rest/v1/devices"
    r=requests.get(url,
        headers={"apikey":ANON,"Authorization":f"Bearer {admin_token}"},
        params={"select":"*","order":"created_at.desc"}, timeout=20)
    print(r.status_code); print(json.dumps(r.json(), indent=2))

if __name__=="__main__":
    # Ejemplos (comentar/descomentar):
    token = login("admin@demo.test", "admin1234")
    add_device("ESP32-001","Lanzador 1 - prototipo","lanzador_01@device.test", owner_email="admin@demo.test", admin_token=token)
    list_devices(admin_token=token)
