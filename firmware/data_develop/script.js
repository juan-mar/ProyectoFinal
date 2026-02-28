// Estado global para el modo seleccionado
let currentMode = 'manual';
let connectionLost = false;
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 60; // 5 minutos (60 intentos x 5 seg)
let trainingInProgress = false;
let lastPendingSessions = 0;

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
    document.getElementById('timeoutInput').addEventListener('input', (e) => {
        document.getElementById('timeoutHelper').innerText = e.target.value + ' segundos';
        saveFormData();
    });
});

// Cambiar modo (Manual / Auto)
function setMode(mode) {
    currentMode = mode;
    // Actualizar clases visuales de los botones
    document.getElementById('btn-manual').className = (mode === 'manual') ? 'mode-btn active' : 'mode-btn';
    document.getElementById('btn-auto').className = (mode === 'auto') ? 'mode-btn active' : 'mode-btn';
    
    // Ocultar o mostrar el campo de timeout (exclusivo Auto)
    const timeoutCard = document.getElementById('timeoutCard');
    if (mode === 'manual') {
        timeoutCard.style.display = 'none';
    } else {
        timeoutCard.style.display = 'block';
    }

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
        timeout: parseInt(document.getElementById('timeoutInput').value) || 120,
        mode: currentMode,
        timestamp: Date.now()
    };
    localStorage.setItem('trainingFormData', JSON.stringify(data));
}

function restoreFormData() {
    const saved = localStorage.getItem('trainingFormData');
    if (!saved) {
        // Si no hay datos, forzamos que arranque en manual
        setMode('manual');
        return;
    }
    
    try {
        const data = JSON.parse(saved);
        // Solo restaurar si es reciente (< 1 hora)
        if (Date.now() - data.timestamp < 3600000) {
            currentMode = data.mode || 'manual';
            setMode(currentMode); // Esto ya se encarga de mostrar/ocultar el timeout
            
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

            if (data.timeout) {
                const timeoutInput = document.getElementById('timeoutInput');
                timeoutInput.value = data.timeout;
                document.getElementById('timeoutHelper').innerText = data.timeout + ' segundos';
            }
        } else {
            setMode('manual'); // Si caducó, arranca en manual
        }
    } catch(e) {
        console.error('Error restaurando datos:', e);
        setMode('manual');
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
            const data = await response.json();
            
            // Actualizar UI con datos de status
            document.getElementById('bat-val').innerText = data.battery;
            document.getElementById('sess-val').innerText = data.pending_sessions;
            if (data.device_code) {
                document.getElementById('device-code').innerText = data.device_code;
            }
            
            // Detectar cambios en sesiones pendientes (fin de entrenamiento)
            if (trainingInProgress && lastPendingSessions !== data.pending_sessions) {
                lastPendingSessions = data.pending_sessions;
                // Si cambió el counter, probablemente terminó algo
                resetTrainingButton();
            }
            
            if (connectionLost) {
                connectionLost = false;
                reconnectAttempts = 0;
                hideOverlay();
                
                // Resetear botón de entrenamiento al reconectar
                resetTrainingButton();
                
                const msg = document.getElementById('status-msg');
                msg.style.color = 'green';
                msg.innerText = '✅ Conexión restablecida';
                setTimeout(() => { msg.innerText = ''; }, 3000);
            }
        } else {
            throw new Error('Bad response');
        }
    } catch(e) {
        handleConnectionLost();
    }
}

function resetTrainingButton() {
    const btn = document.getElementById('btn-start');
    const msg = document.getElementById('status-msg');
    
    btn.disabled = false;
    btn.innerText = "INICIAR ENTRENAMIENTO";
    trainingInProgress = false;
    
    // Mostrar mensaje de estado limpio
    msg.innerText = "";
}

function handleConnectionLost() {
    if (!connectionLost) {
        connectionLost = true;
        reconnectAttempts = 0;
        showOverlay(
            '📡 Dispositivo Desconectado',
            'WiFi desconectado. BLE activo.<br>Intentando reconectar automáticamente...'
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

async function loadDogs() {
    const selector = document.getElementById('dogSelector');
    
    try {
        const response = await fetch('/api/dogs');
        const dogs = await response.json();
        
        selector.innerHTML = ''; 

        if (dogs.length === 0) {
            selector.innerHTML = '<option value="">⚠️ Sincroniza para descargar perros</option>';
            return;
        }

        dogs.forEach(dog => {
            let opt = document.createElement('option');
            opt.value = dog.dog_code; 
            opt.text = dog.name;
            selector.add(opt);
        });
        
        if(dogs.length > 0) selector.selectedIndex = 0;

    } catch (error) {
        console.error("Error cargando perros:", error);
        selector.innerHTML = '<option value="">Error de conexión</option>';
    }
}

async function loadStatus() {
    // Manejado por checkConnection
}

async function startTraining() {
    const btn = document.getElementById('btn-start');
    const msg = document.getElementById('status-msg');
    const dogVal = document.getElementById('dogSelector').value;
    const substanceVal = document.getElementById('substanceSelector').value;
    const distractorsVal = document.getElementById('distractorsCheckbox').checked;
    const contextVal = document.getElementById('contextInput').value;
    const durationVal = parseInt(document.getElementById('durationInput').value) || 30;
    const timeoutVal = parseInt(document.getElementById('timeoutInput').value) || 120;

    saveFormData();

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

    // Validamos la duración siempre
    if (durationVal < 5 || durationVal > 120) {
        alert("⚠️ La duración debe estar entre 5 y 120 segundos.");
        return;
    }

    // Validamos el timeout solo en modo Auto
    if (currentMode === 'auto' && (timeoutVal < 5 || timeoutVal > 120)) {
        alert("⚠️ El timeout debe estar entre 5 y 120 segundos.");
        return;
    }

    btn.disabled = true;
    btn.innerText = "Enviando...";
    msg.innerText = "";

    const typeJson = {
        substance: substanceVal,
        distractors: distractorsVal,
        context: contextVal
    };

    const payload = {
        dog_code: dogVal,
        mode: currentMode,
        duration_s: durationVal,
        type_json: JSON.stringify(typeJson),
        timestamp: new Date().toISOString()
    };

    if (currentMode === 'auto') {
        payload.timeout_s = timeoutVal;
    }

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
            
            // Marcar que hay entrenamiento en progreso
            trainingInProgress = true;
            lastPendingSessions = 0; // Reset para detectar cambios después
            
            // El botón se resetea cuando se detecte el fin del entrenamiento
            // o cuando se reconecte después de una desconexión
        } else {
            throw new Error(data.msg || "Error desconocido");
        }
    } catch (error) {
        msg.style.color = "red";
        msg.innerText = "❌ Error: " + error.message;
        btn.innerText = "REINTENTAR";
        btn.disabled = false;
        trainingInProgress = false;
    }
}

async function startCalibration() {
    const btn = document.getElementById('btn-calibrate');
    const msg = document.getElementById('status-msg');
    
    if (!confirm('¿Iniciar proceso de calibración del sistema?\n\nEl WiFi se desconectará temporalmente.')) {
        return;
    }
    
    saveFormData();
    
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
            
            setTimeout(() => {
                connectionLost = true;
                showOverlay(
                    '📡 Dispositivo en Calibración',
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