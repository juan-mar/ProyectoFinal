-- === 0.0) is admin?
create or replace function public.is_admin()
returns boolean
language sql
security definer
set search_path = public
stable
as $$
  select exists (
    select 1
    from public.profiles
    where id = auth.uid()
      and role = 'admin'
  );
$$;

grant execute on function public.is_admin() to anon, authenticated;

-- ==== 0) PROFILES (rol global) ===============================================
create table if not exists public.profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  role text not null check (role in ('admin','trainer','guest')),
  created_at timestamptz default now()
);
alter table public.profiles enable row level security;
-- Agregar columna full_name si no existe
alter table public.profiles
add column if not exists full_name text;
-- 0.1) Ampliar lista de roles permitidos (agrego 'in_hold')
alter table public.profiles drop constraint if exists profiles_role_check;
alter table public.profiles
  add constraint profiles_role_check
  check (role in ('admin','trainer','guest','in_hold'));

-- Policies (cada usuario ve su perfil; admin puede editar roles)
drop policy if exists "profiles_self_select" on public.profiles;
create policy "profiles_self_select"
  on public.profiles for select
  using (id = auth.uid());

-- Admin puede listar/leer TODOS los perfiles
drop policy if exists "profiles_admin_select_all" on public.profiles;
create policy "profiles_admin_select_all"
  on public.profiles for select
  using (public.is_admin());

-- Admin puede actualizar roles (se mantiene/ajusta)
drop policy if exists "profiles_admin_update" on public.profiles;
create policy "profiles_admin_update"
  on public.profiles for update
  using (exists (
    select 1 from public.profiles p
    where p.id = auth.uid() and p.role = 'admin'
  ))
  with check (true);


-- ==== 1) DOGS (catálogo gestionado por admin) ================================
create table if not exists public.dogs (
  id uuid primary key default gen_random_uuid(),
  dog_code text not null unique,       -- identificador legible (placa/QR)
  name text not null,
  breed text,
  sex text check (sex in ('M','F')),
  birthdate date,
  unit text,
  notes text,
  active boolean default true,
  created_at timestamptz default now()
);
create index if not exists idx_dogs_active on public.dogs(active);
alter table public.dogs enable row level security;

-- Lectura para todos (guest/trainer/admin). Escritura solo admin.
drop policy if exists "dogs_read_all" on public.dogs;
create policy "dogs_read_all"
  on public.dogs for select
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role in ('guest','trainer','admin')));

drop policy if exists "dogs_admin_crud" on public.dogs;
-- admin: puede seleccionar/insertar/actualizar/borrar
drop policy if exists "dogs_admin_all" on public.dogs;
create policy "dogs_admin_all"
  on public.dogs for all
  using (public.is_admin())
  with check (public.is_admin());

-- ==== 2) TRAINING_SESSIONS ===================================================
create table if not exists public.training_sessions (
  id uuid primary key default gen_random_uuid(),

  dog_id uuid not null references public.dogs(id) on delete restrict,

  -- entrenador principal y co-trainer (ambos pueden modificar)
  trainer_id uuid not null references auth.users(id) on delete restrict,
  co_trainer_id uuid references auth.users(id) on delete restrict,

  started_at timestamptz not null,         -- “cuándo ocurrió” (lo manda la app/ESP32)
  duration_s integer not null check (duration_s >= 0),  -- tiempo característico

  result text not null check (result in ('success','fail')),
  conditions jsonb not null,               -- clima/atmósfera
  type jsonb not null,                     -- tipo de búsqueda/sustancia/estructura (flexible)

  device_id text,                          -- id lógico del equipo (opcional)
  created_at timestamptz default now(),

  -- Idempotencia sin session_code:
  unique (trainer_id, dog_id, started_at)
);
create index if not exists idx_sessions_dog_time on public.training_sessions(dog_id, started_at);
create index if not exists idx_sessions_trainer_time on public.training_sessions(trainer_id, started_at);
alter table public.training_sessions enable row level security;

-- RLS: lectura para guest/trainer/admin
drop policy if exists "sessions_read_all" on public.training_sessions;
create policy "sessions_read_all"
  on public.training_sessions for select
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role in ('guest','trainer','admin')));

-- RLS: escritura (INSERT/UPDATE) permitida a admin o al trainer/co_trainer de la fila
drop policy if exists "sessions_write_trainer_co_or_admin" on public.training_sessions;
create policy "sessions_write_trainer_co_or_admin"
  on public.training_sessions
  for all
  with check (
    -- admin siempre puede
    exists (select 1 from public.profiles p where p.id = auth.uid() and p.role = 'admin')
    OR
    -- trainer o co-trainer pueden si la fila es a su nombre
    (
      exists (select 1 from public.profiles p2 where p2.id = auth.uid() and p2.role = 'trainer')
      and (trainer_id = auth.uid() OR co_trainer_id = auth.uid())
    )
  );

