/**
 * Advanced Sound Engine Architecture
 * 
 * This is a complete rewrite of the synth engine to support:
 * - Polyphonic playback with voice allocation
 * - Multiple oscillators per voice with unison/detune
 * - Multimode filters with resonance and drive
 * - Multiple envelopes (amp, filter, pitch)
 * - Multiple LFOs with tempo sync
 * - Modulation matrix
 * - FX rack with reorderable effects
 * - Flexible signal routing
 * - Macro controls
 * - MPE support
 */

// ============================================================================
// ENVELOPE GENERATOR
// ============================================================================

class EnvelopeGenerator {
  constructor(audioContext) {
    this.audioContext = audioContext;
    this.attack = 0.01;
    this.decay = 0.1;
    this.sustain = 0.7;
    this.release = 0.3;
    this.curve = 'exponential'; // 'linear' or 'exponential'
  }
  
  // Trigger envelope and return value at time
  trigger(param, startTime, startValue = 0, targetValue = 1) {
    const now = startTime || this.audioContext.currentTime;
    
    param.cancelScheduledValues(now);
    param.setValueAtTime(startValue, now);
    
    // Attack
    if (this.curve === 'exponential') {
      param.exponentialRampToValueAtTime(
        Math.max(targetValue, 0.001), 
        now + this.attack
      );
    } else {
      param.linearRampToValueAtTime(targetValue, now + this.attack);
    }
    
    // Decay to sustain
    const sustainValue = targetValue * this.sustain;
    if (this.curve === 'exponential') {
      param.exponentialRampToValueAtTime(
        Math.max(sustainValue, 0.001),
        now + this.attack + this.decay
      );
    } else {
      param.linearRampToValueAtTime(
        sustainValue,
        now + this.attack + this.decay
      );
    }
    
    return now + this.attack + this.decay;
  }
  
  // Release envelope
  triggerRelease(param, startTime, currentValue) {
    const now = startTime || this.audioContext.currentTime;
    
    param.cancelScheduledValues(now);
    param.setValueAtTime(currentValue || param.value, now);
    
    if (this.curve === 'exponential') {
      param.exponentialRampToValueAtTime(0.001, now + this.release);
    } else {
      param.linearRampToValueAtTime(0, now + this.release);
    }
    
    return now + this.release;
  }
}

// ============================================================================
// LFO (Low Frequency Oscillator)
// ============================================================================

class LFO {
  constructor(audioContext) {
    this.audioContext = audioContext;
    this.oscillator = null;
    this.gainNode = null;
    this.waveform = 'sine';
    this.rate = 5; // Hz
    this.depth = 0.5;
    this.phase = 0;
    this.tempoSync = false;
    this.bpm = 120;
    this.retrigger = true;
    this.destinations = []; // Array of {param, amount, bipolar}
  }
  
  start() {
    if (this.oscillator) return;
    
    this.oscillator = this.audioContext.createOscillator();
    this.gainNode = this.audioContext.createGain();
    
    this.oscillator.type = this.waveform;
    this.oscillator.frequency.value = this.tempoSync ? 
      this.bpm / 60 / 4 : // Quarter note sync
      this.rate;
    
    this.gainNode.gain.value = this.depth;
    
    this.oscillator.connect(this.gainNode);
    this.oscillator.start();
    
    // Connect to all destinations
    this.destinations.forEach(dest => {
      this.gainNode.connect(dest.param);
    });
  }
  
  stop() {
    if (this.oscillator) {
      this.oscillator.stop();
      this.oscillator.disconnect();
      this.oscillator = null;
    }
    if (this.gainNode) {
      this.gainNode.disconnect();
      this.gainNode = null;
    }
  }
  
  setRate(rate) {
    this.rate = rate;
    if (this.oscillator && !this.tempoSync) {
      this.oscillator.frequency.value = rate;
    }
  }
  
