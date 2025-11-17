/**
 * Performance Features for Hardware-Style Synthesizer
 * 
 * Includes:
 * - Loop-based delay with live parameter changes
 * - Arpeggiator (up, down, up-down, random patterns)
 * - 16-step sequencer
 * - Performance recorder
 * - Tap tempo
 * - Clock/tempo system
 */

// ============================================================================
// CLOCK - Master tempo system
// ============================================================================

class Clock {
  constructor(bpm = 120) {
    this.bpm = bpm;
    this.running = false;
    this.callbacks = [];
    this.intervalId = null;
    this.step = 0;
    this.tapTimes = [];
  }
  
  start() {
    if (this.running) return;
    
    this.running = true;
    this.step = 0;
    
    const interval = (60 / this.bpm) * 1000 / 4; // 16th notes
    
    this.intervalId = setInterval(() => {
      this.tick();
    }, interval);
  }
  
  stop() {
    this.running = false;
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }
  
  tick() {
    this.step = (this.step + 1) % 64; // 4 bars of 16th notes
    
    // Notify all subscribers
    this.callbacks.forEach(callback => {
      callback(this.step, this.bpm);
    });
  }
  
  setBPM(bpm) {
    this.bpm = Math.max(40, Math.min(240, bpm));
    
    // Restart if running to apply new tempo
    if (this.running) {
      this.stop();
      this.start();
    }
  }
  
  // Tap tempo: tap button in rhythm to set BPM
  tap() {
    const now = Date.now();
    this.tapTimes.push(now);
    
    // Keep only last 4 taps
    if (this.tapTimes.length > 4) {
      this.tapTimes.shift();
    }
    
    // Calculate average interval
    if (this.tapTimes.length >= 2) {
      const intervals = [];
      for (let i = 1; i < this.tapTimes.length; i++) {
        intervals.push(this.tapTimes[i] - this.tapTimes[i - 1]);
      }
      
      const avgInterval = intervals.reduce((a, b) => a + b) / intervals.length;
      const bpm = Math.round(60000 / avgInterval);
      
      this.setBPM(bpm);
    }
    
    // Reset if too much time since last tap
    setTimeout(() => {
      if (Date.now() - now > 3000) {
        this.tapTimes = [];
      }
    }, 3000);
  }
  
  subscribe(callback) {
    this.callbacks.push(callback);
  }
  
  unsubscribe(callback) {
    this.callbacks = this.callbacks.filter(cb => cb !== callback);
  }
}

// ============================================================================
// LOOP-BASED DELAY (Hardware Style)
// ============================================================================

class LoopBasedDelay {
  constructor(audioContext) {
    this.audioContext = audioContext;
    
    // Delay nodes
    this.delayNode = audioContext.createDelay(10);
    this.feedbackGain = audioContext.createGain();
    this.feedbackFilter = audioContext.createBiquadFilter();
    this.wetGain = audioContext.createGain();
    this.dryGain = audioContext.createGain();
    
    // Input/output
    this.input = audioContext.createGain();
    this.output = audioContext.createGain();
    
    // Parameters
    this.time = 0.5;
    this.feedback = 0.6;
    this.filterCutoff = 2000;
    this.wetDry = 0.5;
    this.frozen = false;
    this.tempoSync = false;
    this.bpm = 120;
    this.syncDivision = 4; // 1=whole, 2=half, 4=quarter, 8=eighth, 16=sixteenth
    
    // Build signal chain
    this.setupRouting();
    this.updateParameters();
  }
  
  setupRouting() {
    // Input splits to delay and dry
    this.input.connect(this.delayNode);
    this.input.connect(this.dryGain);
    
    // Delay feedback loop: delay → filter → feedback → delay
    this.delayNode.connect(this.feedbackFilter);
    this.feedbackFilter.connect(this.feedbackGain);
    this.feedbackGain.connect(this.delayNode); // FEEDBACK LOOP!
    
    // Delay output
    this.delayNode.connect(this.wetGain);
    
    // Mix wet and dry to output
    this.wetGain.connect(this.output);
    this.dryGain.connect(this.output);
  }
  
