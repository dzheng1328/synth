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

// Note frequencies (A4 = 440Hz)
const noteFrequencies = {
    'C': 261.63,
    'C#': 277.18,
    'D': 293.66,
    'D#': 311.13,
    'E': 329.63,
    'F': 349.23,
    'F#': 369.99,
    'G': 392.00,
    'G#': 415.30,
    'A': 440.00,
    'A#': 466.16,
    'B': 493.88,
    'C5': 523.25
};

// Keyboard mapping
const keyboardMap = {
    'a': 'C',
    'w': 'C#',
    's': 'D',
    'e': 'D#',
    'd': 'E',
    'f': 'F',
    't': 'F#',
    'g': 'G',
    'y': 'G#',
    'h': 'A',
    'u': 'A#',
    'j': 'B',
    'k': 'C5'
};

// Initialize synthesizer
const synth = new Synthesizer();

// Create keyboard UI with proper black key positions
function createKeyboard() {
    const keyboard = document.getElementById('keyboard');
    
    // Piano layout: C C# D D# E F F# G G# A A# B C
    // Black keys only between: C-D, D-E, F-G, G-A, A-B
    const layout = [
        { note: 'C', key: 'A', hasBlack: true, blackNote: 'C#', blackKey: 'W' },
        { note: 'D', key: 'S', hasBlack: true, blackNote: 'D#', blackKey: 'E' },
        { note: 'E', key: 'D', hasBlack: false },
        { note: 'F', key: 'F', hasBlack: true, blackNote: 'F#', blackKey: 'T' },
        { note: 'G', key: 'G', hasBlack: true, blackNote: 'G#', blackKey: 'Y' },
        { note: 'A', key: 'H', hasBlack: true, blackNote: 'A#', blackKey: 'U' },
        { note: 'B', key: 'J', hasBlack: false },
        { note: 'C5', key: 'K', hasBlack: false }
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
        synth.playNote(frequency);
    }
}

// Stop note by name
function stopNoteByName(noteName) {
    const frequency = noteFrequencies[noteName];
    if (frequency) {
        synth.stopNote(frequency);
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
        this.updateIndicator();
        this.onChange(value);
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

new KnobController(
    document.getElementById('waveform-knob'),
    0, 3, 2,
    (value) => {
        const index = Math.round(value);
        synth.updateSettings('waveform', waveforms[index]);
        waveDisplay.textContent = waveformLabels[index];
    }
);

// Volume knob
new KnobController(
    document.getElementById('volume-knob'),
    0, 100, 50,
    (value) => {
        synth.updateSettings('volume', value / 100);
    }
);

// Attack knob
new KnobController(
    document.getElementById('attack-knob'),
    0, 100, 5,
    (value) => {
        synth.updateSettings('attack', value / 1000);
    }
);

// Release knob
new KnobController(
    document.getElementById('release-knob'),
    0, 200, 30,
    (value) => {
        synth.updateSettings('release', value / 100);
    }
);

// Filter knob
new KnobController(
    document.getElementById('filter-knob'),
    0, 100, 50,
    (value) => {
        const freq = 100 + (value / 100) * 9900; // 100Hz to 10kHz
        synth.updateSettings('filterFrequency', freq);
    }
);

// Initialize keyboard on load
createKeyboard();
