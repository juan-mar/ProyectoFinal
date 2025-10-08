import os, requests, json
from dotenv import load_dotenv
from DB01_login import login

load_dotenv()

BASE = os.getenv("SUPABASE_URL")
ANON = os.getenv("SUPABASE_ANON")
TOKEN = "PEGA_ACÁ_TU_TOKEN"  # guest/trainer/admin

def by_dog_code(dog_code, limit=50, desc=True, token=TOKEN):
    url = f"{BASE}/rest/v1/rpc/sessions_by_dog_code"
    payload = {"p_dog_code": dog_code, "p_limit": limit, "p_desc": desc}
    r = requests.post(url,
        headers={"apikey": ANON, "Authorization": f"Bearer {token}", "Content-Type":"application/json"},
        json=payload, timeout=20)
    print(r.status_code)
    try:
        print(json.dumps(r.json(), indent=2))
    except Exception:
        print(r.text)

if __name__ == "__main__":
    tr_token = login("trainer1@demo.test", "trainer1234")
    by_dog_code("LUNA-002", limit=20, desc=True, token=tr_token)