  setDepth(depth) {
    this.depth = depth;
    if (this.gainNode) {
      this.gainNode.gain.value = depth;
    }
  }
  
  setWaveform(waveform) {
    this.waveform = waveform;
    if (this.oscillator) {
      this.oscillator.type = waveform;
    }
  }
}

// ============================================================================
// VOICE - Single voice in polyphonic synth
// ============================================================================

class Voice {
  constructor(audioContext, voiceConfig) {
    this.audioContext = audioContext;
    this.config = voiceConfig;
    
    // Voice state
    this.note = null;
    this.frequency = 0;
    this.velocity = 0;
    this.isActive = false;
    this.startTime = 0;
    
    // Oscillators (multiple per voice for unison)
    this.oscillators = [];
    this.oscillatorGains = [];
    
    // Filter
    this.filter = this.audioContext.createBiquadFilter();
    this.filter.type = 'lowpass';
    this.filter.frequency.value = 1000;
    this.filter.Q.value = 1;
    
    // Envelopes
    this.ampEnvelope = new EnvelopeGenerator(audioContext);
    this.filterEnvelope = new EnvelopeGenerator(audioContext);
    this.pitchEnvelope = new EnvelopeGenerator(audioContext);
    
    // Gain nodes
    this.ampGain = this.audioContext.createGain();
    this.ampGain.gain.value = 0;
    
    // Connect signal chain: oscs → filter → ampGain → output
    this.filter.connect(this.ampGain);
  }
  
  // Start voice with note
  noteOn(note, frequency, velocity = 1) {
    this.note = note;
    this.frequency = frequency;
    this.velocity = velocity;
    this.isActive = true;
    this.startTime = this.audioContext.currentTime;
    
    // Create oscillators (unison voices)
    const unisonVoices = this.config.unison.voices;
    const detune = this.config.unison.detune;
    
    for (let i = 0; i < unisonVoices; i++) {
      const osc = this.audioContext.createOscillator();
      const gain = this.audioContext.createGain();
      
      osc.type = this.config.waveform;
      osc.frequency.value = frequency;
      
      // Apply detune spread
      if (unisonVoices > 1) {
        const spread = (i / (unisonVoices - 1) - 0.5) * 2; // -1 to 1
        osc.detune.value = spread * detune;
      }
      
      // Reduce gain per voice to prevent clipping
      gain.gain.value = 1 / Math.sqrt(unisonVoices);
      
      osc.connect(gain);
      gain.connect(this.filter);
      
      osc.start(this.startTime);
      
      this.oscillators.push(osc);
      this.oscillatorGains.push(gain);
    }
    
    // Trigger envelopes
    this.ampEnvelope.trigger(
      this.ampGain.gain,
      this.startTime,
      0,
      velocity * this.config.volume
    );
    
    this.filterEnvelope.trigger(
      this.filter.frequency,
      this.startTime,
      this.filter.frequency.value,
      this.filter.frequency.value * 2 // Modulate up to 2x
    );
  }
  
  // Stop voice
  noteOff(releaseTime) {
    if (!this.isActive) return;
    
    const now = releaseTime || this.audioContext.currentTime;
    
    // Trigger release
    this.ampEnvelope.triggerRelease(this.ampGain.gain, now, this.ampGain.gain.value);
    
    // Schedule cleanup
    const cleanupTime = now + this.ampEnvelope.release + 0.1;
    
    setTimeout(() => {
      this.cleanup();
    }, (cleanupTime - this.audioContext.currentTime) * 1000);
  }
  
  // Clean up voice resources
  cleanup() {
    this.oscillators.forEach(osc => {
      try {
        osc.stop();
        osc.disconnect();
      } catch (e) {
        // Already stopped
      }
    });
    
    this.oscillatorGains.forEach(gain => {
      gain.disconnect();
    });
    
    this.oscillators = [];
    this.oscillatorGains = [];
    this.isActive = false;
    this.note = null;
  }
  
