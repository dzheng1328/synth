/**
 * Performance Controls UI
 * Connects transport, arpeggiator, and delay UI to the performance engine
 */

// Global performance state
let performanceClock = null;
let performanceArp = null;
let performanceDelay = null;
let isPlaying = false;

// Initialize performance features when DOM is ready
function initPerformanceControls() {
    // Wait for synth to be initialized
    if (!synth.initialized) {
        synth.init();
    }
    
    const engine = synth.getEngine();
    
    // Create Clock
    performanceClock = new Clock();
    performanceClock.setBPM(120);
    
    // Create Arpeggiator
    performanceArp = new Arpeggiator(engine.voiceAllocator, performanceClock);
    performanceArp.setPattern('up');
    performanceArp.setOctaves(1);
    performanceArp.setRate(16); // 16th notes
    performanceArp.setGate(0.75);
    
    // Create Loop Delay
    performanceDelay = new LoopBasedDelay(engine.audioContext);
    performanceDelay.setTime(0.5);
    performanceDelay.setFeedback(0.6);
    performanceDelay.setMix(0.3);
    
    // Connect delay to engine output
    engine.masterGain.disconnect();
    engine.masterGain.connect(performanceDelay.input);
    performanceDelay.output.connect(engine.audioContext.destination);
    
    // Initialize all UI controls
    initTransportControls();
    initArpeggiatorControls();
    initDelayControls();
    initVoiceMeter();
    
    // Keyboard shortcuts
    document.addEventListener('keydown', handlePerformanceShortcuts);
    
    console.log('🎹 Performance controls initialized');
}

// ============== TRANSPORT CONTROLS ==============

function initTransportControls() {
    const playStopBtn = document.getElementById('play-stop-btn');
    const tapTempoBtn = document.getElementById('tap-tempo-btn');
    const bpmDisplay = document.getElementById('bpm-value');
    const clockIndicator = document.getElementById('clock-indicator');
    
    // Play/Stop
    playStopBtn.addEventListener('click', togglePlayStop);
    
    // Tap Tempo
    tapTempoBtn.addEventListener('click', () => {
        const newBPM = performanceClock.tap();
        if (newBPM) {
            bpmDisplay.textContent = Math.round(newBPM);
        }
    });
    
    // Editable BPM - Click to edit
    bpmDisplay.addEventListener('click', () => {
        const currentBPM = parseInt(bpmDisplay.textContent);
        const input = document.createElement('input');
        input.type = 'number';
        input.min = '40';
        input.max = '240';
        input.value = currentBPM;
        input.className = 'bpm-input';
        input.style.cssText = 'width: 50px; background: #000; color: #00ff00; border: 2px solid #ff4400; padding: 2px 5px; font-size: 20px; font-family: "Courier New", monospace; text-align: center; border-radius: 2px;';
        
        bpmDisplay.textContent = '';
        bpmDisplay.appendChild(input);
        bpmDisplay.classList.add('editing');
        input.focus();
        input.select();
        
        const finishEditing = () => {
            const newBPM = parseInt(input.value);
            if (newBPM >= 40 && newBPM <= 240) {
                performanceClock.setBPM(newBPM);
                bpmDisplay.textContent = newBPM;
            } else {
                bpmDisplay.textContent = currentBPM;
            }
            bpmDisplay.classList.remove('editing');
        };
        
        input.addEventListener('blur', finishEditing);
        input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') finishEditing();
            if (e.key === 'Escape') {
                bpmDisplay.textContent = currentBPM;
                bpmDisplay.classList.remove('editing');
            }
        });
    });
    
    // Clock pulse visualization
    performanceClock.subscribe((step) => {
        // Flash on every quarter note (step 0, 4, 8, 12)
        if (step % 4 === 0) {
            clockIndicator.classList.add('active');
            setTimeout(() => clockIndicator.classList.remove('active'), 100);
        }
    });
}

function togglePlayStop() {
    const playStopBtn = document.getElementById('play-stop-btn');
    const icon = playStopBtn.querySelector('.icon');
    const label = playStopBtn.querySelector('.label');
    
    isPlaying = !isPlaying;
    
    if (isPlaying) {
        performanceClock.start();
        playStopBtn.classList.add('playing');
        icon.textContent = '⏸';
        label.textContent = 'STOP';
    } else {
        performanceClock.stop();
        playStopBtn.classList.remove('playing');
        icon.textContent = '▶';
        label.textContent = 'PLAY';
    }
}

