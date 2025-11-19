# Professional Synthesizer Refactor Plan

## Overview
Transform synth_complete.c into a professional DAW-style synthesizer with sequencing, drums, online presets, and extended features.

## Parameter Routing Plan (UI ➜ Audio Thread)

To keep the audio thread lock-free, every UI control that mutates DSP state must send values through the parameter queue. The first implementation covered master volume and tempo; the next wave will route the following groups:

| Group            | UI Controls                                        | Param IDs |
|------------------|----------------------------------------------------|-----------|
| FX – Distortion  | Enabled toggle, Drive, Mix                         | `PARAM_FX_DISTORTION_ENABLED`, `PARAM_FX_DISTORTION_DRIVE`, `PARAM_FX_DISTORTION_MIX` |
| FX – Chorus      | Enabled toggle, Rate, Depth, Mix                   | `PARAM_FX_CHORUS_ENABLED`, `PARAM_FX_CHORUS_RATE`, `PARAM_FX_CHORUS_DEPTH`, `PARAM_FX_CHORUS_MIX` |
| FX – Compressor  | Enabled toggle, Threshold, Ratio                   | `PARAM_FX_COMP_ENABLED`, `PARAM_FX_COMP_THRESHOLD`, `PARAM_FX_COMP_RATIO` |
| FX – Delay       | Time, Feedback, Mix                                | `PARAM_FX_DELAY_TIME`, `PARAM_FX_DELAY_FEEDBACK`, `PARAM_FX_DELAY_MIX` |
| FX – Reverb      | Size, Damping, Mix                                 | `PARAM_FX_REVERB_SIZE`, `PARAM_FX_REVERB_DAMPING`, `PARAM_FX_REVERB_MIX` |
| Synth Globals    | Master Volume (done), future filter/env controls   | `PARAM_MASTER_VOLUME`, `PARAM_FILTER_*`, `PARAM_ENV_*` |
| Transport        | Tempo (done), Arp enable/rate/mode                 | `PARAM_TEMPO`, `PARAM_ARP_ENABLED`, `PARAM_ARP_RATE`, `PARAM_ARP_MODE` |

Implementation steps:
1. Expand `ParamType` enum with the IDs above.
2. Update `param_queue_process_all()` to fan out to `g_app.fx`, `g_app.arp`, etc.
3. In `draw_gui()`, compare old/new slider/toggle values and push queue events when they change.
4. Add optional debug logging (guarded by `#define PARAM_DEBUG`) to trace queue traffic during testing.

## Architecture Changes

### 1. Remove
- ❌ QWERTY keyboard mapping (`g_keymap[]`, `KEYMAP_SIZE`)
- ❌ Key press tracking (`keys_pressed[]`)
- ❌ `key_callback()` function
- ❌ Keyboard polling in main loop
- ❌ `play_note()` / `stop_note()` helper functions
- ❌ Visual piano keyboard display
- ❌ Mouse click piano tracking

### 2. Add - Step Sequencer
```c
typedef struct {
    int note;           // MIDI note (-1 = none)
    float velocity;     // 0.0-1.0
    int length;         // Gate length in steps
    bool active;
} SequencerStep;

typedef struct {
    char name[32];
    SequencerStep steps[16];
    int length;         // 1-16 steps
    float swing;        // 0.0-1.0
} Pattern;

typedef struct {
    Pattern patterns[16];
    int current_pattern;
    int current_step;
    bool playing;
    bool loop_enabled;
    int loop_start, loop_end;
    double next_step_time;
} Sequencer;
```

