# Professional Synthesizer - Full Implementation Plan

## Current Status
✅ **Core DSP Engine Complete** (`synth_engine.c` / `synth_engine.h`)
- 8-voice polyphony
- Multi-waveform oscillators (sine, saw, square, tri, noise)
- State-variable filters (LP/HP/BP/Notch)
- ADSR envelopes
- LFOs
- Modulation matrix
- Voice allocation & stealing
- Master limiter

## Phase 1: GUI Foundation (Next Step)
**Goal**: Playable synth with computer keyboard input and basic controls

### Technologies:
- **GLFW**: Window & input management
- **Nuklear** or **Dear ImGui**: Immediate-mode GUI
- **OpenGL**: Rendering

### Features:
1. **Computer Keyboard → MIDI**
   - QWERTY layout mapped to piano keys
   - Z-M = C3-B3 (lower octave)
   - Q-I = C4-C5 (upper octave)
   - Sustain pedal simulation

2. **Basic GUI Panels**:
   - Oscillator controls (waveform, detune, unison, PWM)
   - Filter controls (cutoff, resonance, mode)
   - Envelope controls (ADSR)
   - Master section (volume, tempo)
   - Visual keyboard display

3. **Performance Monitoring**:
   - CPU usage meter
   - Active voice count
   - Buffer underrun detection

## Phase 2: Effects Rack
### Effects to Implement:
1. **Distortion/Waveshaper**
   - Drive, tone, output gain
   - Multiple curves (soft, hard, asymmetric)

2. **Chorus**
   - LFO rate, depth, feedback
   - Stereo widening

3. **Delay**
   - Time (ms or tempo-synced)
   - Feedback, filter
   - Ping-pong mode

4. **Reverb**
   - Freeverb or plate algorithm
   - Size, damping, width

5. **Compressor**
   - Threshold, ratio, attack, release
   - Makeup gain

6. **EQ**
   - 3-band parametric
   - Low/mid/high gain + frequency

7. **Bitcrusher**
   - Bit depth reduction
   - Sample rate reduction

### FX Chain Features:
- Drag-drop reordering
- Bypass toggle per effect
- Dry/wet mix per effect
- Visual routing display

## Phase 3: Arpeggiator & Sequencing
### Arpeggiator:
- Modes: up, down, up-down, random, chord
- Rate (tempo-synced divisions: 1/4, 1/8, 1/16, 1/32)
- Gate length control
- Swing/shuffle
- Octave range (1-4 octaves)
- Latch mode

### Step Sequencer:
- 8-64 step grid
- Per-step: note, velocity, probability, tie
- Pattern chaining (A→B→C)
- Multiple patterns
- Copy/paste/clear functions

## Phase 4: Drum Synth
### Voices:
- Kick (sine + pitch envelope + noise)
- Snare (noise + body + snap)
- Hi-hat (noise + filter sweep)
- Clap (noise burst + reverb)
- Tom (pitched noise + resonance)

### Features:
- Independent envelopes per voice
- Tuning, decay, snap controls
- Multi-out routing
- Sample layering (optional)

## Phase 5: Sampling Engine
### Core Features:
- Load WAV/AIFF/FLAC samples
- Sample playback oscillator
- Loop points (forward, ping-pong)
- One-shot mode
- Root note assignment
- Key tracking

### Advanced:
- Time-stretch (phase vocoder)
- Pitch-shift (granular)
- Sample browser with waveform preview
- Drag-drop sample loading

## Phase 6: Modulation System UI
### Visual Mod Matrix:
- Source → Destination routing view
- Amount sliders per connection
- Color-coded connections
- Quick preset routes

### MIDI Learn:
- Click parameter + move MIDI controller
- Save MIDI mappings to preset
- Clear individual mappings

### Macro Controls:
- 8 macro knobs
- Assign multiple destinations per macro
- Per-destination amount scaling
- Macro automation

## Phase 7: MIDI I/O
### Input:
- Note on/off
- Pitch bend
- Mod wheel (CC1)
- Sustain pedal (CC64)
- Velocity sensitivity
- Aftertouch
- All CCs

### Output:
- Send notes to external synths
- MIDI thru
- CC output from automation

### MPE Support:
- Per-note pitch bend
- Per-note pressure
- Per-note timbre (CC74)

## Phase 8: Preset System
### Browser:
- List view with search/filter
- Tags: bass, lead, pad, pluck, fx, etc.
- Author, category metadata
- Star/favorite system
- Recently used

### Storage:
- JSON format
- Factory presets (read-only)
- User presets (~/Documents/Synth/Presets/)
- Autosave last state

### Features:
- Save/Save As
- Patch morphing (crossfade between presets)
- A/B comparison
- Random preset generator

## Phase 9: Project Management
### File Operations:
- New/Open/Save/Save As
- Auto-save (every 5 min)
- Crash recovery
- Project bundles (synth state + samples)

### Export:
- Render audio (WAV, MP3)
- Export MIDI
- Export stems (per-voice/per-FX)
- Batch export

## Phase 10: UI/UX Polish
### Visualization:
- Oscilloscope (time domain)
- Spectrum analyzer (frequency domain)
- Envelope curves (interactive)
- Filter response curve

### Theming:
- Light/dark modes
- Custom color schemes
- Adjustable UI scale

### Keyboard Shortcuts:
- Space = play/stop
- Cmd/Ctrl+S = save
- Cmd/Ctrl+O = open
- Cmd/Ctrl+Z = undo
- Cmd/Ctrl+Shift+Z = redo
- Cmd/Ctrl+P = preset browser

### Accessibility:
- Tooltips on hover
- Parameter value displays
- Keyboard navigation
- Screen reader support (future)

## Development Priority
1. **Immediate** (Week 1):
   - ✅ Core engine
   - → GUI framework setup
   - → Computer keyboard input
   - → Basic synth controls

2. **Short-term** (Weeks 2-3):
   - Effects rack (at least 3-4 effects)
   - Arpeggiator
   - Basic preset save/load

3. **Medium-term** (Month 2):
   - Step sequencer
   - MIDI input
   - Complete preset system
   - Mod matrix UI

4. **Long-term** (Month 3+):
   - Drum synth
   - Sampling engine
   - MPE support
   - Polish & optimization

## Technical Notes
### Performance Targets:
- <10ms latency
- <30% CPU at 8 voices
- 60 FPS GUI
- No audio dropouts

### Code Organization:
```
synth/
├── synth_engine.{c,h}     # Core DSP (done)
├── effects.{c,h}          # FX processing
├── arpeggiator.{c,h}      # Arp engine
├── sequencer.{c,h}        # Step sequencer
├── sampler.{c,h}          # Sample playback
├── midi.{c,h}             # MIDI I/O
├── preset.{c,h}           # Preset management
├── gui_main.c             # GUI application
├── gui_panels.c           # UI panels
└── gui_widgets.c          # Custom widgets
```

### Dependencies:
- ✅ miniaudio (audio I/O)
- ✅ GLFW (windowing)
- Nuklear or Dear ImGui (GUI)
- RtMidi (MIDI I/O)
- libsndfile (audio file loading)
- json-c (preset format)

## Next Command to Run:
```bash
# Compile GUI version
gcc synth_gui.c synth_engine.c -o synth_gui \
    -lm -lpthread \
    -framework CoreAudio -framework AudioToolbox \
    -framework OpenGL -framework Cocoa -framework IOKit \
    -L/opt/homebrew/lib -lglfw \
    -I/opt/homebrew/include \
    -O2

# Run it
./synth_gui
```