// ============== ARPEGGIATOR CONTROLS ==============

function initArpeggiatorControls() {
    const enableBtn = document.getElementById('arp-enable-btn');
    const latchBtn = document.getElementById('arp-latch-btn');
    const patternBtns = document.querySelectorAll('.pattern-btn');
    
    // Enable/Disable
    enableBtn.addEventListener('click', () => {
        const isActive = enableBtn.classList.toggle('active');
        if (isActive) {
            performanceArp.enable();
        } else {
            performanceArp.disable();
        }
    });
    
    // Pattern selection
    patternBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            patternBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            const pattern = btn.dataset.pattern;
            performanceArp.setPattern(pattern);
        });
    });
    
    // Latch mode
    latchBtn.addEventListener('click', () => {
        const isActive = latchBtn.classList.toggle('active');
        performanceArp.setLatch(isActive);
    });
    
    // Rate knob
    const rateKnob = document.getElementById('arp-rate-knob');
    const rateDisplay = document.getElementById('arp-rate-display');
    const rateValues = [
        { value: 32, label: '1/32' },
        { value: 16, label: '1/16' },
        { value: 8, label: '1/8' },
        { value: 4, label: '1/4' }
    ];
    
    new KnobController(rateKnob, 0, 3, 1, (value) => {
        const index = Math.round(value);
        const rate = rateValues[index];
        performanceArp.setRate(rate.value);
        rateDisplay.textContent = rate.label;
    });
    
    // Octaves knob
    const octavesKnob = document.getElementById('arp-octaves-knob');
    const octavesDisplay = document.getElementById('arp-octaves-display');
    
    new KnobController(octavesKnob, 0, 3, 0, (value) => {
        const octaves = Math.round(value) + 1; // 1-4 octaves
        performanceArp.setOctaves(octaves);
        octavesDisplay.textContent = octaves;
    });
    
    // Gate knob
    const gateKnob = document.getElementById('arp-gate-knob');
    const gateDisplay = document.getElementById('arp-gate-display');
    
    new KnobController(gateKnob, 0, 100, 75, (value) => {
        const gate = value / 100;
        performanceArp.setGate(gate);
        gateDisplay.textContent = Math.round(value) + '%';
    });
}

// ============== DELAY CONTROLS ==============