-- (opcional) Si querés que solo admin pueda borrar sesiones:
drop policy if exists "sessions_admin_delete" on public.training_sessions;
create policy "sessions_admin_delete"
  on public.training_sessions for delete
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role = 'admin'));

-- ==== 3) RPC: record_training (SIN auto-crear perros) ========================
-- Valida que el dog_code exista; upsert idempotente por (trainer_id, dog_id, started_at).
create or replace function public.record_training(
  p_dog_code text,
  p_started_at timestamptz,
  p_duration_s integer,
  p_result text,
  p_conditions jsonb,
  p_type jsonb,
  p_co_trainer_id uuid default null,
  p_device_id text default null
)
returns public.training_sessions
language plpgsql
security definer
as $$
declare
  v_dog_id uuid;
  v_session public.training_sessions;
begin
  -- normalizar/validar dog_code
  if p_dog_code is null or length(trim(p_dog_code)) = 0 then
    raise exception 'dog_code requerido';
  end if;

  -- buscar perro por dog_code (NO crear)
  select d.id into v_dog_id
  from public.dogs d
  where upper(trim(d.dog_code)) = upper(trim(p_dog_code))
  limit 1;

  if v_dog_id is null then
    raise exception 'dog_code % no existe; contacte a un admin', p_dog_code;
  end if;

  -- idempotente por (trainer_id, dog_id, started_at)
  insert into public.training_sessions (
    dog_id, trainer_id, co_trainer_id, started_at, duration_s, result, conditions, type, device_id
  )
  values (
    v_dog_id, auth.uid(), p_co_trainer_id, p_started_at, p_duration_s, p_result, p_conditions, p_type, p_device_id
  )
  on conflict (trainer_id, dog_id, started_at) do update set
    co_trainer_id = excluded.co_trainer_id,
    duration_s    = excluded.duration_s,
    result        = excluded.result,
    conditions    = excluded.conditions,
    type          = excluded.type,
    device_id     = excluded.device_id
  returning * into v_session;

  return v_session;
end;
$$;

-- Permisos para ejecutar el RPC (se valida RLS vía WITH CHECK implícita por columnas)
grant execute on function public.record_training(
  text, timestamptz, integer, text, jsonb, jsonb, uuid, text
) to anon, authenticated;


-- === 4) Función que crea el profile en 'in_hold' al registrarse/crearse un usuario
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
as $$
begin
  insert into public.profiles (id, role)
  values (new.id, 'in_hold')
  on conflict (id) do nothing;
  return new;
end;
$$;

-- 2.2) Trigger sobre auth.users
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
after insert on auth.users
for each row execute procedure public.handle_new_user();


-- == 3) RPC actualizacion de rol
drop function if exists public.promote_user(text, text, text);

create or replace function public.promote_user(
  p_email text,
  p_new_role text,
  p_name text default null
)
returns text
language plpgsql
security definer
as $$
declare
  v_user_id uuid;
  v_is_admin boolean;
  v_old_role text;
  v_new_name text;
begin
  -- 1) Solo admin puede promover
  select exists(
    select 1 from public.profiles where id = auth.uid() and role = 'admin'
  ) into v_is_admin;
  if not v_is_admin then
    raise exception 'Solo admin puede cambiar roles';
  end if;

  -- 2) Rol válido
  if p_new_role not in ('admin','trainer','guest','in_hold') then
    raise exception 'Rol inválido: %', p_new_role;
  end if;

  -- 3) Buscar usuario por email
  select id into v_user_id from auth.users where lower(email)=lower(p_email) limit 1;
  if v_user_id is null then
    raise exception 'Usuario no encontrado para %', p_email;
  end if;

  -- 4) Guardar rol anterior y actualizar
  select role into v_old_role from public.profiles where id = v_user_id;
  update public.profiles
  set
    role = p_new_role,
    full_name = coalesce(p_name, full_name)
  where id = v_user_id
  returning full_name into v_new_name;

  return format('Usuario %s actualizado: rol %s → %s, nombre: %s',
                p_email, coalesce(v_old_role,'(sin rol)'), p_new_role,
                coalesce(v_new_name,'(sin nombre)'));
end;
$$;

grant execute on function public.promote_user(text, text, text) to anon, authenticated;

-- === 5) is admin?
create or replace function public.is_admin()
returns boolean
language sql
security definer
set search_path = public
stable
as $$
  select exists (
    select 1
    from public.profiles
    where id = auth.uid()
      and role = 'admin'
  );
$$;

grant execute on function public.is_admin() to anon, authenticated;