### 3. Add - Drum Machine
```c
typedef enum {
    DRUM_KICK, DRUM_SNARE, DRUM_CLOSED_HH, 
    DRUM_OPEN_HH, DRUM_CLAP, DRUM_TOM_HI, 
    DRUM_TOM_MID, DRUM_TOM_LOW
} DrumType;

typedef struct {
    DrumType type;
    float pitch;        // Pitch adjust
    float decay;        // Decay time
    float tone;         // Tone control
    float volume;       // 0.0-1.0
} DrumVoice;

typedef struct {
    bool steps[8][16];      // 8 drums x 16 steps
    float velocity[8][16];  // Per-step velocity
    bool accent[8][16];     // Accent flag
} DrumPattern;

typedef struct {
    DrumVoice voices[8];
    DrumPattern patterns[16];
    int current_pattern;
    int current_step;
    bool playing;
    double next_step_time;
} DrumMachine;
```

### 4. Add - Preset Management
```c
typedef struct {
    char name[64];
    char author[64];
    char category[32];  // "Bass", "Lead", "Pad", "Keys", "FX"
    char tags[128];     // Comma-separated
    char url[256];      // Online preset URL
    uint8_t data[4096]; // Serialized parameters
    size_t data_size;
} Preset;

typedef struct {
    Preset presets[256];
    int num_presets;
    int current_preset;
    char search_filter[64];
    char category_filter[32];
} PresetManager;
```

### 5. Add - Extended Synth Features

#### Wavetable Oscillator
```c
#define WAVETABLE_SIZE 2048
typedef struct {
    float tables[64][WAVETABLE_SIZE];  // 64 wavetables
    int num_tables;
    float position;     // 0.0-63.0 (interpolated)
} Wavetable;
```

#### Additional Filters
```c
typedef enum {
    FILTER_LP_12, FILTER_LP_24,
    FILTER_HP_12, FILTER_HP_24,
    FILTER_BP, FILTER_NOTCH,
    FILTER_COMB, FILTER_FORMANT,
    FILTER_LADDER  // Moog-style
} FilterType;
```

#### Voice Modes
```c
typedef enum {
    VOICE_POLY,     // Polyphonic
    VOICE_MONO,     // Monophonic with retriggering
    VOICE_LEGATO    // Mono without retriggering
} VoiceMode;
```

#### Portamento
```c
typedef struct {
    bool enabled;
    float time;         // Glide time in seconds
    float current_pitch;
    float target_pitch;
} Portamento;
```

### 6. Add - Extended Effects

#### Chorus
```c
typedef struct {
    float delay_buffer[88200];  // 2 seconds @ 44.1kHz
    int write_pos;
    float rate;         // LFO rate
    float depth;        // Modulation depth
    float feedback;
    float mix;
} Chorus;
```

#### Flanger
```c
typedef struct {
    float delay_buffer[4410];   // 100ms max
    int write_pos;
    float rate;
    float depth;
    float feedback;
    float mix;
} Flanger;
```

#### Compressor
```c
typedef struct {
    float threshold;    // dB
    float ratio;        // 1:1 to 20:1
    float attack;       // seconds
    float release;      // seconds
    float makeup_gain;  // dB
    float envelope;     // Current envelope value
} Compressor;
```

#### Parametric EQ
```c
typedef struct {
    float low_freq, low_gain;
    float mid_freq, mid_gain, mid_q;
    float high_freq, high_gain;
} ParametricEQ;
```

### 7. Add - Online Preset System
```c
// Uses libcurl for HTTP downloads
typedef struct {
    char repo_url[256];     // GitHub raw content URL
    char cache_dir[256];    // Local cache directory
    bool auto_update;
    int last_update_time;
} PresetLibrary;

// Functions:
int preset_download_from_url(const char* url, Preset* preset);
int preset_library_sync(PresetLibrary* lib);
int preset_save_to_file(const Preset* preset, const char* path);
int preset_load_from_file(Preset* preset, const char* path);
```

### 8. Add - MIDI Support
```c
typedef struct {
    int device_id;
    char device_name[128];
    bool connected;
} MIDIDevice;

typedef struct {
    MIDIDevice inputs[16];
    MIDIDevice outputs[16];
    int num_inputs, num_outputs;
    int selected_input, selected_output;
    
    // MIDI learn
    bool learn_mode;
    int learn_parameter;
    
    // MPE support
    bool mpe_enabled;
    int mpe_channels;
} MIDIManager;
```

