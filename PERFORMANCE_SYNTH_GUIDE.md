# Hardware-Style Performance Synthesizer

## 🎛️ Overview

We're building a **performance-focused, hardware-style synthesizer** inspired by instruments like **Korg Volca**, **Arturia MicroBrute**, and **Moog Mother-32**. This is fundamentally different from a DAW (like FL Studio) - it's designed for **live playing, jamming, and real-time performance** rather than composition and arrangement.

---

## 🎯 Philosophy: Hardware vs DAW

### Hardware Synth Workflow (What We're Building)
```
┌─────────────┐
│  Play Keys  │
└──────┬──────┘
       ↓
┌─────────────────────┐
│  Sound Generated    │ ← Real-time synthesis
│  in Real-Time       │
└──────┬──────────────┘
       ↓
┌─────────────────────┐
│  FX Loop Running    │ ← Delays feed back continuously
│  (Delay, Reverb)    │   Parameters change while running
└──────┬──────────────┘
       ↓
┌─────────────────────┐
│  Arpeggiator /      │ ← Creates patterns from held notes
│  Step Sequencer     │   Synced to tempo clock
└──────┬──────────────┘
       ↓
┌─────────────────────┐
│  Output to Speakers │
└─────────────────────┘

Knobs control parameters INSTANTLY
No timeline, no arrangement, no tracks
Sound is ephemeral - exists only when playing
```

### DAW Workflow (FL Studio, Ableton)
```
┌──────────────┐
│  Piano Roll  │ ← Draw/arrange notes
└──────┬───────┘
       ↓
┌──────────────────┐
│  Pattern Clips   │ ← Organize into patterns
└──────┬───────────┘
       ↓
┌──────────────────┐
│  Timeline/       │ ← Arrange on timeline
│  Arrangement     │
└──────┬───────────┘
       ↓
┌──────────────────┐
│  Multi-Track Mix │ ← Mix multiple instruments
└──────┬───────────┘
       ↓
┌──────────────────┐
│  Render to File  │ ← Export finished song
└──────────────────┘

Timeline-based, composition-focused
Multiple tracks, automation lanes
Everything saved as project file
```

---

## 🔧 What We've Built (So Far)

### 1. Polyphonic Voice Engine

**Voice Class** - Individual synthesizer voice:
- Multiple oscillators per voice (unison/detune)
- Per-voice filter
- ADSR envelopes (amp, filter, pitch)
- Supports up to 32 voices

**VoiceAllocator** - Manages polyphony:
- Voice stealing (oldest voice when limit reached)
- Legato mode (retrigger same voice)
- Portamento/glide (smooth pitch transitions)
- Mono/poly mode switching

```javascript
// Example: 8-voice polyphony
const voiceAllocator = new VoiceAllocator(audioContext, 8);

// Play chord
voiceAllocator.noteOn(60, 261.63, 1.0); // C
voiceAllocator.noteOn(64, 329.63, 1.0); // E
voiceAllocator.noteOn(67, 392.00, 1.0); // G
```

### 2. FX Rack System

**FX Processors**:
- **Distortion**: Waveshaper with drive control
- **Delay**: Standard delay with feedback
- **Reverb**: Convolution reverb with room size

**FX Rack**:
- Reorderable effect chain
- Per-effect wet/dry mix
- Enable/disable individual effects

```javascript
const fxRack = new FXRack(audioContext);
fxRack.addEffect(new DistortionFX(audioContext));
fxRack.addEffect(new DelayFX(audioContext));
fxRack.moveEffect(0, 1); // Reorder effects
```

### 3. Loop-Based Delay (Hardware Style!) ⭐

This is the **key difference** from a DAW delay. Parameters can change **while delay is running**!

```javascript
const loopDelay = new LoopBasedDelay(audioContext);

// Play a note
synth.noteOn(440);

// Delay starts repeating
loopDelay.setTime(0.5);       // 500ms between repeats
loopDelay.setFeedback(0.6);   // 60% feedback

// WHILE IT'S REPEATING:
loopDelay.setTime(0.25);      // ← Repeats now come faster!
loopDelay.setFilterCutoff(500); // ← Each repeat gets darker
loopDelay.freeze();            // ← Hold current repeat forever
loopDelay.clear();             // ← Kill all repeats instantly
```

**Features**:
- **Real-time parameter changes** (no clicks/glitches)
- **Feedback loop** with filter coloring
- **Freeze function** (hold repeats infinitely)
- **Tempo sync** (lock to BPM)
- **Tap tempo** integration
- **Sync divisions** (whole, half, quarter, eighth, sixteenth notes)