function initDelayControls() {
    const enableBtn = document.getElementById('delay-enable-btn');
    const freezeBtn = document.getElementById('delay-freeze-btn');
    const syncBtn = document.getElementById('delay-sync-btn');
    const clearBtn = document.getElementById('delay-clear-btn');
    
    // Enable/Disable
    let delayEnabled = true; // Enabled by default
    enableBtn.classList.add('active');
    
    enableBtn.addEventListener('click', () => {
        delayEnabled = !delayEnabled;
        enableBtn.classList.toggle('active');
        if (delayEnabled) {
            performanceDelay.setMix(parseFloat(document.getElementById('delay-mix-knob').dataset.value) / 100);
        } else {
            performanceDelay.setMix(0);
        }
    });
    
    // Freeze button
    let isFrozen = false;
    freezeBtn.addEventListener('click', () => {
        isFrozen = !isFrozen;
        freezeBtn.classList.toggle('active');
        if (isFrozen) {
            performanceDelay.freeze();
        } else {
            performanceDelay.unfreeze();
        }
    });
    
    // Tempo sync
    let tempoSync = false;
    syncBtn.addEventListener('click', () => {
        tempoSync = !tempoSync;
        syncBtn.classList.toggle('active');
        performanceDelay.enableTempoSync(tempoSync);
        
        if (tempoSync) {
            // Update display to show note division
            const timeDisplay = document.getElementById('delay-time-display');
            timeDisplay.textContent = '1/8';
        }
    });
    
    // Clear button
    clearBtn.addEventListener('click', () => {
        performanceDelay.clear();
        // Visual feedback
        clearBtn.style.opacity = '0.5';
        setTimeout(() => clearBtn.style.opacity = '1', 100);
    });
    
    // Time knob
    const timeKnob = document.getElementById('delay-time-knob');
    const timeDisplay = document.getElementById('delay-time-display');
    
    new KnobController(timeKnob, 0, 100, 50, (value) => {
        const time = 0.05 + (value / 100) * 1.95; // 50ms to 2000ms
        performanceDelay.setTime(time);
        if (!tempoSync) {
            timeDisplay.textContent = Math.round(time * 1000) + 'ms';
        }
    });
    
    // Feedback knob
    const feedbackKnob = document.getElementById('delay-feedback-knob');
    const feedbackDisplay = document.getElementById('delay-feedback-display');
    
    new KnobController(feedbackKnob, 0, 100, 60, (value) => {
        const feedback = value / 100;
        performanceDelay.setFeedback(feedback);
        feedbackDisplay.textContent = Math.round(value) + '%';
    });
    
    // Filter knob
    const filterKnob = document.getElementById('delay-filter-knob');
    const filterDisplay = document.getElementById('delay-filter-display');
    
    new KnobController(filterKnob, 0, 100, 100, (value) => {
        const freq = 200 + (value / 100) * 19800; // 200Hz to 20kHz
        performanceDelay.setFilterCutoff(freq);
        
        if (freq >= 1000) {
            filterDisplay.textContent = (freq / 1000).toFixed(1) + 'kHz';
        } else {
            filterDisplay.textContent = Math.round(freq) + 'Hz';
        }
    });
    
    // Mix knob
    const mixKnob = document.getElementById('delay-mix-knob');
    const mixDisplay = document.getElementById('delay-mix-display');
    
    new KnobController(mixKnob, 0, 100, 30, (value) => {
        const mix = value / 100;
        if (delayEnabled) {
            performanceDelay.setMix(mix);
        }
        mixDisplay.textContent = Math.round(value) + '%';
    });
}

// ============== KEYBOARD SHORTCUTS ==============

function handlePerformanceShortcuts(e) {
    // Ignore if typing in input field
    if (e.target.tagName === 'INPUT') return;
    
    switch(e.key.toLowerCase()) {
        case ' ': // Space - Play/Stop
            e.preventDefault();
            togglePlayStop();
            break;
        
        case 't': // T - Tap Tempo
            if (!keyboardMap[e.key.toLowerCase()]) { // Don't conflict with note
                document.getElementById('tap-tempo-btn').click();
            }
            break;
        
        case 'a': // A - Toggle Arpeggiator
            if (!keyboardMap[e.key.toLowerCase()]) {
                document.getElementById('arp-enable-btn').click();
            }
            break;
        
        case 'f': // F - Freeze Delay
            if (!keyboardMap[e.key.toLowerCase()]) {
                document.getElementById('delay-freeze-btn').click();
            }
            break;
        
        case 'l': // L - Latch
            document.getElementById('arp-latch-btn').click();
            break;
    }
}

// ============== VOICE METER ==============

function initVoiceMeter() {
    const voiceMeter = document.getElementById('voice-meter');
    const voiceCount = document.getElementById('voice-count');
    const voiceLEDs = voiceMeter.querySelectorAll('.voice-led');
    
    // Get the engine and voice allocator
    const engine = synth.getEngine();
    const voiceAllocator = engine.voiceAllocator;
    
    // Update voice meter display
    function updateVoiceMeter() {
        const activeVoices = voiceAllocator.getActiveVoiceCount();
        
        // Update LEDs
        voiceLEDs.forEach((led, index) => {
            if (index < activeVoices) {
                led.classList.add('active');
            } else {
                led.classList.remove('active');
            }
        });
        
        // Update counter
        voiceCount.textContent = `${activeVoices}/8`;
        
        // Check again soon
        requestAnimationFrame(updateVoiceMeter);
    }
    
    // Start monitoring
    updateVoiceMeter();
}

// ============== EXPORT ==============

// Export for global access
window.performanceControls = {
    clock: () => performanceClock,
    arp: () => performanceArp,
    delay: () => performanceDelay,
    isPlaying: () => isPlaying,
    togglePlay: togglePlayStop
};
