-- === 0) Crear la nueva función
create or replace function public.is_admin(uid uuid)
returns boolean
language sql
security definer
as $$
  select exists (
    select 1
    from public.profiles p
    where p.id = uid
      and p.role = 'admin'
  );
$$;

-- Dar permisos de ejecución a todos los usuarios autenticados
grant execute on function public.is_admin(uuid) to authenticated, anon;

-- ==== 1) PROFILES (rol global) ===============================================
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
  check (role in ('admin','trainer','guest','in_hold','device'));

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

-- ==== 2) DEVICES  ============================================================
create table if not exists public.devices (
  id uuid primary key default gen_random_uuid(),
  device_code text not null unique,           -- ej. ESP32-001 (lo que grabás en NVS)
  name text,
  owner_id uuid references auth.users(id) on delete set null,  -- opcional: “dueño”
  uploader_user_id uuid not null references auth.users(id) on delete restrict, -- usuario de Auth que usará la ESP32 para autenticarse
  active boolean default true,
  created_at timestamptz default now()
);
create index if not exists idx_devices_active on public.devices(active);
alter table public.devices enable row level security;


-- Lectura (guest/trainer/admin): si querés solo admin/trainer, ajustalo.
drop policy if exists "devices_read_basic" on public.devices;
create policy "devices_read_basic"
  on public.devices for select
  using (
    exists (select 1 from public.profiles p where p.id = auth.uid() and p.role in ('guest','trainer','admin'))
    or auth.uid() = uploader_user_id  -- el device ve su fila
  );

-- Escritura: solo admin crea/edita devices.
drop policy if exists "devices_admin_all" on public.devices;
create policy "devices_admin_all"
  on public.devices for all
  using (public.is_admin(auth.uid()))
  with check (public.is_admin(auth.uid()));


-- ==== 3) DOGS (catálogo gestionado por admin) ================================
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
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role in ('guest','trainer','admin', 'device')));

drop policy if exists "dogs_admin_crud" on public.dogs;
-- admin: puede seleccionar/insertar/actualizar/borrar
drop policy if exists "dogs_admin_all" on public.dogs;
create policy "dogs_admin_all"
  on public.dogs for all
  using (public.is_admin())
  with check (public.is_admin());



-- ==== 4) TRAINING_SESSIONS ===================================================
-- a) Borrar policies para evitar conflictos de nombres
drop policy if exists "sessions_read_all" on public.training_sessions;
drop policy if exists "sessions_insert_admin_or_device" on public.training_sessions;
drop policy if exists "sessions_update_rules" on public.training_sessions;

-- b) Borrar tabla
-- drop table if exists public.training_sessions cascade; --ya fue borrada

-- c) Crear tabla nueva con el esquema “device-first”
create table if not exists public.training_sessions (
  id uuid primary key default gen_random_uuid(),

  dog_id uuid not null references public.dogs(id) on delete restrict,

  trainer_id uuid references auth.users(id) on delete restrict,      -- ahora nullable
  co_trainer_id uuid references auth.users(id) on delete restrict,   -- nullable

  started_at timestamptz not null,
  duration_s integer not null check (duration_s >= 0),

  result text not null check (result in ('success','fail')),
  conditions jsonb not null,
  type jsonb not null,

  device_id uuid references public.devices(id),
  submitted_by uuid not null default auth.uid() references auth.users(id),

  created_at timestamptz default now(),

  unique (device_id, started_at)     -- idempotencia por device+hora (agregá dog_id si querés)
);

create index if not exists idx_sessions_dog_time on public.training_sessions(dog_id, started_at);
create index if not exists idx_sessions_trainer_time on public.training_sessions(trainer_id, started_at);

alter table public.training_sessions enable row level security;

-- RLS: lectura para guest/trainer/admin
drop policy if exists "sessions_read_all" on public.training_sessions;
create policy "sessions_read_all"
  on public.training_sessions for select
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role in ('guest','trainer','admin')));

