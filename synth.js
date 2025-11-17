// Synthesizer Engine
class Synthesizer {
    constructor() {
        // Create audio context
        this.audioContext = null;
        this.masterGain = null;
        this.filter = null;
        
        // Active notes tracking
        this.activeNotes = new Map();
        
        // Settings
        this.settings = {
            waveform: 'sawtooth',
            volume: 0.5,
            attack: 0.05,
            release: 0.3,
            filterFrequency: 5000
        };
        
        // Initialize audio on user interaction
        this.initialized = false;
    }
    
    init() {
        if (this.initialized) return;
        
        // Create audio context
        this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        
        // Create master gain (volume control)
        this.masterGain = this.audioContext.createGain();
        this.masterGain.gain.value = this.settings.volume;
        
        // Create filter
        this.filter = this.audioContext.createBiquadFilter();
        this.filter.type = 'lowpass';
        this.filter.frequency.value = this.settings.filterFrequency;
        this.filter.Q.value = 1;
        
        // Connect filter -> master gain -> destination
        this.filter.connect(this.masterGain);
        this.masterGain.connect(this.audioContext.destination);
        
        this.initialized = true;
    }
    
    playNote(frequency, velocity = 1) {
        if (!this.initialized) this.init();
        
        const now = this.audioContext.currentTime;
        
        // Create oscillator
        const oscillator = this.audioContext.createOscillator();
        oscillator.type = this.settings.waveform;
        oscillator.frequency.value = frequency;
        
        // Create gain node for this note (for envelope)
        const noteGain = this.audioContext.createGain();
        noteGain.gain.value = 0;
        
        // Connect: oscillator -> note gain -> filter
        oscillator.connect(noteGain);
        noteGain.connect(this.filter);
        
        // Apply attack envelope
        noteGain.gain.setValueAtTime(0, now);
        noteGain.gain.linearRampToValueAtTime(velocity, now + this.settings.attack);
        
        // Start oscillator
        oscillator.start(now);
        
        // Store active note
        this.activeNotes.set(frequency, { oscillator, noteGain });
        
        return frequency;
    }
    
    stopNote(frequency) {
        if (!this.activeNotes.has(frequency)) return;
        
        const { oscillator, noteGain } = this.activeNotes.get(frequency);
        const now = this.audioContext.currentTime;
        
        // Apply release envelope
        noteGain.gain.cancelScheduledValues(now);
        noteGain.gain.setValueAtTime(noteGain.gain.value, now);
        noteGain.gain.linearRampToValueAtTime(0, now + this.settings.release);
        
        // Stop oscillator after release
        oscillator.stop(now + this.settings.release);
        
        // Clean up
        this.activeNotes.delete(frequency);
    }
    
    updateSettings(setting, value) {
        this.settings[setting] = value;
        
        // Update real-time parameters
        if (setting === 'volume' && this.masterGain) {
            this.masterGain.gain.value = value;
        }
        if (setting === 'filterFrequency' && this.filter) {
            this.filter.frequency.value = value;
        }
    }
}

// Note frequencies (C3 to C5 - 3 octaves)
const noteFrequencies = {
    'C3': 130.81, 'C#3': 138.59, 'D3': 146.83, 'D#3': 155.56, 'E3': 164.81, 'F3': 174.61, 'F#3': 185.00, 'G3': 196.00, 'G#3': 207.65, 'A3': 220.00, 'A#3': 233.08, 'B3': 246.94,
    'C4': 261.63, 'C#4': 277.18, 'D4': 293.66, 'D#4': 311.13, 'E4': 329.63, 'F4': 349.23, 'F#4': 369.99, 'G4': 392.00, 'G#4': 415.30, 'A4': 440.00, 'A#4': 466.16, 'B4': 493.88,
    'C5': 523.25
};

// Keyboard mapping for 2+ octaves
const keyboardMap = {
    'z': 'C3', 's': 'C#3', 'x': 'D3', 'd': 'D#3', 'c': 'E3', 'v': 'F3', 'g': 'F#3', 'b': 'G3', 'h': 'G#3', 'n': 'A3', 'j': 'A#3', 'm': 'B3',
    'q': 'C4', '2': 'C#4', 'w': 'D4', '3': 'D#4', 'e': 'E4', 'r': 'F4', '5': 'F#4', 't': 'G4', '6': 'G#4', 'y': 'A4', '7': 'A#4', 'u': 'B4',
    'i': 'C5'
};

// Initialize synthesizer
const synth = new Synthesizer();