  updateParameters() {
    this.delayNode.delayTime.value = this.time;
    this.feedbackGain.gain.value = this.feedback;
    this.feedbackFilter.frequency.value = this.filterCutoff;
    this.wetGain.gain.value = this.wetDry;
    this.dryGain.gain.value = 1 - this.wetDry;
  }
  
  // Set delay time (can change while delay is running!)
  setTime(time) {
    if (this.tempoSync) return; // Time is controlled by BPM
    
    this.time = Math.max(0.001, Math.min(10, time));
    
    // Smooth ramp to avoid clicks
    this.delayNode.delayTime.linearRampToValueAtTime(
      this.time,
      this.audioContext.currentTime + 0.05
    );
  }
  
  // Set feedback amount
  setFeedback(feedback) {
    if (this.frozen) return; // Frozen = infinite feedback
    
    this.feedback = Math.max(0, Math.min(0.95, feedback));
    
    this.feedbackGain.gain.linearRampToValueAtTime(
      this.feedback,
      this.audioContext.currentTime + 0.05
    );
  }
  
  // Set filter cutoff (colors each repeat)
  setFilterCutoff(cutoff) {
    this.filterCutoff = Math.max(20, Math.min(20000, cutoff));
    
    this.feedbackFilter.frequency.linearRampToValueAtTime(
      this.filterCutoff,
      this.audioContext.currentTime + 0.05
    );
  }
  
  // Set wet/dry mix
  setWetDry(amount) {
    this.wetDry = Math.max(0, Math.min(1, amount));
    this.wetGain.gain.value = this.wetDry;
    this.dryGain.gain.value = 1 - this.wetDry;
  }
  
  // Freeze delay (hold current repeats forever)
  freeze() {
    this.frozen = true;
    this.feedbackGain.gain.value = 1.0; // Infinite feedback
  }
  
  // Unfreeze delay
  unfreeze() {
    this.frozen = false;
    this.feedbackGain.gain.value = this.feedback;
  }
  
  // Toggle freeze
  toggleFreeze() {
    if (this.frozen) {
      this.unfreeze();
    } else {
      this.freeze();
    }
  }
  
  // Clear delay buffer (kill all repeats)
  clear() {
    this.feedbackGain.gain.value = 0;
    
    setTimeout(() => {
      if (!this.frozen) {
        this.feedbackGain.gain.value = this.feedback;
      }
    }, this.time * 1000 * 3);
  }
  
  // Tempo sync
  enableTempoSync(bpm) {
    this.tempoSync = true;
    this.bpm = bpm;
    this.updateTempoSyncTime();
  }
  
  disableTempoSync() {
    this.tempoSync = false;
  }
  
  updateTempoSyncTime() {
    if (!this.tempoSync) return;
    
    // Calculate time based on BPM and division
    const beatDuration = 60 / this.bpm; // seconds per beat
    const noteDuration = beatDuration * (4 / this.syncDivision);
    
    this.time = noteDuration;
    this.delayNode.delayTime.value = this.time;
  }
  
  setSyncDivision(division) {
    this.syncDivision = division;
    this.updateTempoSyncTime();
  }
  
  updateBPM(bpm) {
    this.bpm = bpm;
    this.updateTempoSyncTime();
  }
  
  connect(destination) {
    this.output.connect(destination);
  }
  
  disconnect() {
    this.output.disconnect();
  }
}

// ============================================================================
// ARPEGGIATOR
// ============================================================================