**Example Workflow**:
1. Play a melody
2. Delay repeats it with 500ms timing
3. While repeating, twist time knob to 250ms
4. Repeats suddenly come twice as fast
5. Twist filter knob down
6. Each repeat gets progressively darker
7. Hit freeze button
8. Last repeat loops forever, building atmosphere
9. Play new notes over the frozen delay
10. Hit clear to kill frozen loop

This is **exactly like a guitar delay pedal** (Boss DD-3, EHX Memory Man).

### 4. Arpeggiator ⭐

Generates note patterns from held keys (like Korg, Roland).

```javascript
const arp = new Arpeggiator(voiceAllocator, clock);

arp.enable();
arp.setPattern('up-down');
arp.setOctaves(2);

// Hold C-E-G chord
arp.noteOn(60, 261.63, 1.0);
arp.noteOn(64, 329.63, 1.0);
arp.noteOn(67, 392.00, 1.0);

// Arp plays: C-E-G-C(oct)-E(oct)-G(oct)-G(oct)-E(oct)-C(oct)-G-E-C...
//            └─────── up ──────┘└────────── down ──────────┘
```

**Patterns**:
- **Up**: C → E → G → C(oct) → ...
- **Down**: G(oct) → E(oct) → C(oct) → G → ...
- **Up-Down**: C → E → G → C(oct) → G → E → C → ...
- **Random**: Random note from held notes
- **Played**: Order notes were pressed

**Features**:
- **Octave range** (1-4 octaves)
- **Rate** (4th, 8th, 16th, 32nd notes)
- **Gate** (how long each note is held)
- **Latch mode** (notes stay held when you release keys)
- **Tempo sync** (locked to master clock)

**Example Use**:
```
Hold down: C + E + G (C major chord)
Pattern: Up-Down
Octaves: 2
Rate: 16th notes
Gate: 80%

Result: Fast ascending/descending arpeggio 
        Perfect for trance leads!
```

### 5. Step Sequencer ⭐

16-step pattern sequencer (like TR-808, Volca).

```javascript
const seq = new StepSequencer(voiceAllocator, clock);

// Program pattern
seq.setStep(0, true, 60, 261.63, 1.0, 0.8);  // Step 1: C4
seq.setStep(4, true, 64, 329.63, 0.8, 0.6);  // Step 5: E4
seq.setStep(8, true, 67, 392.00, 1.0, 0.8);  // Step 9: G4
seq.setStep(12, true, 60, 261.63, 0.9, 0.5); // Step 13: C4

seq.enable(); // Start playing
```

**Features**:
- **16 steps** (1 bar of 16th notes)
- **Per-step parameters**:
  - Note/pitch
  - Velocity
  - Gate length
  - On/off
- **Live recording** mode (capture played notes)
- **Randomize** function
- **Clear** all steps
- **Tempo sync**

**Example Workflow**:
```
1. Enable sequencer
2. Tap step buttons to activate
3. Sequencer plays activated steps
4. Adjust per-step velocity/gate
5. Pattern loops continuously
6. Tweak synth knobs while it plays
7. Record performance to loop
```

### 6. Clock & Tempo System ⭐

Master tempo clock that syncs everything.

```javascript
const clock = new Clock(120); // 120 BPM

clock.start(); // Start ticking

// Everything subscribes to clock:
clock.subscribe((step, bpm) => {
  // step: 0-63 (16th notes across 4 bars)
  // bpm: current tempo
});

// Tap tempo
clock.tap(); // Tap button
clock.tap(); // Tap again
clock.tap(); // Tap again
// BPM automatically calculated from taps!
```

**Features**:
- **BPM control** (40-240 BPM)
- **Tap tempo** (tap 2-4 times to set tempo)
- **Clock sync** (all features lock to clock)
- **Transport controls** (start/stop)

**What Syncs to Clock**:
- Arpeggiator rate
- Step sequencer playback
- Delay time (when tempo-synced)
- LFOs (when tempo-synced)

### 7. Performance Recorder

Records knob movements and notes for playback.

```javascript
const recorder = new PerformanceRecorder();

recorder.start();

// Play and tweak knobs
synth.noteOn(440);
synth.setFilterCutoff(2000);
// ... more playing ...

recorder.stop();

// Playback
recorder.playback(synth); // Replays everything!
```

---

## 🎹 Complete Signal Flow