// Current octave (default is 3, which gives us middle C around 261Hz)
let currentOctave = 3;

// Create keyboard UI with proper black key positions
function createKeyboard() {
    const keyboard = document.getElementById('keyboard');
    
    // Piano layout for 2+ octaves: C3 to C5
    const layout = [
        // Octave 3 (lower)
        { note: 'C3', key: 'Z', hasBlack: true, blackNote: 'C#3', blackKey: 'S' },
        { note: 'D3', key: 'X', hasBlack: true, blackNote: 'D#3', blackKey: 'D' },
        { note: 'E3', key: 'C', hasBlack: false },
        { note: 'F3', key: 'V', hasBlack: true, blackNote: 'F#3', blackKey: 'G' },
        { note: 'G3', key: 'B', hasBlack: true, blackNote: 'G#3', blackKey: 'H' },
        { note: 'A3', key: 'N', hasBlack: true, blackNote: 'A#3', blackKey: 'J' },
        { note: 'B3', key: 'M', hasBlack: false },
        // Octave 4 (middle)
        { note: 'C4', key: 'Q', hasBlack: true, blackNote: 'C#4', blackKey: '2' },
        { note: 'D4', key: 'W', hasBlack: true, blackNote: 'D#4', blackKey: '3' },
        { note: 'E4', key: 'E', hasBlack: false },
        { note: 'F4', key: 'R', hasBlack: true, blackNote: 'F#4', blackKey: '5' },
        { note: 'G4', key: 'T', hasBlack: true, blackNote: 'G#4', blackKey: '6' },
        { note: 'A4', key: 'Y', hasBlack: true, blackNote: 'A#4', blackKey: '7' },
        { note: 'B4', key: 'U', hasBlack: false },
        // High C
        { note: 'C5', key: 'I', hasBlack: false }
    ];
    
    layout.forEach(item => {
        const container = document.createElement('div');
        container.className = 'key-container';
        
        // Create white key
        const whiteKey = document.createElement('div');
        whiteKey.className = 'key';
        whiteKey.dataset.note = item.note;
        whiteKey.innerHTML = `<span>${item.key}</span>`;
        
        // Mouse events for white key
        whiteKey.addEventListener('mousedown', () => {
            playNoteByName(item.note);
            whiteKey.classList.add('active');
        });
        
        whiteKey.addEventListener('mouseup', () => {
            stopNoteByName(item.note);
            whiteKey.classList.remove('active');
        });
        
        whiteKey.addEventListener('mouseleave', () => {
            stopNoteByName(item.note);
            whiteKey.classList.remove('active');
        });
        
        container.appendChild(whiteKey);
        
        // Create black key if needed
        if (item.hasBlack) {
            const blackKey = document.createElement('div');
            blackKey.className = 'key black';
            blackKey.dataset.note = item.blackNote;
            blackKey.innerHTML = `<span>${item.blackKey}</span>`;
            
            // Mouse events for black key
            blackKey.addEventListener('mousedown', (e) => {
                e.stopPropagation();
                playNoteByName(item.blackNote);
                blackKey.classList.add('active');
            });
            
            blackKey.addEventListener('mouseup', () => {
                stopNoteByName(item.blackNote);
                blackKey.classList.remove('active');
            });
            
            blackKey.addEventListener('mouseleave', () => {
                stopNoteByName(item.blackNote);
                blackKey.classList.remove('active');
            });
            
            container.appendChild(blackKey);
        }
        
        keyboard.appendChild(container);
    });
}

// Play note by name
function playNoteByName(noteName) {
    const frequency = noteFrequencies[noteName];
    if (frequency) {
        // Transpose by octave (each octave doubles or halves the frequency)
        const transposedFreq = frequency * Math.pow(2, currentOctave - 3);
        synth.playNote(transposedFreq);
    }
}

// Stop note by name
function stopNoteByName(noteName) {
    const frequency = noteFrequencies[noteName];
    if (frequency) {
        const transposedFreq = frequency * Math.pow(2, currentOctave - 3);
        synth.stopNote(transposedFreq);
    }
}

// Keyboard input
const pressedKeys = new Set();

document.addEventListener('keydown', (e) => {
    if (pressedKeys.has(e.key.toLowerCase())) return;
    pressedKeys.add(e.key.toLowerCase());
    
    const note = keyboardMap[e.key.toLowerCase()];
    if (note) {
        playNoteByName(note);
        const keyElement = document.querySelector(`[data-note="${note}"]`);
        if (keyElement) keyElement.classList.add('active');
    }
});

