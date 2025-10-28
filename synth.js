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

// Create keyboard UI
function createKeyboard() {
    const keyboard = document.getElementById('keyboard');
    const notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B', 'C5'];
    const keys = ['A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K'];
    
    notes.forEach((note, index) => {
        const keyElement = document.createElement('div');
        keyElement.className = note.includes('#') ? 'key black' : 'key';
        keyElement.dataset.note = note;
        keyElement.innerHTML = `<span>${keys[index]}</span>`;
        
        // Mouse events
        keyElement.addEventListener('mousedown', () => {
            playNoteByName(note);
            keyElement.classList.add('active');
        });
        
        keyElement.addEventListener('mouseup', () => {
            stopNoteByName(note);
            keyElement.classList.remove('active');
        });
        
        keyElement.addEventListener('mouseleave', () => {
            stopNoteByName(note);
            keyElement.classList.remove('active');
        });
        
        keyboard.appendChild(keyElement);
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

// Settings controls
document.getElementById('waveform').addEventListener('change', (e) => {
    synth.updateSettings('waveform', e.target.value);
});

document.getElementById('volume').addEventListener('input', (e) => {
    const value = e.target.value / 100;
    synth.updateSettings('volume', value);
    document.getElementById('volume-value').textContent = `${e.target.value}%`;
});

document.getElementById('attack').addEventListener('input', (e) => {
    const value = e.target.value / 1000;
    synth.updateSettings('attack', value);
    document.getElementById('attack-value').textContent = `${e.target.value}ms`;
});

document.getElementById('release').addEventListener('input', (e) => {
    const value = e.target.value / 1000;
    synth.updateSettings('release', value);
    document.getElementById('release-value').textContent = `${e.target.value}ms`;
});

document.getElementById('filter').addEventListener('input', (e) => {
    const value = parseInt(e.target.value);
    synth.updateSettings('filterFrequency', value);
    document.getElementById('filter-value').textContent = `${value}Hz`;
});

// Initialize keyboard on load
createKeyboard();
