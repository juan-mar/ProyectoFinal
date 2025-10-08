import os, requests, json
from dotenv import load_dotenv
from DB01_login import login

load_dotenv()
BASE = os.getenv("SUPABASE_URL"); ANON = os.getenv("SUPABASE_ANON")

TOKEN = "PEGÁ_ACÁ_TU_ACCESS_TOKEN"  # guest/trainer/admin

def sessions_by_dog(dog_id, limit=50):
    url = f"{BASE}/rest/v1/training_sessions"
    params = {"select":"*,dogs(name,dog_code)",
              "dog_id": f"eq.{dog_id}",
              "order": "started_at.desc",
              "limit": str(limit)}
    r = requests.get(url, headers={"apikey": ANON, "Authorization": f"Bearer {TOKEN}"}, params=params, timeout=20)
    print(r.status_code); print(json.dumps(r.json(), indent=2))

def sessions_by_trainer(trainer_id, limit=50):
    url = f"{BASE}/rest/v1/training_sessions"
    params = {"select":"*,dogs(name,dog_code)",
              "trainer_id": f"eq.{trainer_id}",
              "order": "started_at.desc",
              "limit": str(limit)}
    r = requests.get(url, headers={"apikey": ANON, "Authorization": f"Bearer {TOKEN}"}, params=params, timeout=20)
    print(r.status_code); print(json.dumps(r.json(), indent=2))

if __name__ == "__main__":
    # Primero obtené el access_token con login() y pegalo en TOKEN más arriba
    tr_token = login("trainer1@demo.test", "trainer1234")
    #gs_token = login("guest@demo.test", "guest1234")

    # Completa uno de estos y comentá el otro:
    # sessions_by_dog("<DOG_UUID>", limit=20)
    # sessions_by_trainer("<TRAINER_UUID>", limit=20)
    pass
