import os, requests, json
from dotenv import load_dotenv
from DB01_login import login

load_dotenv()
BASE = os.getenv("SUPABASE_URL"); ANON = os.getenv("SUPABASE_ANON")
ADMIN_TOKEN = "PEGÁ_TU_TOKEN_DE_ADMIN"

def promote_user(email, new_role, name=None, admin_token=ADMIN_TOKEN):
    url = f"{BASE}/rest/v1/rpc/promote_user"
    payload = {"p_email": email, "p_new_role": new_role}
    if name:
        payload["p_name"] = name
    r = requests.post(url,
        headers={"apikey": ANON, "Authorization": f"Bearer {admin_token}",
                 "Content-Type":"application/json", "Prefer":"return=representation"},
        json=payload, timeout=20)
    print(r.status_code, r.text)

if __name__ == "__main__":
    token = login("admin@demo.test", "admin1234")
    #promote_user("trainer1@demo.test", "trainer", "Jose Antonio", token)
    promote_user("admin@demo.test", "admin", "Admin Test", token)