```
┌────────────────┐
│   Keyboard     │ ← You play keys
│   MIDI Input   │
└────────┬───────┘
         ↓
    ┌────────────────┐
    │  Arpeggiator   │ ← Optional: generates patterns
    │  (if enabled)  │
    └────────┬───────┘
             ↓
    ┌───────────────────┐
    │ Step Sequencer    │ ← Optional: plays programmed pattern
    │ (if enabled)      │
    └────────┬──────────┘
             ↓
┌─────────────────────────┐
│   Voice Allocator       │ ← Manages polyphony
│   (8 voices)            │
└────────┬────────────────┘
         ↓
┌─────────────────────────┐
│   Voice 1               │
│   ┌──────────┐          │
│   │Oscillator│ (unison) │
│   │Oscillator│          │
│   └─────┬────┘          │
│         ↓               │
│   ┌─────────┐           │
│   │ Filter  │           │
│   └────┬────┘           │
│        ↓                │
│   ┌─────────┐           │
│   │ Amp Env │           │
│   └─────────┘           │
└────────┬────────────────┘
         ↓
    (Voice 2-8 same structure)
         ↓
┌─────────────────────────┐
│   FX Rack               │
│   ┌──────────────┐      │
│   │  Distortion  │      │
│   └──────┬───────┘      │
│          ↓              │
│   ┌──────────────┐      │
│   │ Loop Delay   │ ←─┐  │ ← Feedback loop!
│   └──────┬───────┘   │  │
│          │           │  │
│          └───────────┘  │
│          ↓              │
│   ┌──────────────┐      │
│   │   Reverb     │      │
│   └──────────────┘      │
└────────┬────────────────┘
         ↓
┌─────────────────────────┐
│   Master Output         │
└─────────────────────────┘
         ↓
      Speakers

┌─────────────────────────┐
│   Master Clock          │ ← Syncs everything
│   (Tap Tempo)           │
└─────────────────────────┘
```

---

## 🎚️ Control Philosophy

### Hardware Synth: One Knob = One Function
```
Knob 1: Filter Cutoff  ← Turn knob, filter changes instantly
Knob 2: Resonance      ← Visual feedback on physical knob
Knob 3: Attack Time    ← Muscle memory develops
Knob 4: Release Time   ← Fast, intuitive workflow
```

### DAW: Click and Drag
```
Click filter cutoff → Drag slider → Release → Click next parameter
└─ Slower, less tactile, more precise
```

Our synth follows the **hardware philosophy**:
- Knobs respond instantly
- Visual feedback (knob rotation)
- Keyboard shortcuts for quick access
- No menus or sub-pages (everything visible)

---

## 🎵 Example Jam Session Workflow

### 1. **Set Tempo**
```
Tap tempo button in rhythm → BPM set to 128
```

### 2. **Program a Bass Line**
```
Enable step sequencer
Activate steps: 1, 5, 9, 13 (kick drum rhythm)
Set notes: C1, C1, E1, C1
Pattern loops
```

### 3. **Add Arpeggio Lead**
```
Enable arpeggiator
Pattern: Up-Down
Octaves: 2
Rate: 16th notes
Hold chord: C-E-G
Fast arpeggio plays over bass
```

### 4. **Add Loop Delay**
```
Play melody line
Delay time: Quarter note (synced to tempo)
Feedback: 70%
Filter cutoff: 1000Hz

While melody repeats:
- Turn feedback to 90% → repeats last longer
- Turn filter down to 300Hz → repeats get darker
- Hit freeze → last note loops forever
```

### 5. **Record Performance**
```
Start performance recorder
Twist filter cutoff knob up and down
Change delay time
Adjust resonance
Stop recorder

Play back → entire performance replays!
```

### 6. **Save as Preset**
```
All settings saved:
- Step sequencer pattern
- Arp settings
- Delay parameters
- Synth patch

Load preset later → instant recall of entire setup
```

---

## 🆚 Comparison Table

| Feature | Hardware Synth (Our Goal) | DAW (FL Studio) |
|---------|---------------------------|-----------------|
| **Workflow** | Play → Perform → Loop | Arrange → Compose → Export |
| **Timeline** | None | Multi-track timeline |
| **Focus** | Real-time performance | Composition & production |
| **Controls** | Knobs/sliders (tactile) | Mouse & keyboard |
| **Arpeggiator** | Generates from held keys | MIDI effect plugin |
| **Delay** | Loop-based, live tweaking | Insert effect with automation |
| **Step Sequencer** | Hardware-style 16 steps | Piano roll (unlimited notes) |
| **Saving** | Presets (patches) | Project files (entire songs) |
| **Best For** | Jamming, live sets, sound design | Full track production |