class Arpeggiator {
  constructor(voiceAllocator, clock) {
    this.voiceAllocator = voiceAllocator;
    this.clock = clock;
    
    this.enabled = false;
    this.heldNotes = []; // {note, frequency, velocity}
    this.currentIndex = 0;
    this.pattern = 'up'; // up, down, up-down, random, played
    this.octaves = 1; // 1-4 octaves
    this.rate = 16; // 16th notes
    this.gate = 0.8; // 0-1, how long note is held
    this.latch = false;
    
    this.lastPlayedNote = null;
    this.direction = 1; // For up-down pattern
    
    // Subscribe to clock
    this.clockCallback = (step) => this.onClockTick(step);
  }
  
  enable() {
    if (this.enabled) return;
    
    this.enabled = true;
    this.clock.subscribe(this.clockCallback);
  }
  
  disable() {
    if (!this.enabled) return;
    
    this.enabled = false;
    this.clock.unsubscribe(this.clockCallback);
    
    // Stop current note
    if (this.lastPlayedNote) {
      this.voiceAllocator.noteOff(this.lastPlayedNote.note);
      this.lastPlayedNote = null;
    }
  }
  
  toggle() {
    if (this.enabled) {
      this.disable();
    } else {
      this.enable();
    }
  }
  
  onClockTick(step) {
    if (!this.enabled || this.heldNotes.length === 0) return;
    
    // Check if we should play on this step (based on rate)
    const division = 16 / this.rate;
    if (step % division !== 0) return;
    
    // Stop previous note
    if (this.lastPlayedNote) {
      this.voiceAllocator.noteOff(this.lastPlayedNote.note);
    }
    
    // Get next note from pattern
    const noteData = this.getNextNote();
    
    if (noteData) {
      // Play note
      this.voiceAllocator.noteOn(noteData.note, noteData.frequency, noteData.velocity);
      this.lastPlayedNote = noteData;
      
      // Schedule note off based on gate
      const noteDuration = (60 / this.clock.bpm) * (16 / this.rate) * this.gate;
      
      setTimeout(() => {
        if (this.lastPlayedNote === noteData) {
          this.voiceAllocator.noteOff(noteData.note);
        }
      }, noteDuration * 1000);
    }
  }
  
  getNextNote() {
    if (this.heldNotes.length === 0) return null;
    
    // Build note sequence with octaves
    const sequence = [];
    for (let oct = 0; oct < this.octaves; oct++) {
      this.heldNotes.forEach(noteData => {
        sequence.push({
          note: noteData.note + (oct * 12),
          frequency: noteData.frequency * Math.pow(2, oct),
          velocity: noteData.velocity
        });
      });
    }
    
    // Get note based on pattern
    let noteData;
    
    switch (this.pattern) {
      case 'up':
        noteData = sequence[this.currentIndex];
        this.currentIndex = (this.currentIndex + 1) % sequence.length;
        break;
        
      case 'down':
        noteData = sequence[sequence.length - 1 - this.currentIndex];
        this.currentIndex = (this.currentIndex + 1) % sequence.length;
        break;
        
      case 'up-down':
        noteData = sequence[this.currentIndex];
        
        this.currentIndex += this.direction;
        
        if (this.currentIndex >= sequence.length - 1) {
          this.direction = -1;
        } else if (this.currentIndex <= 0) {
          this.direction = 1;
        }
        break;
        
      case 'random':
        const randomIndex = Math.floor(Math.random() * sequence.length);
        noteData = sequence[randomIndex];
        break;
        
      case 'played':
        // Play in order notes were pressed
        noteData = sequence[this.currentIndex % sequence.length];
        this.currentIndex = (this.currentIndex + 1) % sequence.length;
        break;
    }
    
    return noteData;
  }
  
  noteOn(note, frequency, velocity) {
    // Add to held notes
    this.heldNotes.push({ note, frequency, velocity });
    
    // If not latched and arp is off, just pass through
    if (!this.enabled && !this.latch) {
      this.voiceAllocator.noteOn(note, frequency, velocity);
    }
  }
  
