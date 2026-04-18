let port;
let writer;
const connectBtn = document.getElementById('connect-btn');
const statusDiv = document.getElementById('connection-status');
const slidersContainer = document.getElementById('sliders-container');

// Servo details
const numServos = 7;
const defaultAngle = 90;

// Initialize Sliders
function createSliders() {
    for (let i = 1; i <= numServos; i++) {
        let name = "Servo " + i;
        // In Arduino Array: Index 4 maps to slider 5, Index 5 maps to slider 6
        if (i === 5) name = "Servo 5 (Arm 4a Primary)";
        if (i === 6) name = "Servo 6 (Arm 4b Mirrors)";

        const sliderGroup = document.createElement('div');
        sliderGroup.className = 'slider-group';
        
        sliderGroup.innerHTML = `
            <div class="slider-header">
                <span>${name}</span>
                <span class="slider-val" id="val-${i}">${defaultAngle}°</span>
            </div>
            <input type="range" id="slider-${i}" min="0" max="180" value="${defaultAngle}" ${i === 6 ? 'disabled' : ''}>
        `;
        slidersContainer.appendChild(sliderGroup);

        const slider = document.getElementById(`slider-${i}`);
        const valSpan = document.getElementById(`val-${i}`);

        if (i !== 6) {
            slider.addEventListener('input', (e) => {
                const val = e.target.value;
                valSpan.textContent = `${val}°`;
                sendSerialCommand(`S${i}:${val}\n`);
                
                // If it's servo 5, visibly update servo 6 slider as well 
                if (i === 5) {
                    const s6Slider = document.getElementById('slider-6');
                    const s6Val = document.getElementById('val-6');
                    s6Slider.value = val;
                    s6Val.textContent = `${val}°`;
                }
            });
        }
    }
}

// Attach listeners to pose buttons
document.querySelectorAll('.pose-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const pose = btn.getAttribute('data-pose');
        // sending just the character will remain compatible with original Arduino code
        sendSerialCommand(pose + "\n"); 
    });
});

createSliders();

// Web Serial API Implementation
async function connectSerial() {
    if ('serial' in navigator) {
        try {
            port = await navigator.serial.requestPort();
            await port.open({ baudRate: 9600 }); // Default baud rate for many Arduino projects
            
            const textEncoder = new TextEncoderStream();
            const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
            writer = textEncoder.writable.getWriter();
            
            statusDiv.textContent = 'Connected';
            statusDiv.className = 'status connected';
            connectBtn.textContent = 'Disconnect';
        } catch (err) {
            console.error('There was an error opening the serial port:', err);
            statusDiv.textContent = 'Connection Failed: ' + err.message;
        }
    } else {
        alert('Web Serial API is not supported in your browser. Please use Chrome or Edge.');
    }
}

async function disconnectSerial() {
    if (writer) {
        await writer.close();
        writer.releaseLock();
    }
    if (port) {
        await port.close();
    }
    port = null;
    statusDiv.textContent = 'Disconnected';
    statusDiv.className = 'status disconnected';
    connectBtn.textContent = 'Connect via USB';
}

connectBtn.addEventListener('click', () => {
    if (port) {
        disconnectSerial();
    } else {
        connectSerial();
    }
});

async function sendSerialCommand(command) {
    if (port && writer) {
        try {
            await writer.write(command);
            console.log(`Sent: ${command}`);
        } catch (error) {
            console.error("Error writing to serial port:", error);
        }
    } else {
        console.warn(`Cannot send '${command}', not connected to Serial port.`);
    }
}
