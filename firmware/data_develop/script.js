// Estado global para el modo seleccionado
let currentMode = 'manual';
let connectionLost = false;
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 60; // 5 minutos (60 intentos x 5 seg)

// Inicialización
document.addEventListener('DOMContentLoaded', () => {
    restoreFormData(); // Recuperar datos guardados
    loadDogs();
    loadStatus();
    // Actualizar status cada 5 segundos y detectar desconexión
    setInterval(checkConnection, 5000);
    
    // Listeners para guardar datos al cambiar
    document.getElementById('dogSelector').addEventListener('change', saveFormData);
    document.getElementById('substanceSelector').addEventListener('change', saveFormData);
    document.getElementById('distractorsCheckbox').addEventListener('change', saveFormData);
    document.getElementById('contextInput').addEventListener('change', saveFormData);
    document.getElementById('durationInput').addEventListener('input', (e) => {
        document.getElementById('durationHelper').innerText = e.target.value + ' segundos';
        saveFormData();
    });
});

// Cambiar modo (Manual / Auto)
function setMode(mode) {
    currentMode = mode;
    // Actualizar clases visuales
    document.getElementById('btn-manual').className = (mode === 'manual') ? 'mode-btn active' : 'mode-btn';
    document.getElementById('btn-auto').className = (mode === 'auto') ? 'mode-btn active' : 'mode-btn';
    saveFormData(); // Guardar al cambiar modo
}

// === PERSISTENCIA DE DATOS ===

function saveFormData() {
    const data = {
        dogCode: document.getElementById('dogSelector').value,
        substance: document.getElementById('substanceSelector').value,
        distractors: document.getElementById('distractorsCheckbox').checked,
        context: document.getElementById('contextInput').value,
        duration: parseInt(document.getElementById('durationInput').value) || 30,
        mode: currentMode,
        timestamp: Date.now()
    };
    localStorage.setItem('trainingFormData', JSON.stringify(data));
}

function restoreFormData() {
    const saved = localStorage.getItem('trainingFormData');
    if (!saved) return;
    
    try {
        const data = JSON.parse(saved);
        // Solo restaurar si es reciente (< 1 hora)
        if (Date.now() - data.timestamp < 3600000) {
            currentMode = data.mode || 'manual';
            setMode(currentMode);
            
            // Restaurar todos los campos
            if (data.dogCode) {
                setTimeout(() => {
                    document.getElementById('dogSelector').value = data.dogCode;
                }, 500);
            }
            
            if (data.substance) {
                document.getElementById('substanceSelector').value = data.substance;
            }
            
            if (data.distractors) {
                document.getElementById('distractorsCheckbox').checked = data.distractors;
            }
            
            if (data.context) {
                document.getElementById('contextInput').value = data.context;
            }
            
            if (data.duration) {
                const durationInput = document.getElementById('durationInput');
                durationInput.value = data.duration;
                document.getElementById('durationHelper').innerText = data.duration + ' segundos';
            }
        }
    } catch(e) {
        console.error('Error restaurando datos:', e);
    }
}

function clearFormData() {
    localStorage.removeItem('trainingFormData');
}

// === GESTIÓN DE CONEXIÓN ===

function showOverlay(title, message) {
    document.getElementById('overlay-title').innerText = title;
    document.getElementById('overlay-msg').innerHTML = message;
    document.getElementById('overlay').classList.remove('hidden');
}

function hideOverlay() {
    document.getElementById('overlay').classList.add('hidden');
}

async function checkConnection() {
    try {
        const response = await fetch('/api/status', { 
            method: 'GET',
            cache: 'no-cache',
            signal: AbortSignal.timeout(3000) // timeout 3 seg
        });
        
        if (response.ok) {
            // Conexión OK
            if (connectionLost) {
                // Recuperación exitosa
                connectionLost = false;
                reconnectAttempts = 0;
                hideOverlay();
                
                // Recargar datos del servidor
                await loadDogs();
                await loadStatus();
                
                // Mostrar mensaje de éxito temporal
                const msg = document.getElementById('status-msg');
                msg.style.color = 'green';
                msg.innerText = '✅ Conexión restablecida';
                setTimeout(() => { msg.innerText = ''; }, 3000);
            } else {
                // Actualización normal de status
                const data = await response.json();
                document.getElementById('bat-val').innerText = data.battery;
                document.getElementById('sess-val').innerText = data.pending_sessions;
                if (data.device_code) {
                    document.getElementById('device-code').innerText = data.device_code;
                }
            }
        } else {
            throw new Error('Bad response');
        }
    } catch(e) {
        handleConnectionLost();
    }
}

