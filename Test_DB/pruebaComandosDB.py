import os, json, argparse, time, random, uuid, requests
from datetime import datetime, timedelta, timezone
from dotenv import load_dotenv

load_dotenv()
BASE = os.getenv("SUPABASE_URL")
ANON = os.getenv("SUPABASE_ANON")

def ensure_env():
    assert BASE and ANON, "Faltan SUPABASE_URL o SUPABASE_ANON en .env"

# ------------------------
# Helpers HTTP / headers
# ------------------------
def hdr(token=None, extra=None):
    h = {
        "apikey": ANON,
        "Authorization": f"Bearer {token or ANON}",
    }
    if extra:
        h.update(extra)
    return h

def jpost(url, token, payload, prefer=None, timeout=20):
    headers = hdr(token, {"Content-Type": "application/json"})
    if prefer:
        headers["Prefer"] = prefer
    r = requests.post(url, headers=headers, data=json.dumps(payload), timeout=timeout)
    _raise_or_return(r)
    return r.json() if r.text else {}

def jget(url, token, params=None, timeout=20):
    r = requests.get(url, headers=hdr(token), params=params or {}, timeout=timeout)
    _raise_or_return(r)
    ct = r.headers.get("Content-Type", "")
    return r.json() if ct.startswith("application/json") else r.text

def jpatch(url, token, payload, timeout=20):
    r = requests.patch(url, headers=hdr(token, {"Content-Type":"application/json"}), data=json.dumps(payload), timeout=timeout)
    _raise_or_return(r)
    return r.json() if r.text else {}

def _raise_or_return(r):
    try:
        r.raise_for_status()
    except requests.HTTPError as e:
        print("Status:", r.status_code)
        print("Body:", r.text)
        raise

# ------------------------
# Auth (obtener token)
# ------------------------
def login(email, password):
    url = f"{BASE}/auth/v1/token?grant_type=password"
    payload = {"email": email, "password": password}
    r = requests.post(url, headers=hdr(), json=payload, timeout=20)
    _raise_or_return(r)
    return r.json()["access_token"]

# ------------------------
# Dogs (admin)
# ------------------------
def dogs_add(token, dog_code, name, breed=None, sex=None, birthdate=None, unit=None, notes=None):
    url = f"{BASE}/rest/v1/dogs"
    row = {
        "dog_code": dog_code,
        "name": name,
        "breed": breed,
        "sex": sex,           # 'M' o 'F' o None
        "birthdate": birthdate, # 'YYYY-MM-DD' o None
        "unit": unit,
        "notes": notes,
        "active": True
    }
    # limpiar None
    row = {k:v for k,v in row.items() if v is not None}
    return jpost(url, token, row, prefer="return=representation")

def dogs_list(token, only_active=True, limit=100):
    url = f"{BASE}/rest/v1/dogs"
    params = {"select":"*", "order":"created_at.desc", "limit": str(limit)}
    if only_active:
        params["active"] = "eq.true"
    return jget(url, token, params=params)

# ------------------------
# Sessions (RPC recomendado)
# ------------------------
def record_session_rpc(token, dog_code, started_at, duration_s, result, conditions, typ, co_trainer_id=None, device_id=None):
    url = f"{BASE}/rest/v1/rpc/record_training"
    payload = {
        "p_dog_code": dog_code,
        "p_started_at": started_at,
        "p_duration_s": duration_s,
        "p_result": result,           # 'success' / 'fail'
        "p_conditions": conditions,   # dict JSON
        "p_type": typ,                # dict JSON
        "p_co_trainer_id": co_trainer_id,
        "p_device_id": device_id
    }
    # limpiar None
    payload = {k:v for k,v in payload.items() if v is not None}
    return jpost(url, token, payload, prefer="return=representation")

