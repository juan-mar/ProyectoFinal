-- Tabla de prueba
create table if not exists public.pings (
  id uuid primary key default gen_random_uuid(),
  device_id text not null,
  value numeric not null,
  created_at timestamptz default now()
);

-- Activar RLS (Row Level Security)
alter table public.pings enable row level security;

-- Políticas *abiertas* SOLO para PoC (luego las vamos a endurecer)
drop policy if exists "allow insert pings" on public.pings;
drop policy if exists "allow select pings" on public.pings;

create policy "allow insert pings"
  on public.pings for insert to anon
  with check (true);

create policy "allow select pings"
  on public.pings for select to anon
  using (true);

-- Habilitar UPDATE para anon durante el PoC
drop policy if exists "allow update pings" on public.pings;
create policy "allow update pings"
  on public.pings for update to anon
  using (true)
  with check (true);