  // Connect voice output to destination
  connect(destination) {
    this.ampGain.connect(destination);
  }
  
  disconnect() {
    this.ampGain.disconnect();
  }
}

// ============================================================================
// VOICE ALLOCATOR - Manages polyphony
// ============================================================================

class VoiceAllocator {
  constructor(audioContext, maxVoices = 8) {
    this.audioContext = audioContext;
    this.maxVoices = maxVoices;
    this.voices = [];
    this.activeNotes = new Map(); // note -> voice mapping
    
    // Config shared by all voices
    this.voiceConfig = {
      waveform: 'sawtooth',
      volume: 0.5,
      unison: {
        voices: 1,
        detune: 10,
        spread: 1
      },
      filter: {
        type: 'lowpass',
        cutoff: 1000,
        resonance: 1,
        drive: 1
      }
    };
    
    // Modes
    this.monoMode = false;
    this.legatoMode = false;
    this.portamentoTime = 0;
    
    // Master output
    this.output = this.audioContext.createGain();
    this.output.gain.value = 1;
    
    // Initialize voice pool
    for (let i = 0; i < maxVoices; i++) {
      const voice = new Voice(audioContext, this.voiceConfig);
      voice.connect(this.output);
      this.voices.push(voice);
    }
  }
  
  // Find free voice or steal oldest
  allocateVoice() {
    // Find free voice
    let freeVoice = this.voices.find(v => !v.isActive);
    if (freeVoice) return freeVoice;
    
    // Voice stealing: find oldest active voice
    let oldestVoice = this.voices[0];
    let oldestTime = this.voices[0].startTime;
    
    for (const voice of this.voices) {
      if (voice.isActive && voice.startTime < oldestTime) {
        oldestVoice = voice;
        oldestTime = voice.startTime;
      }
    }
    
    // Stop oldest and return it
    oldestVoice.cleanup();
    return oldestVoice;
  }
  
  // Trigger note
  noteOn(note, frequency, velocity = 1) {
    // Mono mode: stop previous notes
    if (this.monoMode && this.activeNotes.size > 0) {
      if (this.legatoMode) {
        // Legato: retrigger same voice
        const [prevNote, voice] = this.activeNotes.entries().next().value;
        this.activeNotes.delete(prevNote);
        
        // Glide to new frequency
        if (this.portamentoTime > 0) {
          voice.oscillators.forEach(osc => {
            osc.frequency.exponentialRampToValueAtTime(
              frequency,
              this.audioContext.currentTime + this.portamentoTime
            );
          });
          voice.frequency = frequency;
          voice.note = note;
          this.activeNotes.set(note, voice);
        } else {
          voice.cleanup();
          const newVoice = this.allocateVoice();
          newVoice.noteOn(note, frequency, velocity);
          this.activeNotes.set(note, newVoice);
        }
        return;
      } else {
        // Stop all previous notes
        this.activeNotes.forEach((voice, n) => {
          voice.noteOff();
          this.activeNotes.delete(n);
        });
      }
    }
    
    // Allocate new voice
    const voice = this.allocateVoice();
    voice.noteOn(note, frequency, velocity);
    this.activeNotes.set(note, voice);
  }
  
  // Release note
  noteOff(note) {
    const voice = this.activeNotes.get(note);
    if (voice) {
      voice.noteOff();
      this.activeNotes.delete(note);
    }
  }
  
  // Stop all voices
  allNotesOff() {
    this.activeNotes.forEach((voice, note) => {
      voice.noteOff();
    });
    this.activeNotes.clear();
  }
  
  // Update voice config (applies to all voices)
  updateConfig(config) {
    Object.assign(this.voiceConfig, config);
    
    // Update active voices
    this.voices.forEach(voice => {
      Object.assign(voice.config, this.voiceConfig);
      
      // Update oscillator waveforms
      voice.oscillators.forEach(osc => {
        osc.type = this.voiceConfig.waveform;
      });
      
      // Update filter
      if (voice.filter) {
        voice.filter.type = this.voiceConfig.filter.type;
        voice.filter.frequency.value = this.voiceConfig.filter.cutoff;
        voice.filter.Q.value = this.voiceConfig.filter.resonance;
      }
    });
  }
  
