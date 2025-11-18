/**
 * Professional Synthesizer Engine
 * 
 * Core audio DSP engine with:
 * - Multi-waveform oscillators (sine, saw, square, tri, noise, wavetable)
 * - State-variable filters (LP/HP/BP/Notch)
 * - ADSR envelopes (amp, filter, pitch)
 * - LFOs with tempo sync
 * - Modulation matrix
 * - Polyphonic voice engine
 */

#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define MAX_VOICES 8
#define MAX_UNISON 5
#define MAX_LFO 4
#define MAX_MOD_SLOTS 16
#define WAVETABLE_SIZE 2048

// ============================================================================
// ENUMS
// ============================================================================

typedef enum {
    WAVE_SINE,
    WAVE_SAW,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_NOISE,
    WAVE_WAVETABLE,
    WAVE_COUNT
} WaveformType;

typedef enum {
    FILTER_LP,      // Low-pass
    FILTER_HP,      // High-pass
    FILTER_BP,      // Band-pass
    FILTER_NOTCH,   // Notch (band-reject)
    FILTER_ALLPASS,
    FILTER_COUNT
} FilterMode;

typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_OFF
} EnvelopeState;

typedef enum {
    MOD_SOURCE_NONE,
    MOD_SOURCE_LFO1,
    MOD_SOURCE_LFO2,
    MOD_SOURCE_LFO3,
    MOD_SOURCE_LFO4,
    MOD_SOURCE_ENV_AMP,
    MOD_SOURCE_ENV_FILTER,
    MOD_SOURCE_ENV_PITCH,
    MOD_SOURCE_VELOCITY,
    MOD_SOURCE_MODWHEEL,
    MOD_SOURCE_AFTERTOUCH,
    MOD_SOURCE_KEYTRACK,
    MOD_SOURCE_RANDOM,
    MOD_SOURCE_COUNT
} ModSource;

typedef enum {
    MOD_DEST_NONE,
    MOD_DEST_OSC1_PITCH,
    MOD_DEST_OSC1_PWM,
    MOD_DEST_OSC2_PITCH,
    MOD_DEST_OSC2_PWM,
    MOD_DEST_FILTER_CUTOFF,
    MOD_DEST_FILTER_RESONANCE,
    MOD_DEST_AMP,
    MOD_DEST_PAN,
    MOD_DEST_LFO_RATE,
    MOD_DEST_COUNT
} ModDestination;

typedef enum {
    VOICE_OFF,
    VOICE_ATTACK,
    VOICE_HOLD,
    VOICE_RELEASE
} VoiceState;

// ============================================================================
// OSCILLATOR
// ============================================================================

typedef struct {
    // Core parameters
    WaveformType waveform;
    float phase;              // 0.0 to 1.0
    float frequency;          // Hz
    float amplitude;          // 0.0 to 1.0
    
    // PWM (pulse width modulation)
    float pulse_width;        // 0.0 to 1.0 (for square wave)
    
    // Unison/detune
    int unison_voices;        // 1-5
    float detune_cents;       // -100 to +100
    float unison_spread;      // 0.0 to 1.0
    
    // Sync
    bool hard_sync;
    float sync_phase;
    
    // FM/RM
    float fm_amount;          // Frequency modulation depth
    float rm_amount;          // Ring modulation depth
    
    // Phase
    float phase_offset;       // 0.0 to 1.0
    bool phase_reset;         // Reset phase on note trigger
    
    // Drift (analog feel)
    float drift_amount;       // 0.0 to 1.0
    float drift_rate;         // Hz
    float drift_phase;
    
    // Wavetable
    float* wavetable;
    int wavetable_size;
    float wavetable_position; // 0.0 to 1.0 through table
} Oscillator;

// ============================================================================
// FILTER
// ============================================================================

typedef struct {
    FilterMode mode;
    float cutoff;             // 20 Hz to 20000 Hz
    float resonance;          // 0.0 to 1.0 (Q factor)
    float keytrack;           // 0.0 to 1.0 (filter follows key)
    float env_amount;         // -1.0 to 1.0 (envelope modulation)
    
    // State variable filter state
    float low;
    float high;
    float band;
    float notch;
    
    // Coefficients (updated when cutoff/resonance change)
    float f;                  // Frequency coefficient
    float q;                  // Resonance coefficient
} Filter;

// ============================================================================
// ENVELOPE
// ============================================================================

typedef struct {
    // ADSR parameters (all in seconds)
    float attack;
    float decay;
    float sustain;            // 0.0 to 1.0 (level, not time)
    float release;
    
    // State
    EnvelopeState state;
    float current_level;      // 0.0 to 1.0
    float target_level;
    
    // Velocity sensitivity
    float velocity_sensitivity; // 0.0 to 1.0
    float velocity;            // 0.0 to 1.0
    
    // Retrigger/loop
    bool retrigger;
    bool loop;
} Envelope;

// ============================================================================
// LFO
// ============================================================================