function handleConnectionLost() {
    if (!connectionLost) {
        connectionLost = true;
        reconnectAttempts = 0;
        showOverlay(
            '⚙️ Calibrando Sistema',
            'WiFi desconectado. BLE activo.<br>La conexión se restablecerá automáticamente al terminar.'
        );
    } else {
        reconnectAttempts++;
        if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
            showOverlay(
                '❌ Conexión Perdida',
                'No se pudo restablecer la conexión.<br>Reinicia el dispositivo e intenta nuevamente.'
            );
        }
    }
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
    // Esta función ahora es manejada por checkConnection()
    // Se mantiene para compatibilidad pero no hace nada
}

// Enviar Configuración
async function startTraining() {
    const btn = document.getElementById('btn-start');
    const msg = document.getElementById('status-msg');
    const dogVal = document.getElementById('dogSelector').value;
    const substanceVal = document.getElementById('substanceSelector').value;
    const distractorsVal = document.getElementById('distractorsCheckbox').checked;
    const contextVal = document.getElementById('contextInput').value;
    const durationVal = parseInt(document.getElementById('durationInput').value) || 30;

    // 1. Guardar datos antes de enviar
    saveFormData();

    // 2. VALIDACIÓN ROBUSTA
    if (!dogVal || dogVal === "" || dogVal === "undefined") {
        alert("⚠️ Por favor selecciona un perro válido.");
        return;
    }

    if (!substanceVal || substanceVal === "") {
        alert("⚠️ Por favor selecciona una sustancia.");
        return;
    }

    if (!contextVal || contextVal.trim() === "") {
        alert("⚠️ Por favor describe el contexto de búsqueda.");
        return;
    }

    if (durationVal < 5 || durationVal > 120) {
        alert("⚠️ La duración debe estar entre 5 y 120 segundos.");
        return;
    }

    // Bloquear UI
    btn.disabled = true;
    btn.innerText = "Enviando...";
    msg.innerText = "";

    // Construir datos de tipo de entrenamiento
    const typeJson = {
        substance: substanceVal,
        distractors: distractorsVal,
        context: contextVal
    };

    // Construir Payload para /api/start
    const payload = {
        dog_code: dogVal,
        mode: currentMode,
        duration_s: durationVal,
        type_json: JSON.stringify(typeJson),
        timestamp: new Date().toISOString()
    };

    console.log("Enviando:", payload);

    // Enviar al Backend
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
            // No limpiar datos aún, por si se calibra después
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

// Iniciar Calibración
async function startCalibration() {
    const btn = document.getElementById('btn-calibrate');
    const msg = document.getElementById('status-msg');
    
    if (!confirm('¿Iniciar proceso de calibración del sistema?\n\nEl WiFi se desconectará temporalmente.')) {
        return;
    }
    
    // Guardar datos del formulario antes de perder conexión
    saveFormData();
    
    // Bloquear UI
    btn.disabled = true;
    btn.innerText = "Calibrando...";
    msg.innerText = "";
    
    try {
        const res = await fetch('/api/calibrate', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'}
        });
        
        const data = await res.json();
        
        if (res.ok) {
            msg.style.color = "green";
            msg.innerText = "✅ Calibración iniciada. Sigue las instrucciones del dispositivo.";
            
            // Mostrar overlay preventivamente (WiFi se caerá pronto)
            setTimeout(() => {
                connectionLost = true;
                showOverlay(
                    '⚙️ Calibrando Sistema',
                    'WiFi desconectado. BLE activo.<br>La conexión se restablecerá automáticamente al terminar.'
                );
            }, 2000);
        } else {
            throw new Error(data.msg || "Error al iniciar calibración");
        }
    } catch (error) {
        msg.style.color = "red";
        msg.innerText = "❌ Error: " + error.message;
        btn.innerText = "CALIBRAR SISTEMA";
        btn.disabled = false;
    }
}