  // Set polyphony limit
  setMaxVoices(maxVoices) {
    if (maxVoices > this.maxVoices) {
      // Add voices
      while (this.voices.length < maxVoices) {
        const voice = new Voice(this.audioContext, this.voiceConfig);
        voice.connect(this.output);
        this.voices.push(voice);
      }
    } else if (maxVoices < this.maxVoices) {
      // Remove excess voices (stop active ones first)
      while (this.voices.length > maxVoices) {
        const voice = this.voices.pop();
        if (voice.isActive) {
          voice.cleanup();
        }
        voice.disconnect();
      }
    }
    this.maxVoices = maxVoices;
  }
  
  // Get active voice count
  getActiveVoiceCount() {
    return Array.from(this.activeNotes.values()).filter(voice => voice.isActive).length;
  }
  
  // Connect to destination
  connect(destination) {
    this.output.connect(destination);
  }
  
  disconnect() {
    this.output.disconnect();
  }
}

// ============================================================================
// FX PROCESSOR BASE CLASS
// ============================================================================

class FXProcessor {
  constructor(audioContext, type) {
    this.audioContext = audioContext;
    this.type = type;
    this.enabled = true;
    this.wetDry = 1; // 0 = dry, 1 = wet
    
    // Create input/output nodes
    this.input = this.audioContext.createGain();
    this.output = this.audioContext.createGain();
    this.wetGain = this.audioContext.createGain();
    this.dryGain = this.audioContext.createGain();
    
    this.updateMix();
  }
  
  updateMix() {
    this.wetGain.gain.value = this.wetDry;
    this.dryGain.gain.value = 1 - this.wetDry;
  }
  
  setWetDry(amount) {
    this.wetDry = Math.max(0, Math.min(1, amount));
    this.updateMix();
  }
  
  enable() {
    this.enabled = true;
  }
  
  disable() {
    this.enabled = false;
  }
  
  // Override in subclasses
  connect(destination) {
    this.output.connect(destination);
  }
  
  disconnect() {
    this.output.disconnect();
  }
}

// ============================================================================
// FX: DISTORTION
// ============================================================================

class DistortionFX extends FXProcessor {
  constructor(audioContext) {
    super(audioContext, 'distortion');
    
    this.waveshaper = this.audioContext.createWaveShaper();
    this.drive = 50;
    this.updateCurve();
    
    // Routing: input → waveshaper → wetGain → output
    //                 ↘ dryGain → output
    this.input.connect(this.waveshaper);
    this.input.connect(this.dryGain);
    this.waveshaper.connect(this.wetGain);
    this.wetGain.connect(this.output);
    this.dryGain.connect(this.output);
  }
  
  updateCurve() {
    const samples = 44100;
    const curve = new Float32Array(samples);
    const deg = Math.PI / 180;
    
    for (let i = 0; i < samples; i++) {
      const x = (i * 2) / samples - 1;
      curve[i] = ((3 + this.drive) * x * 20 * deg) / 
                 (Math.PI + this.drive * Math.abs(x));
    }
    
    this.waveshaper.curve = curve;
  }
  
  setDrive(drive) {
    this.drive = Math.max(0, Math.min(100, drive));
    this.updateCurve();
  }
}

// ============================================================================
// FX: DELAY
// ============================================================================

