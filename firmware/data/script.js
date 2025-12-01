// Estado global para el modo seleccionado
let currentMode = 'manual';

// Inicialización
document.addEventListener('DOMContentLoaded', () => {
    loadDogs();
    loadStatus();
    // Actualizar status cada 5 segundos
    setInterval(loadStatus, 5000);
});

// Cambiar modo (Manual / Auto)
function setMode(mode) {
    currentMode = mode;
    // Actualizar clases visuales
    document.getElementById('btn-manual').className = (mode === 'manual') ? 'mode-btn active' : 'mode-btn';
    document.getElementById('btn-auto').className = (mode === 'auto') ? 'mode-btn active' : 'mode-btn';
}

// Cargar lista de perros desde el ESP32
async function loadDogs() {
    const selector = document.getElementById('dogSelector');
    
    try {
        const response = await fetch('/api/dogs');
        const dogs = await response.json();
        
        selector.innerHTML = ''; // Limpiar opciones

        if (dogs.length === 0) {
            selector.innerHTML = '<option value="">⚠️ Sincroniza para descargar perros</option>';
            return;
        }

        dogs.forEach(dog => {
            let opt = document.createElement('option');
            // Aseguramos que usamos las claves correctas que vienen del JSON
            opt.value = dog.dog_code; 
            opt.text = dog.name;
            selector.add(opt);
        });
        
        // Seleccionar el primero por defecto para evitar envíos nulos
        if(dogs.length > 0) selector.selectedIndex = 0;

    } catch (error) {
        console.error("Error cargando perros:", error);
        selector.innerHTML = '<option value="">Error de conexión</option>';
    }
}

// Cargar estado (batería, almacenamiento)
async function loadStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        document.getElementById('bat-val').innerText = data.battery;
        document.getElementById('sess-val').innerText = data.pending_sessions;
    } catch(e) {
        // Silencioso si falla el update de fondo
    }
}

// Enviar Configuración
async function startTraining() {
    const btn = document.getElementById('btn-start');
    const msg = document.getElementById('status-msg');
    const dogVal = document.getElementById('dogSelector').value;
    const tempVal = document.getElementById('tempInput').value;

    // 1. VALIDACIÓN ROBUSTA (Evita crash en ESP32)
    if (!dogVal || dogVal === "" || dogVal === "undefined") {
        alert("⚠️ Por favor selecciona un perro válido.");
        return;
    }

    if (tempVal === "") {
        alert("⚠️ Ingresa una temperatura.");
        return;
    }

    // 2. Bloquear UI
    btn.disabled = true;
    btn.innerText = "Enviando...";
    msg.innerText = "";

    // 3. Construir Payload
    const payload = {
        dog_code: dogVal,
        mode: currentMode,
        temp: parseFloat(tempVal), // Asegurar que sea número
        timestamp: new Date().toISOString()
    };

    console.log("Enviando:", payload);

    // 4. Enviar al Backend
    try {
        const res = await fetch('/api/start', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });

        const data = await res.json();

        if (res.ok) {
            msg.style.color = "green";
            msg.innerText = "✅ ¡Configurado! Iniciando...";
            // Opcional: mostrar pantalla de éxito o bloquear todo
        } else {
            throw new Error(data.msg || "Error desconocido");
        }
    } catch (error) {
        msg.style.color = "red";
        msg.innerText = "❌ Error: " + error.message;
        btn.innerText = "REINTENTAR";
        btn.disabled = false;
    }
}