# ------------------------
# Sessions (upsert directo)
# ------------------------
def upsert_session_direct(token, row):
    # row debe incluir: dog_id, trainer_id (= auth.uid del token), started_at, duration_s, result, conditions, type
    url = f"{BASE}/rest/v1/training_sessions?on_conflict=trainer_id,dog_id,started_at"
    prefer = "return=representation,resolution=merge-duplicates"
    # PostgREST acepta lista para upsert múltiple
    payload = [row]
    return jpost(url, token, payload, prefer=prefer)

# ------------------------
# Queries de sesiones
# ------------------------
def sessions_by_dog(token, dog_id, limit=100, order="started_at.desc"):
    url = f"{BASE}/rest/v1/training_sessions"
    params = {"select":"*,dogs(name,dog_code)", "dog_id":f"eq.{dog_id}", "order": order, "limit": str(limit)}
    return jget(url, token, params=params)

def sessions_by_trainer(token, trainer_id, limit=100, order="started_at.desc"):
    url = f"{BASE}/rest/v1/training_sessions"
    params = {"select":"*,dogs(name,dog_code)", "trainer_id":f"eq.{trainer_id}", "order": order, "limit": str(limit)}
    return jget(url, token, params=params)

# ------------------------
# Simulación simple de un día
# ------------------------
def simulate_day(token_trainer, dog_codes, base_start=None, n_sessions=5, device_id="esp32-sim-01"):
    """
    Genera n_sessions con tiempos crecientes y resultados aleatorios (success/fail).
    Usa el RPC (record_training). Requiere que los dog_codes existan.
    """
    if base_start is None:
        # hoy a las 10:00 UTC (ajustá si querés)
        base_start = datetime.now(timezone.utc).replace(hour=13, minute=0, second=0, microsecond=0)

    out = []
    clock = base_start
    for i in range(n_sessions):
        dog_code = random.choice(dog_codes)
        duration = random.randint(120, 600)  # 2 a 10 min
        result = random.choice(["success", "fail"])
        conditions = {"temp": round(random.uniform(12.0, 24.0), 1), "wind_kmh": random.randint(0, 25), "humidity": random.randint(30, 80)}
        typ = {"scent": random.choice(["Explosivos", "Narcóticos", "Búsqueda"]), "mode": random.choice(["en linea", "ambiental"])}

        started_at = clock.isoformat().replace("+00:00", "Z")
        row = record_session_rpc(
            token_trainer,
            dog_code=dog_code,
            started_at=started_at,
            duration_s=duration,
            result=result,
            conditions=conditions,
            typ=typ,
            co_trainer_id=None,
            device_id=device_id
        )
        out.append(row)
        # mover reloj para próxima sesión
        clock = clock + timedelta(minutes=random.randint(5, 30))
    return out