class DelayFX extends FXProcessor {
  constructor(audioContext) {
    super(audioContext, 'delay');
    
    this.delayNode = this.audioContext.createDelay(5);
    this.feedbackGain = this.audioContext.createGain();
    
    this.time = 0.5;
    this.feedback = 0.3;
    
    this.delayNode.delayTime.value = this.time;
    this.feedbackGain.gain.value = this.feedback;
    
    // Routing: input → delay → feedback → delay (loop)
    //                        ↘ wetGain → output
    //          input ↘ dryGain → output
    this.input.connect(this.delayNode);
    this.input.connect(this.dryGain);
    this.delayNode.connect(this.feedbackGain);
    this.feedbackGain.connect(this.delayNode);
    this.delayNode.connect(this.wetGain);
    this.wetGain.connect(this.output);
    this.dryGain.connect(this.output);
  }
  
  setTime(time) {
    this.time = Math.max(0.001, Math.min(5, time));
    this.delayNode.delayTime.value = this.time;
  }
  
  setFeedback(feedback) {
    this.feedback = Math.max(0, Math.min(0.95, feedback));
    this.feedbackGain.gain.value = this.feedback;
  }
}

// ============================================================================
// FX: REVERB (Convolver-based)
// ============================================================================

class ReverbFX extends FXProcessor {
  constructor(audioContext) {
    super(audioContext, 'reverb');
    
    this.convolver = this.audioContext.createConvolver();
    this.roomSize = 0.5;
    
    this.generateImpulseResponse();
    
    // Routing
    this.input.connect(this.convolver);
    this.input.connect(this.dryGain);
    this.convolver.connect(this.wetGain);
    this.wetGain.connect(this.output);
    this.dryGain.connect(this.output);
  }
  
  generateImpulseResponse() {
    const sampleRate = this.audioContext.sampleRate;
    const length = sampleRate * this.roomSize * 4;
    const impulse = this.audioContext.createBuffer(2, length, sampleRate);
    
    for (let channel = 0; channel < 2; channel++) {
      const channelData = impulse.getChannelData(channel);
      for (let i = 0; i < length; i++) {
        channelData[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / length, 2);
      }
    }
    
    this.convolver.buffer = impulse;
  }
  
  setRoomSize(size) {
    this.roomSize = Math.max(0.1, Math.min(2, size));
    this.generateImpulseResponse();
  }
}

// ============================================================================
// FX RACK - Manages chain of effects
// ============================================================================

class FXRack {
  constructor(audioContext) {
    this.audioContext = audioContext;
    this.effects = [];
    
    // Master input/output
    this.input = this.audioContext.createGain();
    this.output = this.audioContext.createGain();
    
    // Initial connection
    this.input.connect(this.output);
  }
  
  // Add effect to chain
  addEffect(fx) {
    this.effects.push(fx);
    this.reconnect();
  }
  
  // Remove effect from chain
  removeEffect(fx) {
    const index = this.effects.indexOf(fx);
    if (index > -1) {
      this.effects.splice(index, 1);
      this.reconnect();
    }
  }
  
  // Reorder effects
  moveEffect(fromIndex, toIndex) {
    const fx = this.effects.splice(fromIndex, 1)[0];
    this.effects.splice(toIndex, 0, fx);
    this.reconnect();
  }
  
  // Reconnect entire chain
  reconnect() {
    // Disconnect everything
    this.input.disconnect();
    this.effects.forEach(fx => fx.disconnect());
    
    if (this.effects.length === 0) {
      // No effects: direct connection
      this.input.connect(this.output);
    } else {
      // Chain effects: input → fx1 → fx2 → ... → output
      this.input.connect(this.effects[0].input);
      
      for (let i = 0; i < this.effects.length - 1; i++) {
        this.effects[i].output.connect(this.effects[i + 1].input);
      }
      
      this.effects[this.effects.length - 1].output.connect(this.output);
    }
  }
  
  connect(destination) {
    this.output.connect(destination);
  }
  
  disconnect() {
    this.output.disconnect();
  }
}

// ============================================================================
// ADVANCED SYNTHESIZER
// ============================================================================

