document.addEventListener('DOMContentLoaded', () => {
    loadDogs();
});

// 1. GET: Pedir lista de perros
async function loadDogs() {
    try {
        const response = await fetch('/api/dogs');
        const dogs = await response.json();
        
        const select = document.getElementById('dogSelector');
        select.innerHTML = ''; // Limpiar
        
        dogs.forEach(dog => {
            const opt = document.createElement('option');
            opt.value = dog.code;
            opt.innerText = dog.name;
            select.appendChild(opt);
        });
    } catch (e) {
        document.getElementById('status').innerText = "Error cargando perros";
    }
}

// 2. POST: Enviar configuración
async function startTraining() {
    const payload = {
        dog_code: document.getElementById('dogSelector').value,
        temp: document.getElementById('tempInput').value,
        timestamp: new Date().toISOString()
    };

    const response = await fetch('/api/start', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    });

    if (response.ok) {
        alert("¡Entrenamiento Iniciado!");
    } else {
        alert("Error al iniciar");
    }
}