typedef struct {
    WaveformType waveform;
    float rate;               // Hz or BPM fraction
    float phase;              // 0.0 to 1.0
    float amount;             // 0.0 to 1.0
    
    // Timing
    bool tempo_sync;
    float tempo_division;     // 1.0 = quarter note, 0.5 = eighth, etc.
    bool key_sync;            // Reset on note trigger
    
    // Fade in
    float delay_time;         // Seconds before LFO starts
    float fade_time;          // Seconds to fade in
    float fade_level;         // Current fade level (0.0 to 1.0)
    
    // Bipolar output
    bool bipolar;             // -1 to 1 vs 0 to 1
} LFO;

// ============================================================================
// MODULATION
// ============================================================================

typedef struct {
    ModSource source;
    ModDestination destination;
    float amount;             // -1.0 to 1.0 (bipolar)
    bool enabled;
} ModSlot;

typedef struct {
    ModSlot slots[MAX_MOD_SLOTS];
    int num_slots;
    
    // Cached source values (updated once per buffer)
    float source_values[MOD_SOURCE_COUNT];
} ModulationMatrix;

// ============================================================================
// VOICE
// ============================================================================

typedef struct {
    // State
    VoiceState state;
    int midi_note;
    float frequency;          // Hz
    float velocity;           // 0.0 to 1.0
    
    // Components
    Oscillator osc1;
    Oscillator osc2;
    Filter filter;
    Envelope env_amp;
    Envelope env_filter;
    Envelope env_pitch;
    
    // Voice parameters
    float pan;                // -1.0 (left) to 1.0 (right)
    float pitch_bend;         // -1.0 to 1.0 (semitones based on bend range)
    
    // Glide/portamento
    float glide_rate;         // Seconds to glide full octave
    float current_pitch;      // Current gliding pitch
    float target_pitch;       // Target pitch
    
    // Timing
    uint64_t note_on_time;
    uint64_t note_off_time;
    
    // Per-voice random
    float random_value;       // 0.0 to 1.0
} Voice;

// ============================================================================
// SYNTH ENGINE
// ============================================================================

typedef struct {
    // Audio settings
    float sample_rate;
    float tempo;              // BPM
    
    // Voices
    Voice voices[MAX_VOICES];
    int num_active_voices;
    
    // Global LFOs
    LFO lfos[MAX_LFO];
    
    // Modulation
    ModulationMatrix mod_matrix;
    
    // Global parameters
    float master_volume;      // 0.0 to 1.0
    float master_tune;        // -100 to +100 cents
    int pitch_bend_range;     // Semitones (default 2)
    
    // Voice management
    bool mono_mode;
    bool legato_mode;
    float glide_time;         // Global glide time
    
    // Protection
    float limiter_threshold;  // 0.0 to 1.0
    float limiter_release;    // Seconds
    float limiter_gain;       // Current gain reduction
    
    // Sample counter (for time-based calculations)
    uint64_t sample_counter;
} SynthEngine;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Engine
void synth_init(SynthEngine* synth, float sample_rate);
void synth_process(SynthEngine* synth, float* output, int num_frames);
void synth_set_tempo(SynthEngine* synth, float bpm);

// MIDI
void synth_note_on(SynthEngine* synth, int note, float velocity);
void synth_note_off(SynthEngine* synth, int note);
void synth_all_notes_off(SynthEngine* synth);
void synth_pitch_bend(SynthEngine* synth, float amount);

// Oscillator
void osc_init(Oscillator* osc, float sample_rate);
float osc_process(Oscillator* osc, float sample_rate, float fm_input);
void osc_set_waveform(Oscillator* osc, WaveformType type);
void osc_set_frequency(Oscillator* osc, float freq);

// Filter
void filter_init(Filter* filter, float sample_rate);
float filter_process(Filter* filter, float input, float cutoff, float resonance);
void filter_set_mode(Filter* filter, FilterMode mode);
void filter_update_coefficients(Filter* filter, float sample_rate);

// Envelope
void envelope_init(Envelope* env);
void envelope_trigger(Envelope* env, float velocity);
void envelope_release(Envelope* env);
float envelope_process(Envelope* env, float sample_rate);
bool envelope_is_active(Envelope* env);

// LFO
void lfo_init(LFO* lfo);
void lfo_trigger(LFO* lfo);
float lfo_process(LFO* lfo, float sample_rate, float tempo);

// Modulation
void mod_matrix_init(ModulationMatrix* matrix);
void mod_matrix_add_slot(ModulationMatrix* matrix, ModSource source, 
                         ModDestination dest, float amount);
void mod_matrix_update_sources(ModulationMatrix* matrix, SynthEngine* synth);
float mod_matrix_get_value(ModulationMatrix* matrix, ModDestination dest);

// Voice
void voice_init(Voice* voice, float sample_rate);
void voice_note_on(Voice* voice, int midi_note, float velocity, uint64_t time);
void voice_note_off(Voice* voice, uint64_t time);
void voice_process(Voice* voice, float* left, float* right, float sample_rate);
bool voice_is_active(Voice* voice);

// Utilities
float midi_to_freq(int midi_note);
float cents_to_ratio(float cents);
float db_to_linear(float db);
float linear_to_db(float linear);
float soft_clip(float x);

#endif // SYNTH_ENGINE_H
