# login.py
import os, requests
from dotenv import load_dotenv
load_dotenv()

BASE = os.getenv("SUPABASE_URL")
ANON = os.getenv("SUPABASE_ANON")

def login(email, password):
    url = f"{BASE}/auth/v1/token?grant_type=password"
    r = requests.post(url, headers={"apikey": ANON, "Authorization": f"Bearer {ANON}"},
                      json={"email": email, "password": password}, timeout=20)
    r.raise_for_status()
    data = r.json()
    print("access_token:\n", data["access_token"])
    return data["access_token"]

if __name__ == "__main__":
    # EDITA acá con tus credenciales creadas en Supabase Auth → Users
    login("lanzador_01@device.test", "lanzador_01")
