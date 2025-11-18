# C Synthesizer Project

A minimal, cross-platform software synthesizer written in pure C.

## Project Goals

- Learn professional audio programming in C
- Build a real-time polyphonic synthesizer
- Understand DSP fundamentals
- Create a foundation for future audio projects

## Technology Stack

- **Audio I/O:** miniaudio (single-file, cross-platform)
- **DSP:** Soundpipe (modular synthesis library)
- **File I/O:** dr_wav (single-file WAV support)
- **Serialization:** cJSON (preset management)
- **Build:** CMake (cross-platform build system)

## Features (Planned)

### Phase 1: Basic Audio (Week 1)
- [x] Setup project structure
- [ ] Generate sine wave (440 Hz)
- [ ] Play audio through system
- [ ] Verify low-latency playback

### Phase 2: Synthesis (Week 2)
- [ ] Implement oscillator (sine, saw, square, triangle)
- [ ] ADSR envelope
- [ ] Basic filter (lowpass)
- [ ] Single voice synth

### Phase 3: Polyphony (Week 3)
- [ ] 8-voice polyphony
- [ ] Voice allocation/stealing
- [ ] MIDI note on/off
- [ ] Velocity sensitivity

### Phase 4: Advanced (Week 4+)
- [ ] Preset system (save/load)
- [ ] Unison/detune
- [ ] Effects (delay, chorus)
- [ ] GUI (optional: nuklear or ImGui)

## Directory Structure

```
synth-c/
├── README.md           # This file
├── CMakeLists.txt      # Build configuration
├── build/              # Build output (generated)
├── src/
│   ├── main.c          # Entry point
│   ├── synth.c         # Synthesizer engine
│   ├── synth.h         # Synthesizer header
│   ├── dsp.c           # DSP functions (oscillators, filters)
│   ├── dsp.h           # DSP header
│   └── voice.c         # Voice management
│       voice.h
├── lib/                # Third-party libraries
│   ├── miniaudio.h     # Audio I/O
│   ├── soundpipe.h     # DSP library
│   ├── dr_wav.h        # WAV file support
│   └── cJSON.h         # JSON parsing
└── presets/            # Preset files
    └── default.json
```

## Build Instructions

### Prerequisites

**macOS:**
```bash
brew install cmake
```

**Linux:**
```bash
sudo apt-get install cmake build-essential libasound2-dev
```

**Windows:**
- Install Visual Studio 2019+ with C++ tools
- Install CMake from cmake.org

### Building

```bash
cd synth-c
mkdir build
cd build
cmake ..
make
./synth
```

### Quick Start (Just Test)

```bash
# Compile and run minimal example
gcc src/main.c -o synth -lm -lpthread
./synth
```

## Learning Path

### Week 1: Audio Basics
**Goal:** Generate and play a 440 Hz sine wave

**What you'll learn:**
- Audio callback functions
- Sample rate and buffer sizes
- Float vs integer audio
- Real-time constraints

**Code:** `src/main.c` (phase 1)

---

### Week 2: DSP Fundamentals
**Goal:** Build oscillator + envelope + filter

**What you'll learn:**
- Waveform generation (sine, saw, square, triangle)
- ADSR envelopes
- Lowpass/highpass filters
- Frequency-to-phase conversion

**Code:** `src/dsp.c`

---

### Week 3: Polyphony
**Goal:** 8 simultaneous voices like your web synth

**What you'll learn:**
- Voice allocation
- Voice stealing algorithms
- MIDI note mapping
- Mixing multiple voices

**Code:** `src/voice.c`

---

### Week 4: Polish
**Goal:** Presets, effects, UI

**What you'll learn:**
- JSON serialization
- Delay/reverb effects
- Parameter smoothing
- Cross-thread communication

---

## Comparison to Web Audio Version

| Feature | Web Audio | C Version |
|---------|-----------|-----------|
| **Latency** | ~10-20ms | ~5ms |
| **CPU Usage** | Medium | Low |
| **Portability** | Browser only | Desktop app |
| **Distribution** | Just URL | Binary per OS |
| **Development Speed** | Fast | Slower |
| **Learning Curve** | Easy | Steep |
| **Professional Use** | Prototyping | Production |

## Real-Time Audio Rules (CRITICAL!)

These are **non-negotiable** in the audio callback:

❌ **NEVER in audio callback:**
- `malloc()` / `free()` - No dynamic allocation
- `printf()` / logging - No I/O operations
- `fopen()` / file access - No file operations
- Locks / mutexes - Can cause priority inversion
- System calls - Unpredictable timing

✅ **ALWAYS in audio callback:**
- Pre-allocated buffers
- Lock-free data structures
- Simple math operations
- Fixed-size arrays
- Pure computations

## Key Concepts

### Sample Rate
- 44.1 kHz = 44,100 samples per second
- Each sample = amplitude at that instant
- Higher rate = better quality, more CPU

### Buffer Size
- Number of samples processed at once
- Smaller = lower latency, more CPU strain
- Typical: 64, 128, 256, 512 samples
- 128 samples @ 44.1kHz = ~3ms latency

### Nyquist Frequency
- Maximum representable frequency = Sample Rate / 2
- 44.1 kHz → max 22.05 kHz (above human hearing)

### Phase Accumulation
```c
// Generate sine wave
float phase = 0.0f;
float frequency = 440.0f;
float sample_rate = 44100.0f;

for (int i = 0; i < num_samples; i++) {
    output[i] = sinf(phase * 2.0f * M_PI);
    phase += frequency / sample_rate;
    if (phase >= 1.0f) phase -= 1.0f;
}
```

### MIDI Note to Frequency
```c
// A4 = 440 Hz = MIDI note 69
float midi_to_freq(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}
```

## Resources

### Documentation
- [miniaudio GitHub](https://github.com/mackron/miniaudio)
- [Soundpipe Docs](https://paulbatchelor.github.io/proj/soundpipe.html)
- [Music DSP Archive](https://www.musicdsp.org/)

### Books
- "The Audio Programming Book" - Boulanger & Lazzarini
- "Designing Audio Effect Plugins in C++" - Will Pirkle
- "Real Sound Synthesis for Interactive Applications" - Perry Cook

### Tutorials
- [Handmade Hero](https://handmadehero.org/) - Casey Muratori
- [Audio Programming Tutorial](https://www.youtube.com/c/TheAudioProgrammer)

## License

MIT License - Feel free to use for any purpose

## Next Steps

1. **Read this entire README** to understand the architecture
2. **Build and run** the minimal example
3. **Modify `main.c`** to change frequency/waveform
4. **Complete Week 1 goals** before moving on
5. **Compare to your web synth** to understand differences

Let's build something awesome! 🎹