  noteOff(note) {
    // Remove from held notes
    this.heldNotes = this.heldNotes.filter(n => n.note !== note);
    
    // If latch mode, don't clear notes
    if (this.latch) return;
    
    // If no more held notes, reset index
    if (this.heldNotes.length === 0) {
      this.currentIndex = 0;
      this.direction = 1;
    }
    
    // If arp is off, pass through
    if (!this.enabled) {
      this.voiceAllocator.noteOff(note);
    }
  }
  
  clearLatch() {
    this.heldNotes = [];
    this.currentIndex = 0;
    this.direction = 1;
  }
  
  setPattern(pattern) {
    this.pattern = pattern;
    this.currentIndex = 0;
    this.direction = 1;
  }
  
  setOctaves(octaves) {
    this.octaves = Math.max(1, Math.min(4, octaves));
  }
  
  setRate(rate) {
    // rate: 4, 8, 16, 32 (quarter, eighth, sixteenth, thirty-second notes)
    this.rate = rate;
  }
  
  setGate(gate) {
    this.gate = Math.max(0.1, Math.min(1, gate));
  }
  
  setLatch(enabled) {
    this.latch = enabled;
    if (!enabled) {
      this.clearLatch();
    }
  }
}

// ============================================================================
// STEP SEQUENCER (16 steps)
// ============================================================================

class StepSequencer {
  constructor(voiceAllocator, clock) {
    this.voiceAllocator = voiceAllocator;
    this.clock = clock;
    
    this.steps = [];
    this.numSteps = 16;
    this.currentStep = 0;
    this.enabled = false;
    this.recording = false;
    
    // Initialize empty steps
    for (let i = 0; i < this.numSteps; i++) {
      this.steps.push({
        active: false,
        note: 60, // C4
        frequency: 261.63,
        velocity: 1.0,
        gate: 0.8
      });
    }
    
    this.lastPlayedNote = null;
    
    // Subscribe to clock
    this.clockCallback = (step) => this.onClockTick(step);
  }
  
  enable() {
    if (this.enabled) return;
    
    this.enabled = true;
    this.currentStep = 0;
    this.clock.subscribe(this.clockCallback);
  }
  
  disable() {
    if (!this.enabled) return;
    
    this.enabled = false;
    this.clock.unsubscribe(this.clockCallback);
    
    // Stop current note
    if (this.lastPlayedNote) {
      this.voiceAllocator.noteOff(this.lastPlayedNote.note);
      this.lastPlayedNote = null;
    }
  }
  
  toggle() {
    if (this.enabled) {
      this.disable();
    } else {
      this.enable();
    }
  }
  
  onClockTick(step) {
    if (!this.enabled) return;
    
    // Play on 16th notes
    if (step % 1 === 0) {
      this.playStep();
    }
  }
  
  playStep() {
    // Stop previous note
    if (this.lastPlayedNote) {
      this.voiceAllocator.noteOff(this.lastPlayedNote.note);
      this.lastPlayedNote = null;
    }
    
    // Get current step
    const stepData = this.steps[this.currentStep];
    
    // Play if active
    if (stepData.active) {
      this.voiceAllocator.noteOn(
        stepData.note,
        stepData.frequency,
        stepData.velocity
      );
      
      this.lastPlayedNote = stepData;
      
      // Schedule note off
      const noteDuration = (60 / this.clock.bpm) * stepData.gate;
      
      setTimeout(() => {
        if (this.lastPlayedNote === stepData) {
          this.voiceAllocator.noteOff(stepData.note);
          this.lastPlayedNote = null;
        }
      }, noteDuration * 1000);
    }
    
    // Advance step
    this.currentStep = (this.currentStep + 1) % this.numSteps;
  }
  
  // Set step data
  setStep(index, active, note = null, frequency = null, velocity = null, gate = null) {
    if (index < 0 || index >= this.numSteps) return;
    
    const step = this.steps[index];
    step.active = active;
    
    if (note !== null) step.note = note;
    if (frequency !== null) step.frequency = frequency;
    if (velocity !== null) step.velocity = velocity;
    if (gate !== null) step.gate = gate;
  }
  
