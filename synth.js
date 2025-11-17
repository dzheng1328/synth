// Synthesizer Engine Wrapper (uses AdvancedSynthesizer)
class Synthesizer {
    constructor() {
        // Create polyphonic engine
        this.engine = null;
        
        // Settings (maintained for compatibility)
        this.settings = {
            waveform: 'sawtooth',
            volume: 0.5,
            attack: 0.05,
            decay: 0.1,
            sustain: 0.7,
            release: 0.3,
            filterFrequency: 5000,
            resonance: 1
        };
        
        // Track active notes by frequency for compatibility
        this.activeNotes = new Map();
        
        // Initialize audio on user interaction
        this.initialized = false;
    }
    
    init() {
        if (this.initialized) return;
        
        // Create the advanced polyphonic engine
        this.engine = new AdvancedSynthesizer();
        
        // Apply initial settings
        this.engine.setWaveform(this.settings.waveform);
        this.engine.setVolume(this.settings.volume);
        this.engine.setFilterCutoff(this.settings.filterFrequency);
        this.engine.setFilterResonance(this.settings.resonance);
        this.engine.updateEnvelope('amp', {
            attack: this.settings.attack,
            decay: this.settings.decay,
            sustain: this.settings.sustain,
            release: this.settings.release
        });
        
        this.initialized = true;
    }
    
    playNote(frequency, velocity = 1) {
        if (!this.initialized) this.init();
        
        // Convert frequency to MIDI note number
        const midiNote = this.frequencyToMidi(frequency);
        
        // Play note on engine
        this.engine.noteOn(midiNote, frequency, velocity);
        
        // Track for compatibility
        this.activeNotes.set(frequency, midiNote);
        
        return frequency;
    }
    
    stopNote(frequency) {
        if (!this.activeNotes.has(frequency)) return;
        
        const midiNote = this.activeNotes.get(frequency);
        this.engine.noteOff(midiNote);
        
        // Clean up tracking
        this.activeNotes.delete(frequency);
    }
    
    updateSettings(setting, value) {
        if (!this.initialized) this.init();
        
        this.settings[setting] = value;
        
        // Map settings to engine methods
        switch(setting) {
            case 'waveform':
                this.engine.setWaveform(value);
                break;
            case 'volume':
                this.engine.setVolume(value);
                break;
            case 'attack':
                this.engine.updateEnvelope('amp', { attack: value });
                break;
            case 'decay':
                this.engine.updateEnvelope('amp', { decay: value });
                break;
            case 'sustain':
                this.engine.updateEnvelope('amp', { sustain: value });
                break;
            case 'release':
                this.engine.updateEnvelope('amp', { release: value });
                break;
            case 'filterFrequency':
                this.engine.setFilterCutoff(value);
                break;
            case 'resonance':
                this.engine.setFilterResonance(value);
                break;
        }
    }
    
    // Helper: Convert frequency to MIDI note number
    frequencyToMidi(frequency) {
        return Math.round(69 + 12 * Math.log2(frequency / 440));
    }
    
    // Expose engine for direct access if needed
    getEngine() {
        if (!this.initialized) this.init();
        return this.engine;
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

// Attack knob (0-1000ms)
window.knobControllers.attack = new KnobController(
    document.getElementById('attack-knob'),
    0, 100, 5,
    (value) => {
        synth.updateSettings('attack', value / 1000); // Convert to seconds
    }
);

// Release knob (0-2s)
window.knobControllers.release = new KnobController(
    document.getElementById('release-knob'),
    0, 200, 30,
    (value) => {
        synth.updateSettings('release', value / 100); // Convert to seconds
    }
);

// Filter knob (100Hz to 10kHz)
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