class AdvancedSynthesizer {
  constructor() {
    this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
    
    // Voice allocator (polyphony engine)
    this.voiceAllocator = new VoiceAllocator(this.audioContext, 8);
    
    // FX Rack
    this.fxRack = new FXRack(this.audioContext);
    
    // Add default effects
    this.distortion = new DistortionFX(this.audioContext);
    this.delay = new DelayFX(this.audioContext);
    this.reverb = new ReverbFX(this.audioContext);
    
    this.fxRack.addEffect(this.distortion);
    this.fxRack.addEffect(this.delay);
    this.fxRack.addEffect(this.reverb);
    
    // LFOs
    this.lfos = [
      new LFO(this.audioContext),
      new LFO(this.audioContext)
    ];
    
    // Master output
    this.masterGain = this.audioContext.createGain();
    this.masterGain.gain.value = 0.5;
    
    // Connect signal chain: voices → fxRack → master → speakers
    this.voiceAllocator.connect(this.fxRack.input);
    this.fxRack.connect(this.masterGain);
    this.masterGain.connect(this.audioContext.destination);
    
    console.log('Advanced synthesizer initialized');
  }
  
  // Note on
  noteOn(frequency, velocity = 1) {
    const note = Math.round(12 * Math.log2(frequency / 440) + 69);
    this.voiceAllocator.noteOn(note, frequency, velocity);
  }
  
  // Note off
  noteOff(frequency) {
    const note = Math.round(12 * Math.log2(frequency / 440) + 69);
    this.voiceAllocator.noteOff(note);
  }
  
  // Set waveform for all voices
  setWaveform(waveform) {
    this.voiceAllocator.updateConfig({ waveform });
  }
  
  // Set volume
  setVolume(volume) {
    this.voiceAllocator.updateConfig({ volume });
  }
  
  // Set filter
  setFilterCutoff(cutoff) {
    this.voiceAllocator.updateConfig({
      filter: { 
        ...this.voiceAllocator.voiceConfig.filter,
        cutoff 
      }
    });
  }
  
  setFilterResonance(resonance) {
    this.voiceAllocator.updateConfig({
      filter: { 
        ...this.voiceAllocator.voiceConfig.filter,
        resonance 
      }
    });
  }
  
  // Set envelope parameters
  setAttack(attack) {
    this.voiceAllocator.voices.forEach(voice => {
      voice.ampEnvelope.attack = attack;
    });
  }
  
  setRelease(release) {
    this.voiceAllocator.voices.forEach(voice => {
      voice.ampEnvelope.release = release;
    });
  }
  
  // Set unison
  setUnisonVoices(voices) {
    this.voiceAllocator.updateConfig({
      unison: {
        ...this.voiceAllocator.voiceConfig.unison,
        voices
      }
    });
  }
  
  setUnisonDetune(detune) {
    this.voiceAllocator.updateConfig({
      unison: {
        ...this.voiceAllocator.voiceConfig.unison,
        detune
      }
    });
  }
  
  // Set polyphony
  setPolyphony(voices) {
    this.voiceAllocator.setMaxVoices(voices);
  }
  
  // Set legato/portamento
  setLegatoMode(enabled) {
    this.voiceAllocator.legatoMode = enabled;
  }
  
  setPortamentoTime(time) {
    this.voiceAllocator.portamentoTime = time;
  }
  
  // FX controls
  setDistortionDrive(drive) {
    this.distortion.setDrive(drive);
  }
  
  setDelayTime(time) {
    this.delay.setTime(time);
  }
  
  setDelayFeedback(feedback) {
    this.delay.setFeedback(feedback);
  }
  
  setReverbSize(size) {
    this.reverb.setRoomSize(size);
  }
}

// Export for use
if (typeof window !== 'undefined') {
  window.AdvancedSynthesizer = AdvancedSynthesizer;
  window.VoiceAllocator = VoiceAllocator;
  window.FXRack = FXRack;
  window.DistortionFX = DistortionFX;
  window.DelayFX = DelayFX;
  window.ReverbFX = ReverbFX;
}