-- RLS: escritura (INSERT) permitida a admin o al trainer/co_trainer de la fila
drop policy if exists "sessions_write_trainer_co_or_admin" on public.training_sessions; -- Anterior poliza
drop policy if exists "sessions_insert_admin_or_device" on public.training_sessions;
create policy "sessions_insert_admin_or_device"
  on public.training_sessions for insert
  with check (
    public.is_admin(auth.uid())
    or exists (
      select 1 from public.devices d
      where d.id = training_sessions.device_id
        and d.uploader_user_id = auth.uid()
    )
  );

-- RLS: escritura (UPDATE) permitida a admin o al trainer/co_trainer de la fila
drop policy if exists "sessions_update_rules" on public.training_sessions;
create policy "sessions_update_rules"
  on public.training_sessions for update
  using (
    public.is_admin(auth.uid())
    or (
      exists (select 1 from public.profiles p where p.id = auth.uid() and p.role = 'trainer')
      and (training_sessions.trainer_id is null or training_sessions.trainer_id = auth.uid())
    )
    or (
      exists (select 1 from public.devices d
              where d.id = training_sessions.device_id
                and d.uploader_user_id = auth.uid())
      and training_sessions.submitted_by = auth.uid()
    )
  )
  with check (
    public.is_admin(auth.uid())
    or (
      exists (select 1 from public.profiles p where p.id = auth.uid() and p.role = 'trainer')
      and (trainer_id is null or trainer_id = auth.uid())
    )
    or (
      exists (select 1 from public.devices d
              where d.id = training_sessions.device_id
                and d.uploader_user_id = auth.uid())
      and submitted_by = auth.uid()
    )
  );

-- Solo admin pueda borrar sesiones:
drop policy if exists "sessions_admin_delete" on public.training_sessions;
create policy "sessions_admin_delete"
  on public.training_sessions for delete
  using (exists (select 1 from public.profiles p where p.id = auth.uid() and p.role = 'admin'));

-- ==== 5) RPC: record_training (SIN auto-crear perros) ========================
-- Reemplaza la versión anterior que recibía p_device_id
drop function if exists public.record_training(text, timestamptz, int, text, jsonb, jsonb, uuid, uuid);

-- Nueva firma: recibe p_device_code (text)
drop function if exists public.record_training(text, timestamptz, int, text, jsonb, jsonb, text, uuid);

create or replace function public.record_training(
  p_dog_code       text,
  p_started_at     timestamptz,
  p_duration_s     integer,
  p_result         text,
  p_conditions     jsonb,
  p_type           jsonb,
  p_device_code    text,         -- <- ahora code, NO uuid
  p_co_trainer_id  uuid default null
)
returns public.training_sessions
language plpgsql
security definer
as $$
declare
  v_dog_id   uuid;
  v_dev_id   uuid;
  v_ok       boolean;
  v_session  public.training_sessions;
begin
  -- 0) Resolver device_id por device_code y validar propietario (auth.uid)
  select d.id
    into v_dev_id
  from public.devices d
  where upper(trim(d.device_code)) = upper(trim(p_device_code))
    and d.active = true
  limit 1;

  if v_dev_id is null then
    raise exception 'device_code % inexistente o inactivo', p_device_code;
  end if;

  -- Validar que el token pertenezca al uploader del device
  select exists(
    select 1 from public.devices d
    where d.id = v_dev_id
      and d.uploader_user_id = auth.uid()
  ) into v_ok;

  if not v_ok then
    raise exception 'device/auth no autorizado';
  end if;

  -- 1) Resolver dog_id por code
  select id into v_dog_id
  from public.dogs
  where upper(trim(dog_code)) = upper(trim(p_dog_code))
  limit 1;

  if v_dog_id is null then
    raise exception 'dog_code % no existe', p_dog_code;
  end if;

  -- 2) Insert / Upsert idempotente por (device_id, started_at)
  insert into public.training_sessions (
    dog_id, trainer_id, co_trainer_id,
    started_at, duration_s, result, conditions, type,
    device_id, submitted_by
  )
  values (
    v_dog_id, null, p_co_trainer_id,
    p_started_at, p_duration_s, p_result, p_conditions, p_type,
    v_dev_id, auth.uid()
  )
  on conflict (device_id, started_at) do update set
    duration_s    = excluded.duration_s,
    result        = excluded.result,
    conditions    = excluded.conditions,
    type          = excluded.type,
    co_trainer_id = excluded.co_trainer_id
  returning * into v_session;

  return v_session;