## GUI Layout Changes

### New Tab Structure
1. **SYNTH** - Oscillators, Filters, Envelopes, LFOs, Modulation
2. **FX** - All effects with enable/bypass, preset selection
3. **SEQUENCER** - 16-step grid editor, pattern management, loop controls
4. **DRUMS** - 8-track drum sequencer, drum voice editing
5. **PRESETS** - Browser with search, categories, online library
6. **MIDI** - MIDI I/O configuration, MIDI learn

### Sequencer UI
```
┌────────────────────────────────────────────┐
│ [▶] Play  [■] Stop  [🔁] Loop  BPM: 120   │
├────────────────────────────────────────────┤
│ Pattern: [01 ▼]  Length: [16]  Swing: 50% │
├────────────────────────────────────────────┤
│ Loop: [Start: 0] [End: 15]                 │
├────────────────────────────────────────────┤
│ Step Grid:                                 │
│ [1][2][3][4][5][6][7][8]...                │
│ Click step to toggle, drag to set note     │
└────────────────────────────────────────────┘
```

### Drum Machine UI
```
┌────────────────────────────────────────────┐
│ [▶] Play  [■] Stop  BPM: 120              │
├────────────────────────────────────────────┤
│      [1][2][3][4][5][6][7][8][9][10]...   │
│ KICK [●][ ][ ][●][ ][ ][●][ ]...          │
│ SNRE [ ][ ][●][ ][ ][ ][●][ ]...          │
│ CLHH [●][●][●][●][●][●][●][●]...          │
│ ...                                        │
└────────────────────────────────────────────┘
```

### Preset Browser UI
```
┌────────────────────────────────────────────┐
│ Search: [________] Category: [All ▼]       │
├────────────────────────────────────────────┤
│ Name          Category    Author   Online  │
│ Deep Bass     Bass        Factory   ✓      │
│ Wobble Lead   Lead        User      -      │
│ ...                                        │
├────────────────────────────────────────────┤
│ [Download Online] [Refresh] [Save As...]   │
└────────────────────────────────────────────┘
```

## Implementation Order

### Phase 1: Clean up keyboard code (1 hour)
- Remove all keyboard mapping
- Remove key callbacks
- Remove piano display
- Test compilation

### Phase 2: Add sequencer (2-3 hours)
- Implement `Sequencer` struct and functions
- Add sequencer processing to audio callback
- Create sequencer UI tab
- Add pattern editing

### Phase 3: Add drum machine (2-3 hours)
- Implement drum synthesis (kick, snare, hats)
- Add `DrumMachine` struct
- Create drum sequencer UI
- Integrate drum audio with main mix

### Phase 4: Extend synth engine (3-4 hours)
- Add wavetable oscillator
- Add more filter types
- Add portamento
- Add voice modes
- Expand modulation matrix

### Phase 5: Add more effects (2-3 hours)
- Implement chorus, flanger, phaser
- Implement compressor
- Implement parametric EQ
- Implement bitcrusher

### Phase 6: Preset system (3-4 hours)
- Implement preset serialization
- Create preset browser UI
- Add save/load functionality
- Create 20+ factory presets

### Phase 7: Online preset library (2-3 hours)
- Integrate libcurl
- Implement download functionality
- Create preset JSON format
- Set up GitHub preset repository

### Phase 8: MIDI support (3-4 hours)
- Integrate CoreMIDI (macOS) / RtMidi
- Implement MIDI I/O
- Add MIDI learn
- Add MPE support

## Total Estimated Time: 18-27 hours

## Dependencies to Add
- libcurl (for online presets)
- CoreMIDI / RtMidi (for MIDI I/O)

## Compatibility
- macOS (current)
- Could port to Linux/Windows later