document.addEventListener('keyup', (e) => {
    pressedKeys.delete(e.key.toLowerCase());
    
    const note = keyboardMap[e.key.toLowerCase()];
    if (note) {
        stopNoteByName(note);
        const keyElement = document.querySelector(`[data-note="${note}"]`);
        if (keyElement) keyElement.classList.remove('active');
    }
});

// Rotary Knob Controller
class KnobController {
    constructor(element, min, max, initialValue, onChange) {
        this.element = element;
        this.min = min;
        this.max = max;
        this.value = initialValue;
        this.onChange = onChange;
        this.isDragging = false;
        this.startY = 0;
        this.startValue = 0;
        
        this.init();
    }
    
    init() {
        this.updateIndicator();
        
        this.element.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.startY = e.clientY;
            this.startValue = this.value;
            e.preventDefault();
        });
        
        document.addEventListener('mousemove', (e) => {
            if (!this.isDragging) return;
            
            const delta = this.startY - e.clientY;
            const range = this.max - this.min;
            const newValue = Math.max(this.min, Math.min(this.max, this.startValue + (delta / 100) * range));
            
            this.setValue(newValue);
        });
        
        document.addEventListener('mouseup', () => {
            this.isDragging = false;
        });
    }
    
    setValue(value) {
        this.value = value;
        this.element.dataset.value = value;
        this.updateIndicator();
        this.onChange(value);
        
        // Emit change event for project management
        document.dispatchEvent(new CustomEvent('knob-change'));
    }
    
    updateIndicator() {
        const indicator = this.element.querySelector('.knob-indicator');
        const range = this.max - this.min;
        const normalized = (this.value - this.min) / range;
        const degrees = -135 + (normalized * 270); // -135 to +135 degrees
        indicator.style.transform = `translateX(-50%) rotate(${degrees}deg)`;
    }
}

// Waveform knob (discrete values)
const waveforms = ['sine', 'square', 'sawtooth', 'triangle'];
const waveformLabels = ['SINE', 'SQR', 'SAW', 'TRI'];
const waveDisplay = document.getElementById('wave-display');

// Store knob controllers globally for project management
window.knobControllers = {};

window.knobControllers.waveform = new KnobController(
    document.getElementById('waveform-knob'),
    0, 3, 2,
    (value) => {
        const index = Math.round(value);
        synth.updateSettings('waveform', waveforms[index]);
        waveDisplay.textContent = waveformLabels[index];
    }
);

// Volume knob
window.knobControllers.volume = new KnobController(
    document.getElementById('volume-knob'),
    0, 100, 50,
    (value) => {
        synth.updateSettings('volume', value / 100);
    }
);

// Attack knob
window.knobControllers.attack = new KnobController(
    document.getElementById('attack-knob'),
    0, 100, 5,
    (value) => {
        synth.updateSettings('attack', value / 1000);
    }
);

// Release knob
window.knobControllers.release = new KnobController(
    document.getElementById('release-knob'),
    0, 200, 30,
    (value) => {
        synth.updateSettings('release', value / 100);
    }
);

// Filter knob
window.knobControllers.filter = new KnobController(
    document.getElementById('filter-knob'),
    0, 100, 50,
    (value) => {
        const freq = 100 + (value / 100) * 9900; // 100Hz to 10kHz
        synth.updateSettings('filterFrequency', freq);
    }
);

// Initialize keyboard on load
createKeyboard();

// Octave controls
const octaveDisplay = document.getElementById('octave-value');
const octaveUpBtn = document.getElementById('octave-up');
const octaveDownBtn = document.getElementById('octave-down');

function updateOctaveDisplay() {
    octaveDisplay.textContent = currentOctave;
}

octaveUpBtn.addEventListener('click', () => {
    if (currentOctave < 7) {
        currentOctave++;
        updateOctaveDisplay();
    }
});

octaveDownBtn.addEventListener('click', () => {
    if (currentOctave > 0) {
        currentOctave--;
        updateOctaveDisplay();
    }
});

// Keyboard shortcuts for octave ([ and ] keys)
document.addEventListener('keydown', (e) => {
    if (e.key === '[') {
        if (currentOctave > 0) {
            currentOctave--;
            updateOctaveDisplay();
        }
    } else if (e.key === ']') {
        if (currentOctave < 7) {
            currentOctave++;
            updateOctaveDisplay();
        }
    }
});