end;
$$;

grant execute on function public.record_training(
  text, timestamptz, int, text, jsonb, jsonb, text, uuid
) to anon, authenticated;


-- === 6) RPC: Función que crea el profile en 'in_hold' al registrarse/crearse un usuario
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

-- 6.1) Trigger sobre auth.users
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
after insert on auth.users
for each row execute procedure public.handle_new_user();


-- === 7) RPC: actualizacion de rol
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
  -- 7.1) Solo admin puede promover
  select exists(
    select 1 from public.profiles where id = auth.uid() and role = 'admin'
  ) into v_is_admin;
  if not v_is_admin then
    raise exception 'Solo admin puede cambiar roles';
  end if;

  -- 7.2) Rol válido
  if p_new_role not in ('admin','trainer','guest','in_hold') then
    raise exception 'Rol inválido: %', p_new_role;
  end if;

  -- 7.3) Buscar usuario por email
  select id into v_user_id from auth.users where lower(email)=lower(p_email) limit 1;
  if v_user_id is null then
    raise exception 'Usuario no encontrado para %', p_email;
  end if;

  -- 7.4) Guardar rol anterior y actualizar
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

-- === 8) RPC: Carga por lotes (cada item = una sesión)
drop function if exists public.record_training_batch(jsonb);

create or replace function public.record_training_batch(p_items jsonb)
returns void
language plpgsql
security definer
as $$
declare
  item jsonb;
begin
  if p_items is null or jsonb_typeof(p_items) <> 'array' then
    raise exception 'p_items debe ser un array JSON';
  end if;

  for item in select jsonb_array_elements(p_items)
  loop
    -- Validación rápida de campos nulos
    if item->>'p_dog_code' is null or item->>'p_device_code' is null then
       raise exception 'Faltan campos obligatorios: p_dog_code o p_device_code';
    end if;

    -- Llamamos a la función unitaria. Si esto falla, abortará toda la función batch.
    perform public.record_training(
      item->>'p_dog_code',
      (item->>'p_started_at')::timestamptz,
      (item->>'p_duration_s')::int,
      item->>'p_result',
      item->'p_conditions',
      item->'p_type',
      item->>'p_device_code',
      (item->>'p_co_trainer_id')::uuid
    );
  end loop;
end;
$$;

grant execute on function public.record_training_batch(jsonb) to anon, authenticated;

-- === 9) Devuelve el UUID de un usuario por email (case-insensitive)
-- SECURITY DEFINER para poder leer auth.users sin RLS
drop function if exists public.get_user_id_by_email(text);
create or replace function public.get_user_id_by_email(p_email text)
returns uuid
language sql
security definer
set search_path = public
as $$
  select u.id
  from auth.users u
  where lower(u.email) = lower(p_email)
  limit 1
$$;
revoke all on function public.get_user_id_by_email(text) from public;
grant execute on function public.get_user_id_by_email(text) to anon, authenticated;

-- === 9.1) Get sessions by dog_code
drop function if exists public.sessions_by_dog_code(text, int, boolean);
create or replace function public.sessions_by_dog_code(
  p_dog_code text,
  p_limit    int default 100,
  p_desc     boolean default true
)
returns table (
  id uuid,
  dog_id uuid,
  dog_code text,
  dog_name text,
  trainer_id uuid,
  co_trainer_id uuid,
  started_at timestamptz,
  duration_s integer,
  result text,
  conditions jsonb,
  type jsonb,
  device_id uuid,          -- <- corregido a uuid
  device_code text,        -- <- extra útil para lectura
  created_at timestamptz
)
language plpgsql
security invoker
as $$
declare
  v_dog_id uuid;