---

## 🚀 What's Next

### Already Built ✅
- Voice allocation & polyphony
- ADSR envelopes
- Unison/detune
- FX rack (distortion, delay, reverb)
- Loop-based delay with freeze
- Arpeggiator (5 patterns, latch mode)
- 16-step sequencer
- Master clock with tap tempo
- Performance recorder

### Next Steps (In Order)

#### 1. **Integrate with Existing UI** (2-3 hours)
- Replace current monophonic engine with VoiceAllocator
- Wire up existing knobs to new engine
- Test polyphony on keyboard

#### 2. **Build Performance UI** (3-4 hours)
- Arpeggiator controls (pattern, octaves, rate, gate, latch)
- Step sequencer grid (16 buttons, visual playhead)
- Delay controls (time, feedback, filter, freeze button, sync toggle)
- Transport section (play/stop, record, tap tempo, BPM display)
- Clock/sync indicators

#### 3. **Polish & Test** (1-2 hours)
- Visual feedback (active steps light up, arp/seq running indicators)
- Keyboard shortcuts (space: play/stop, R: record, T: tap tempo)
- Preset system updates (save arp/seq patterns)
- Performance mode toggle

#### 4. **Documentation** (1 hour)
- User guide for performance features
- Video tutorials (record jam sessions)
- Preset demos

---

## 💡 Unique Features (What Makes This Special)

### 1. **Web-Based Hardware Synth**
Most web synths copy DAWs. Ours is a **true hardware synth experience** in the browser.

### 2. **Loop Delay with Real-Time Control**
You can twist knobs **while delays are running**. This is powerful for live performance.

### 3. **Integrated Arp + Sequencer + Delay**
Everything synced to one master clock. Jam with all features together.

### 4. **Performance Recording**
Capture your jam, play it back, share it. No DAW needed.

### 5. **Retro Aesthetic**
Dark metal panels, rotary knobs, LED indicators. Looks like vintage hardware.

---

## 🎯 Target Use Cases

### 1. **Live Performance**
- Play synth live on stage
- Use arp + sequencer for backing patterns
- Twist knobs for real-time modulation
- Freeze delay for ambient builds

### 2. **Sound Design**
- Explore sounds with tactile controls
- Record interesting performances
- Save presets for later use

### 3. **Electronic Music Production**
- Generate ideas quickly
- Export audio to DAW for finishing
- Use as MIDI controller input to synth

### 4. **Education**
- Learn synthesis concepts
- Understand hardware synth workflow
- Practice performance techniques

### 5. **Fun/Jamming**
- Just play and have fun!
- No pressure to "finish a song"
- Instant gratification

---

## 📖 Technical Architecture

### Engine Hierarchy
```
AdvancedSynthesizer (top level)
├── VoiceAllocator (polyphony management)
│   ├── Voice 1
│   │   ├── Oscillators (unison)
│   │   ├── Filter
│   │   └── Envelopes (amp, filter, pitch)
│   ├── Voice 2-8 (same structure)
│   └── Voice Config (shared settings)
├── FXRack (effect chain)
│   ├── DistortionFX
│   ├── LoopBasedDelay ⭐
│   └── ReverbFX
├── Clock (master tempo)
├── Arpeggiator
├── StepSequencer
└── PerformanceRecorder
```

### Key Classes

**Voice** - Single synthesis voice with oscillators, filter, envelopes  
**VoiceAllocator** - Manages pool of voices, handles note on/off  
**LoopBasedDelay** - Hardware-style delay with feedback loop  
**Arpeggiator** - Pattern generator from held notes  
**StepSequencer** - 16-step programmable sequencer  
**Clock** - Master tempo with tap tempo  
**PerformanceRecorder** - Records and plays back performances

---

## 🎊 Summary

We're building something **unique**: a web-based synthesizer that works like **actual hardware** rather than trying to be a mini-DAW. The focus is on **real-time performance, immediate feedback, and tactile control**.

The **loop-based delay** is the killer feature - you can twist knobs **while delays are running**, just like a guitar pedal. Combined with the arpeggiator and step sequencer, this creates a powerful jamming and performance tool.

**Next step**: Integrate this engine with the existing UI and build the performance controls!

Ready to continue? 🎹