# ------------------------
# CLI
# ------------------------
def main():
    ensure_env()
    p = argparse.ArgumentParser(description="CLI para probar Supabase (dogs & training_sessions).")
    sub = p.add_subparsers(dest="cmd", required=True)

    # login
    sp = sub.add_parser("login", help="Obtener access_token (email/password)")
    sp.add_argument("--email", required=True)
    sp.add_argument("--password", required=True)

    # dogs add/list
    sp = sub.add_parser("dogs-add", help="(ADMIN) Crear perro")
    sp.add_argument("--token", required=True)
    sp.add_argument("--dog_code", required=True)
    sp.add_argument("--name", required=True)
    sp.add_argument("--breed")
    sp.add_argument("--sex", choices=["M","F"])
    sp.add_argument("--birthdate")  # YYYY-MM-DD
    sp.add_argument("--unit")
    sp.add_argument("--notes")

    sp = sub.add_parser("dogs-list", help="Listar perros")
    sp.add_argument("--token", required=True)
    sp.add_argument("--all", action="store_true")
    sp.add_argument("--limit", type=int, default=100)

    # record (RPC)
    sp = sub.add_parser("record", help="(TRAINER) Registrar sesión vía RPC")
    sp.add_argument("--token", required=True)
    sp.add_argument("--dog_code", required=True)
    sp.add_argument("--started_at", required=True, help="ISO 8601 ej 2025-10-04T14:12:00Z")
    sp.add_argument("--duration_s", type=int, required=True)
    sp.add_argument("--result", choices=["success","fail"], required=True)
    sp.add_argument("--conditions", required=True, help="JSON dict ej '{\"temp\":18.2}'")
    sp.add_argument("--type", required=True, help="JSON dict ej '{\"scent\":\"Explosivos\",\"mode\":\"en linea\"}'")
    sp.add_argument("--co_trainer_id")
    sp.add_argument("--device_id")

    # upsert directo
    sp = sub.add_parser("upsert", help="(TRAINER) Upsert directo en training_sessions")
    sp.add_argument("--token", required=True)
    sp.add_argument("--dog_id", required=True)
    sp.add_argument("--trainer_id", required=True)
    sp.add_argument("--started_at", required=True)
    sp.add_argument("--duration_s", type=int, required=True)
    sp.add_argument("--result", choices=["success","fail"], required=True)
    sp.add_argument("--conditions", required=True)
    sp.add_argument("--type", required=True)
    sp.add_argument("--co_trainer_id")
    sp.add_argument("--device_id")

    # queries
    sp = sub.add_parser("by-dog", help="Listar sesiones por perro")
    sp.add_argument("--token", required=True)
    sp.add_argument("--dog_id", required=True)
    sp.add_argument("--limit", type=int, default=50)

    sp = sub.add_parser("by-trainer", help="Listar sesiones por trainer")
    sp.add_argument("--token", required=True)
    sp.add_argument("--trainer_id", required=True)
    sp.add_argument("--limit", type=int, default=50)

    # simulación
    sp = sub.add_parser("simulate-day", help="Simular n sesiones en el día con un trainer")
    sp.add_argument("--token", required=True)
    sp.add_argument("--dogs", required=True, help="Lista separada por comas de dog_code, ej FIRU-001,FIRU-002")
    sp.add_argument("--n", type=int, default=5)
    sp.add_argument("--device_id", default="esp32-sim-01")

    args = p.parse_args()

    if args.cmd == "login":
        token = login(args.email, args.password)
        print(token)
        return

    if args.cmd == "dogs-add":
        res = dogs_add(
            token=args.token, dog_code=args.dog_code, name=args.name,
            breed=args.breed, sex=args.sex, birthdate=args.birthdate,
            unit=args.unit, notes=args.notes
        )
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "dogs-list":
        res = dogs_list(token=args.token, only_active=not args.all, limit=args.limit)
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "record":
        conditions = json.loads(args.conditions)
        typ = json.loads(args.type)
        res = record_session_rpc(
            token=args.token,
            dog_code=args.dog_code,
            started_at=args.started_at,
            duration_s=args.duration_s,
            result=args.result,
            conditions=conditions,
            typ=typ,
            co_trainer_id=args.co_trainer_id,
            device_id=args.device_id
        )
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "upsert":
        row = {
            "dog_id": args.dog_id,
            "trainer_id": args.trainer_id,
            "co_trainer_id": args.co_trainer_id,
            "started_at": args.started_at,
            "duration_s": args.duration_s,
            "result": args.result,
            "conditions": json.loads(args.conditions),
            "type": json.loads(args.type),
            "device_id": args.device_id
        }
        # limpiar None
        row = {k:v for k,v in row.items() if v is not None}
        res = upsert_session_direct(args.token, row)
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "by-dog":
        res = sessions_by_dog(args.token, args.dog_id, limit=args.limit)
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "by-trainer":
        res = sessions_by_trainer(args.token, args.trainer_id, limit=args.limit)
        print(json.dumps(res, indent=2))
        return

    if args.cmd == "simulate-day":
        dog_codes = [d.strip() for d in args.dogs.split(",") if d.strip()]
        res = simulate_day(args.token, dog_codes, n_sessions=args.n, device_id=args.device_id)
        print(json.dumps(res, indent=2))
        return

if __name__ == "__main__":
    main()