begin
  select d.id into v_dog_id
  from public.dogs d
  where upper(trim(d.dog_code)) = upper(trim(p_dog_code))
  limit 1;

  if v_dog_id is null then
    return;
  end if;

  if p_desc then
    return query
      select ts.id, ts.dog_id, d.dog_code, d.name,
             ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
             ts.result, ts.conditions, ts.type,
             ts.device_id, dv.device_code,   -- <- agregamos code
             ts.created_at
      from public.training_sessions ts
      join public.dogs d on d.id = ts.dog_id
      left join public.devices dv on dv.id = ts.device_id
      where ts.dog_id = v_dog_id
      order by ts.started_at desc
      limit p_limit;
  else
    return query
      select ts.id, ts.dog_id, d.dog_code, d.name,
             ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
             ts.result, ts.conditions, ts.type,
             ts.device_id, dv.device_code,
             ts.created_at
      from public.training_sessions ts
      join public.dogs d on d.id = ts.dog_id
      left join public.devices dv on dv.id = ts.device_id
      where ts.dog_id = v_dog_id
      order by ts.started_at asc
      limit p_limit;
  end if;
end;
$$;

grant execute on function public.sessions_by_dog_code(text, int, boolean) to anon, authenticated;


-- === 9.2) get sessions by trainer
drop function if exists public.sessions_by_trainer_email(text, boolean, int, boolean);
create or replace function public.sessions_by_trainer_email(
  p_email text,
  p_include_as_co_trainer boolean default true,
  p_limit int default 100,
  p_desc boolean default true
)
returns table (
  id uuid,
  dog_id uuid,
  dog_code text,
  dog_name text,
  trainer_id uuid,
  co_trainer_id uuid,
  started_at timestamptz,
  duration_s integer,
  result text,
  conditions jsonb,
  type jsonb,
  device_id uuid,       -- <- uuid
  device_code text,     -- <- agregado
  created_at timestamptz
)
language plpgsql
security invoker
as $$
declare
  v_trainer uuid;
begin
  v_trainer := public.get_user_id_by_email(p_email);
  if v_trainer is null then
    return;
  end if;

  if p_desc then
    if p_include_as_co_trainer then
      return query
        select ts.id, ts.dog_id, d.dog_code, d.name,
               ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
               ts.result, ts.conditions, ts.type,
               ts.device_id, dv.device_code,
               ts.created_at
        from public.training_sessions ts
        join public.dogs d on d.id = ts.dog_id
        left join public.devices dv on dv.id = ts.device_id
        where ts.trainer_id = v_trainer or ts.co_trainer_id = v_trainer
        order by ts.started_at desc
        limit p_limit;
    else
      return query
        select ts.id, ts.dog_id, d.dog_code, d.name,
               ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
               ts.result, ts.conditions, ts.type,
               ts.device_id, dv.device_code,
               ts.created_at
        from public.training_sessions ts
        join public.dogs d on d.id = ts.dog_id
        left join public.devices dv on dv.id = ts.device_id
        where ts.trainer_id = v_trainer
        order by ts.started_at desc
        limit p_limit;
    end if;
  else
    if p_include_as_co_trainer then
      return query
        select ts.id, ts.dog_id, d.dog_code, d.name,
               ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
               ts.result, ts.conditions, ts.type,
               ts.device_id, dv.device_code,
               ts.created_at
        from public.training_sessions ts
        join public.dogs d on d.id = ts.dog_id
        left join public.devices dv on dv.id = ts.device_id
        where ts.trainer_id = v_trainer or ts.co_trainer_id = v_trainer
        order by ts.started_at asc
        limit p_limit;
    else
      return query
        select ts.id, ts.dog_id, d.dog_code, d.name,
               ts.trainer_id, ts.co_trainer_id, ts.started_at, ts.duration_s,
               ts.result, ts.conditions, ts.type,
               ts.device_id, dv.device_code,
               ts.created_at
        from public.training_sessions ts
        join public.dogs d on d.id = ts.dog_id
        left join public.devices dv on dv.id = ts.device_id
        where ts.trainer_id = v_trainer
        order by ts.started_at asc
        limit p_limit;
    end if;
  end if;
end;
$$;

grant execute on function public.sessions_by_trainer_email(text, boolean, int, boolean) to anon, authenticated;