  // Toggle step on/off
  toggleStep(index) {
    if (index < 0 || index >= this.numSteps) return;
    
    this.steps[index].active = !this.steps[index].active;
  }
  
  // Clear all steps
  clear() {
    this.steps.forEach(step => {
      step.active = false;
    });
  }
  
  // Randomize pattern
  randomize() {
    this.steps.forEach(step => {
      step.active = Math.random() > 0.5;
      step.velocity = 0.5 + Math.random() * 0.5;
    });
  }
  
  // Start recording (capture played notes to steps)
  startRecording() {
    this.recording = true;
    this.currentStep = 0;
  }
  
  stopRecording() {
    this.recording = false;
  }
  
  // Record note to current step
  recordNote(note, frequency, velocity) {
    if (!this.recording) return;
    
    this.setStep(this.currentStep, true, note, frequency, velocity);
  }
  
  // Get step for UI display
  getStep(index) {
    return this.steps[index];
  }
}

// ============================================================================
// PERFORMANCE RECORDER
// ============================================================================

class PerformanceRecorder {
  constructor() {
    this.events = [];
    this.recording = false;
    this.startTime = 0;
    this.playbackIntervalId = null;
  }
  
  start() {
    this.recording = true;
    this.startTime = Date.now();
    this.events = [];
  }
  
  stop() {
    this.recording = false;
  }
  
  // Record parameter change
  recordParameter(paramName, value) {
    if (!this.recording) return;
    
    this.events.push({
      type: 'param',
      time: Date.now() - this.startTime,
      param: paramName,
      value
    });
  }
  
  // Record note
  recordNote(note, frequency, velocity, on) {
    if (!this.recording) return;
    
    this.events.push({
      type: 'note',
      time: Date.now() - this.startTime,
      note,
      frequency,
      velocity,
      on
    });
  }
  
  // Playback recorded performance
  playback(synth) {
    if (this.events.length === 0) return;
    
    this.stopPlayback();
    
    let eventIndex = 0;
    const startTime = Date.now();
    
    this.playbackIntervalId = setInterval(() => {
      const elapsed = Date.now() - startTime;
      
      while (eventIndex < this.events.length && this.events[eventIndex].time <= elapsed) {
        const event = this.events[eventIndex];
        
        if (event.type === 'param') {
          // Apply parameter change
          if (typeof synth[`set${event.param}`] === 'function') {
            synth[`set${event.param}`](event.value);
          }
        } else if (event.type === 'note') {
          // Play note
          if (event.on) {
            synth.noteOn(event.frequency, event.velocity);
          } else {
            synth.noteOff(event.frequency);
          }
        }
        
        eventIndex++;
      }
      
      // Stop when done
      if (eventIndex >= this.events.length) {
        this.stopPlayback();
      }
    }, 10); // Check every 10ms
  }
  
  stopPlayback() {
    if (this.playbackIntervalId) {
      clearInterval(this.playbackIntervalId);
      this.playbackIntervalId = null;
    }
  }
  
  clear() {
    this.events = [];
    this.stopPlayback();
  }
  
  // Export performance as JSON
  export() {
    return JSON.stringify({
      events: this.events,
      duration: this.events.length > 0 ? this.events[this.events.length - 1].time : 0
    }, null, 2);
  }
  
  // Import performance from JSON
  import(json) {
    try {
      const data = JSON.parse(json);
      this.events = data.events;
    } catch (error) {
      console.error('Failed to import performance:', error);
    }
  }
}

// Export classes
if (typeof window !== 'undefined') {
  window.Clock = Clock;
  window.LoopBasedDelay = LoopBasedDelay;
  window.Arpeggiator = Arpeggiator;
  window.StepSequencer = StepSequencer;
  window.PerformanceRecorder = PerformanceRecorder;
}
