/*
 * PROFESSIONAL SYNTHESIZER WORKSTATION
 * =====================================
 * Features:
 * - QWERTY keyboard input (FL Studio style layout)
 * - 16-step sequencer with pattern management
 * - 8-voice drum machine with real synthesis
 * - Extended effects rack (Distortion, Chorus, Compressor, Delay, Reverb)
 * - Arpeggiator with multiple modes
 * - Online preset library (coming soon)
 * - MIDI I/O support (coming soon)
 * 
 * Keyboard Layout: Q-P = C4-E5, Z-M = C3-B3
 * Shortcuts: SPACE = toggle sequencer, ESC = panic (all notes off)
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "synth_engine.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include "nuklear.h"

#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include "nuklear_glfw_gl3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_PATTERNS 4

// ============================================================================
// LOOP RECORDER SYSTEM
// ============================================================================

#define MAX_RECORDED_EVENTS 1000

typedef enum {
    EVENT_NOTE_ON,
    EVENT_NOTE_OFF
} EventType;

typedef struct {
    EventType type;
    int note;
    float velocity;
    double time;  // Time in seconds from loop start
} RecordedEvent;

typedef struct {
    char name[32];
    RecordedEvent events[MAX_RECORDED_EVENTS];
    int num_events;
    double loop_length;  // Total loop length in seconds
} Loop;

typedef struct {
    Loop loops[16];  // 16 loop slots
    int current_loop;
    
    bool recording;
    bool playing;
    bool overdub;  // Add to existing loop
    
    double loop_start_time;
    double current_time;
    double loop_position;
    
    int next_event_to_play;
    int active_notes[128];  // Track which notes are currently on
} LoopRecorder;

void loop_recorder_init(LoopRecorder* lr) {
    memset(lr, 0, sizeof(LoopRecorder));
    
    for (int i = 0; i < 16; i++) {
        snprintf(lr->loops[i].name, sizeof(lr->loops[i].name), "Loop %d", i + 1);
        lr->loops[i].loop_length = 0.0;
        lr->loops[i].num_events = 0;
    }
    
    strcpy(lr->loops[0].name, "My Loop");
}

void loop_recorder_start_recording(LoopRecorder* lr) {
    lr->recording = true;
    lr->playing = false;
    lr->loop_start_time = lr->current_time;
    lr->loops[lr->current_loop].num_events = 0;
    memset(lr->active_notes, 0, sizeof(lr->active_notes));
    printf("🔴 Recording started\n");
}

void loop_recorder_stop_recording(LoopRecorder* lr, SynthEngine* synth) {
    if (!lr->recording) return;
    
    lr->recording = false;
    Loop* loop = &lr->loops[lr->current_loop];
    loop->loop_length = lr->current_time - lr->loop_start_time;
    
    // Stop all active notes
    for (int i = 0; i < 128; i++) {
        if (lr->active_notes[i]) {
            synth_note_off(synth, i);
            lr->active_notes[i] = 0;
        }
    }
    
    printf("⏸ Recording stopped - %d events, %.2fs long\n", 
           loop->num_events, loop->loop_length);
}

void loop_recorder_record_note_on(LoopRecorder* lr, int note, float velocity) {
    if (!lr->recording) return;
    
    Loop* loop = &lr->loops[lr->current_loop];
    if (loop->num_events >= MAX_RECORDED_EVENTS) {
        printf("⚠️ Loop full!\n");
        return;
    }
    
    RecordedEvent* event = &loop->events[loop->num_events++];
    event->type = EVENT_NOTE_ON;
    event->note = note;
    event->velocity = velocity;
    event->time = lr->current_time - lr->loop_start_time;
    
    lr->active_notes[note] = 1;
    printf("🔴 REC: Note %d ON at %.3fs\n", note, event->time);
}

void loop_recorder_record_note_off(LoopRecorder* lr, int note) {
    if (!lr->recording) return;
    
    Loop* loop = &lr->loops[lr->current_loop];
    if (loop->num_events >= MAX_RECORDED_EVENTS) return;
    
    RecordedEvent* event = &loop->events[loop->num_events++];
    event->type = EVENT_NOTE_OFF;
    event->note = note;
    event->velocity = 0.0f;
    event->time = lr->current_time - lr->loop_start_time;
    
    lr->active_notes[note] = 0;
    printf("🔴 REC: Note %d OFF at %.3fs\n", note, event->time);
}

void loop_recorder_start_playback(LoopRecorder* lr) {
    Loop* loop = &lr->loops[lr->current_loop];
    if (loop->num_events == 0) {
        printf("⚠️ No loop recorded!\n");
        return;
    }
    
    lr->playing = true;
    lr->recording = false;
    lr->loop_position = 0.0;
    lr->next_event_to_play = 0;
    memset(lr->active_notes, 0, sizeof(lr->active_notes));
    printf("▶ Loop playback started\n");
}

void loop_recorder_stop_playback(LoopRecorder* lr, SynthEngine* synth) {
    if (!lr->playing) return;
    
    lr->playing = false;
    
    // Stop all active notes
    for (int i = 0; i < 128; i++) {
        if (lr->active_notes[i]) {
            synth_note_off(synth, i);
            lr->active_notes[i] = 0;
        }
    }
    
    printf("⏸ Loop playback stopped\n");
}

void loop_recorder_clear(LoopRecorder* lr, SynthEngine* synth) {
    // Stop playback/recording first
    if (lr->playing) {
        loop_recorder_stop_playback(lr, synth);
    }
    if (lr->recording) {
        loop_recorder_stop_recording(lr, synth);
    }
    
    // Clear current loop
    Loop* loop = &lr->loops[lr->current_loop];
    loop->num_events = 0;
    loop->loop_length = 0.0;
    
    printf("🗑 Loop cleared\n");
}

void loop_recorder_process(LoopRecorder* lr, SynthEngine* synth, double delta_time) {
    lr->current_time += delta_time;
    
    if (!lr->playing) return;
    
    Loop* loop = &lr->loops[lr->current_loop];
    if (loop->num_events == 0 || loop->loop_length <= 0.0) return;
    
    // Advance loop position
    lr->loop_position += delta_time;
    
    // Loop back to start
    if (lr->loop_position >= loop->loop_length) {
        // Stop all notes before looping
        for (int i = 0; i < 128; i++) {
            if (lr->active_notes[i]) {
                synth_note_off(synth, i);
                lr->active_notes[i] = 0;
            }
        }
        
        lr->loop_position -= loop->loop_length;
        lr->next_event_to_play = 0;
    }
    
    // Play all events that should trigger now
    while (lr->next_event_to_play < loop->num_events) {
        RecordedEvent* event = &loop->events[lr->next_event_to_play];
        
        if (event->time > lr->loop_position) {
            break;  // Wait for this event
        }
        
        if (event->type == EVENT_NOTE_ON) {
            synth_note_on(synth, event->note, event->velocity);
            lr->active_notes[event->note] = 1;
        } else if (event->type == EVENT_NOTE_OFF) {
            synth_note_off(synth, event->note);
            lr->active_notes[event->note] = 0;
        }
        
        lr->next_event_to_play++;
    }
}

// ============================================================================
// DRUM MACHINE
// ============================================================================

#define DRUM_VOICES 8
#define DRUM_STEPS 16

typedef enum {
    DRUM_KICK, DRUM_SNARE, DRUM_CLOSED_HH, 
    DRUM_OPEN_HH, DRUM_CLAP, DRUM_TOM_HI, 
    DRUM_TOM_MID, DRUM_TOM_LOW
} DrumType;

typedef struct {
    DrumType type;
    float phase;        // Current oscillator phase
    float env;          // Current envelope value
    float pitch;        // Base pitch
    float decay;        // Decay time
    float tone;         // Tone control
    float volume;       // 0.0-1.0
} DrumVoice;

typedef struct {
    bool steps[DRUM_VOICES][DRUM_STEPS];      // Active steps
    float velocity[DRUM_VOICES][DRUM_STEPS];  // Per-step velocity
    bool accent[DRUM_VOICES][DRUM_STEPS];     // Accent flag
} DrumPattern;

typedef struct {
    DrumVoice voices[DRUM_VOICES];
    DrumPattern patterns[MAX_PATTERNS];
    int current_pattern;
    int current_step;
    bool playing;
    double next_step_time;
    float sample_rate;
} DrumMachine;

void drum_voice_init(DrumVoice* voice, DrumType type) {
    voice->type = type;
    voice->phase = 0.0f;
    voice->env = 0.0f;
    voice->volume = 0.8f;
    
    switch (type) {
        case DRUM_KICK:
            voice->pitch = 50.0f;
            voice->decay = 0.4f;
            voice->tone = 0.5f;
            break;
        case DRUM_SNARE:
            voice->pitch = 200.0f;
            voice->decay = 0.2f;
            voice->tone = 0.7f;
            break;
        case DRUM_CLOSED_HH:
            voice->pitch = 8000.0f;
            voice->decay = 0.05f;
            voice->tone = 0.8f;
            break;
        case DRUM_OPEN_HH:
            voice->pitch = 8000.0f;
            voice->decay = 0.3f;
            voice->tone = 0.6f;
            break;
        case DRUM_CLAP:
            voice->pitch = 1000.0f;
            voice->decay = 0.1f;
            voice->tone = 0.5f;
            break;
        default:
            voice->pitch = 100.0f;
            voice->decay = 0.3f;
            voice->tone = 0.5f;
            break;
    }
}

void drum_machine_init(DrumMachine* dm, float sample_rate) {
    memset(dm, 0, sizeof(DrumMachine));
    dm->sample_rate = sample_rate;
    
    // Initialize voices
    drum_voice_init(&dm->voices[0], DRUM_KICK);
    drum_voice_init(&dm->voices[1], DRUM_SNARE);
    drum_voice_init(&dm->voices[2], DRUM_CLOSED_HH);
    drum_voice_init(&dm->voices[3], DRUM_OPEN_HH);
    drum_voice_init(&dm->voices[4], DRUM_CLAP);
    drum_voice_init(&dm->voices[5], DRUM_TOM_HI);
    drum_voice_init(&dm->voices[6], DRUM_TOM_MID);
    drum_voice_init(&dm->voices[7], DRUM_TOM_LOW);
    
    // Initialize patterns with default velocities
    for (int p = 0; p < MAX_PATTERNS; p++) {
        for (int v = 0; v < DRUM_VOICES; v++) {
            for (int s = 0; s < DRUM_STEPS; s++) {
                dm->patterns[p].velocity[v][s] = 0.8f;
            }
        }
    }
    
    // Create a demo pattern - basic beat
    DrumPattern* demo = &dm->patterns[0];
    // Kick on 1, 5, 9, 13
    demo->steps[0][0] = demo->steps[0][4] = demo->steps[0][8] = demo->steps[0][12] = true;
    // Snare on 5, 13
    demo->steps[1][4] = demo->steps[1][12] = true;
    // Closed hi-hat on every other step
    for (int i = 0; i < DRUM_STEPS; i += 2) {
        demo->steps[2][i] = true;
    }
}

float drum_voice_process(DrumVoice* voice, float sample_rate) {
    if (voice->env <= 0.0001f) return 0.0f;
    
    float output = 0.0f;
    
    switch (voice->type) {
        case DRUM_KICK: {
            // Kick: Sine wave with pitch envelope
            float pitch_env = voice->env * voice->env * 40.0f;
            float freq = voice->pitch + pitch_env;
            voice->phase += (freq / sample_rate) * 2.0f * M_PI;
            if (voice->phase > 2.0f * M_PI) voice->phase -= 2.0f * M_PI;
            output = sinf(voice->phase) * voice->env;
            break;
        }
        
        case DRUM_SNARE: {
            // Snare: Noise + tone
            float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            voice->phase += (voice->pitch / sample_rate) * 2.0f * M_PI;
            if (voice->phase > 2.0f * M_PI) voice->phase -= 2.0f * M_PI;
            float tone = sinf(voice->phase);
            output = (noise * (1.0f - voice->tone) + tone * voice->tone) * voice->env;
            break;
        }
        
        case DRUM_CLOSED_HH:
        case DRUM_OPEN_HH: {
            // Hi-hat: Filtered noise
            float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            output = noise * voice->env * 0.5f;
            break;
        }
        
        case DRUM_CLAP: {
            // Clap: Multiple noise bursts
            float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float clap_env = voice->env;
            if (voice->env > 0.7f) clap_env += (voice->env - 0.7f) * 2.0f;
            output = noise * clap_env * 0.7f;
            break;
        }
        
        default: {
            // Toms: Sine wave
            voice->phase += (voice->pitch / sample_rate) * 2.0f * M_PI;
            if (voice->phase > 2.0f * M_PI) voice->phase -= 2.0f * M_PI;
            output = sinf(voice->phase) * voice->env;
            break;
        }
    }
    
    // Apply envelope decay
    voice->env *= powf(0.001f, 1.0f / (voice->decay * sample_rate));
    
    return output * voice->volume;
}

void drum_machine_trigger(DrumMachine* dm, int voice_idx, float velocity) {
    if (voice_idx < 0 || voice_idx >= DRUM_VOICES) return;
    dm->voices[voice_idx].env = velocity;
    dm->voices[voice_idx].phase = 0.0f;
}

void drum_machine_process(DrumMachine* dm, double time, double tempo) {
    if (!dm->playing) return;
    
    double beat_duration = 60.0 / tempo;
    double step_duration = beat_duration / 4.0;  // 16th notes
    
    if (time >= dm->next_step_time) {
        DrumPattern* pattern = &dm->patterns[dm->current_pattern];
        
        // Trigger all active voices for this step
        for (int v = 0; v < DRUM_VOICES; v++) {
            if (pattern->steps[v][dm->current_step]) {
                float vel = pattern->velocity[v][dm->current_step];
                if (pattern->accent[v][dm->current_step]) {
                    vel = fminf(vel * 1.3f, 1.0f);  // Boost accent
                }
                drum_machine_trigger(dm, v, vel);
            }
        }
        
        // Advance step
        dm->current_step = (dm->current_step + 1) % DRUM_STEPS;
        dm->next_step_time = time + step_duration;
    }
}

float drum_machine_render(DrumMachine* dm) {
    float mix = 0.0f;
    for (int i = 0; i < DRUM_VOICES; i++) {
        mix += drum_voice_process(&dm->voices[i], dm->sample_rate);
    }
    return mix * 0.5f;  // Scale down to prevent clipping
}

// ============================================================================
// EFFECTS (Extended)
// ============================================================================

// Distortion
typedef struct {
    bool enabled;
    float drive;
    float mix;
} FX_Distortion;

void fx_distortion_process(FX_Distortion* fx, float* left, float* right) {
    if (!fx->enabled) return;
    
    float l_dry = *left, r_dry = *right;
    float l_wet = tanhf(*left * fx->drive);
    float r_wet = tanhf(*right * fx->drive);
    
    *left = l_dry * (1.0f - fx->mix) + l_wet * fx->mix;
    *right = r_dry * (1.0f - fx->mix) + r_wet * fx->mix;
}

// Chorus
typedef struct {
    bool enabled;
    float delay_buffer_l[8820];  // 200ms @ 44.1kHz
    float delay_buffer_r[8820];
    int write_pos;
    float rate;         // LFO rate in Hz
    float depth;        // Modulation depth in samples
    float mix;
    float lfo_phase;
} FX_Chorus;

void fx_chorus_init(FX_Chorus* fx) {
    memset(fx, 0, sizeof(FX_Chorus));
    fx->rate = 0.5f;
    fx->depth = 10.0f;
    fx->mix = 0.5f;
}

void fx_chorus_process(FX_Chorus* fx, float* left, float* right, float sample_rate) {
    if (!fx->enabled) return;
    
    // Store input
    fx->delay_buffer_l[fx->write_pos] = *left;
    fx->delay_buffer_r[fx->write_pos] = *right;
    
    // Update LFO
    fx->lfo_phase += (fx->rate / sample_rate) * 2.0f * M_PI;
    if (fx->lfo_phase > 2.0f * M_PI) fx->lfo_phase -= 2.0f * M_PI;
    
    float lfo = sinf(fx->lfo_phase);
    float delay = fx->depth * (1.0f + lfo);
    
    // Read delayed signal
    float read_pos = fx->write_pos - delay;
    if (read_pos < 0) read_pos += 8820;
    
    int read_idx = (int)read_pos;
    float frac = read_pos - read_idx;
    
    int next_idx = (read_idx + 1) % 8820;
    
    float delayed_l = fx->delay_buffer_l[read_idx] * (1.0f - frac) + 
                      fx->delay_buffer_l[next_idx] * frac;
    float delayed_r = fx->delay_buffer_r[read_idx] * (1.0f - frac) + 
                      fx->delay_buffer_r[next_idx] * frac;
    
    // Mix
    *left = *left * (1.0f - fx->mix) + delayed_l * fx->mix;
    *right = *right * (1.0f - fx->mix) + delayed_r * fx->mix;
    
    fx->write_pos = (fx->write_pos + 1) % 8820;
}

// Delay
typedef struct {
    bool enabled;
    float buffer_l[88200];  // 2 sec @ 44.1kHz
    float buffer_r[88200];
    int write_pos;
    float time;
    float feedback;
    float mix;
} FX_Delay;

void fx_delay_process(FX_Delay* fx, float* left, float* right, float sample_rate) {
    if (!fx->enabled) return;
    
    int delay_samples = (int)(fx->time * sample_rate);
    if (delay_samples >= 88200) delay_samples = 88199;
    
    int read_pos = fx->write_pos - delay_samples;
    if (read_pos < 0) read_pos += 88200;
    
    float delayed_l = fx->buffer_l[read_pos];
    float delayed_r = fx->buffer_r[read_pos];
    
    fx->buffer_l[fx->write_pos] = *left + delayed_l * fx->feedback;
    fx->buffer_r[fx->write_pos] = *right + delayed_r * fx->feedback;
    
    *left = *left * (1.0f - fx->mix) + delayed_l * fx->mix;
    *right = *right * (1.0f - fx->mix) + delayed_r * fx->mix;
    
    fx->write_pos = (fx->write_pos + 1) % 88200;
}

// Simple Reverb
typedef struct {
    bool enabled;
    float buffer[88200];
    int write_pos;
    float size;
    float damping;
    float mix;
} FX_Reverb;

void fx_reverb_process(FX_Reverb* fx, float* left, float* right) {
    if (!fx->enabled) return;
    
    float input = (*left + *right) * 0.5f;
    fx->buffer[fx->write_pos] = input + fx->buffer[fx->write_pos] * fx->damping;
    
    int read_pos = fx->write_pos - (int)(fx->size * 44100.0f);
    if (read_pos < 0) read_pos += 88200;
    
    float reverb_out = fx->buffer[read_pos];
    
    *left = *left * (1.0f - fx->mix) + reverb_out * fx->mix;
    *right = *right * (1.0f - fx->mix) + reverb_out * fx->mix;
    
    fx->write_pos = (fx->write_pos + 1) % 88200;
}

// Compressor
typedef struct {
    bool enabled;
    float threshold;    // 0.0-1.0
    float ratio;        // 1.0-20.0
    float attack;       // seconds
    float release;      // seconds
    float makeup_gain;  // 0.0-2.0
    float envelope;
} FX_Compressor;

void fx_compressor_process(FX_Compressor* fx, float* left, float* right, float sample_rate) {
    if (!fx->enabled) return;
    
    float input_level = fmaxf(fabsf(*left), fabsf(*right));
    
    // Envelope follower
    float attack_coef = expf(-1.0f / (fx->attack * sample_rate));
    float release_coef = expf(-1.0f / (fx->release * sample_rate));
    
    if (input_level > fx->envelope) {
        fx->envelope = attack_coef * fx->envelope + (1.0f - attack_coef) * input_level;
    } else {
        fx->envelope = release_coef * fx->envelope + (1.0f - release_coef) * input_level;
    }
    
    // Compression
    float gain = 1.0f;
    if (fx->envelope > fx->threshold) {
        float over = fx->envelope / fx->threshold;
        gain = powf(over, (1.0f / fx->ratio) - 1.0f);
    }
    
    *left *= gain * fx->makeup_gain;
    *right *= gain * fx->makeup_gain;
}

typedef struct {
    FX_Distortion distortion;
    FX_Chorus chorus;
    FX_Delay delay;
    FX_Reverb reverb;
    FX_Compressor compressor;
} EffectsRack;

// ============================================================================
// ARPEGGIATOR
// ============================================================================

typedef enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UP_DOWN, ARP_RANDOM } ArpMode;

typedef struct {
    bool enabled;
    ArpMode mode;
    float rate;
    float gate;
    int held_notes[16];
    int num_held;
    int current_step;
    double next_step_time;
    int last_played_note;
} Arpeggiator;

void arp_init(Arpeggiator* arp) {
    memset(arp, 0, sizeof(Arpeggiator));
    arp->rate = 2.0f;
    arp->gate = 0.8f;
    arp->mode = ARP_UP;
    arp->last_played_note = -1;
}

// ============================================================================
// KEYBOARD MAPPING (FL Studio Style)
// ============================================================================

typedef struct { int glfw_key; int midi_note; const char* label; } KeyMapping;

KeyMapping g_keymap[] = {
    // Bottom row (Octave 2-3)
    {GLFW_KEY_Z, 48, "C3"}, {GLFW_KEY_S, 49, "C#3"}, {GLFW_KEY_X, 50, "D3"},
    {GLFW_KEY_D, 51, "D#3"}, {GLFW_KEY_C, 52, "E3"}, {GLFW_KEY_V, 53, "F3"},
    {GLFW_KEY_G, 54, "F#3"}, {GLFW_KEY_B, 55, "G3"}, {GLFW_KEY_H, 56, "G#3"},
    {GLFW_KEY_N, 57, "A3"}, {GLFW_KEY_J, 58, "A#3"}, {GLFW_KEY_M, 59, "B3"},
    {GLFW_KEY_COMMA, 60, "C4"},
    
    // Top row (Octave 3-4)
    {GLFW_KEY_Q, 60, "C4"}, {GLFW_KEY_2, 61, "C#4"}, {GLFW_KEY_W, 62, "D4"},
    {GLFW_KEY_3, 63, "D#4"}, {GLFW_KEY_E, 64, "E4"}, {GLFW_KEY_R, 65, "F4"},
    {GLFW_KEY_5, 66, "F#4"}, {GLFW_KEY_T, 67, "G4"}, {GLFW_KEY_6, 68, "G#4"},
    {GLFW_KEY_Y, 69, "A4"}, {GLFW_KEY_7, 70, "A#4"}, {GLFW_KEY_U, 71, "B4"},
    {GLFW_KEY_I, 72, "C5"}, {GLFW_KEY_9, 73, "C#5"}, {GLFW_KEY_O, 74, "D5"},
    {GLFW_KEY_0, 75, "D#5"}, {GLFW_KEY_P, 76, "E5"},
};
#define KEYMAP_SIZE (sizeof(g_keymap) / sizeof(g_keymap[0]))

void arp_note_on(Arpeggiator* arp, int note) {
    if (arp->num_held < 16) {
        arp->held_notes[arp->num_held++] = note;
    }
}

void arp_note_off(Arpeggiator* arp, int note) {
    for (int i = 0; i < arp->num_held; i++) {
        if (arp->held_notes[i] == note) {
            for (int j = i; j < arp->num_held - 1; j++) {
                arp->held_notes[j] = arp->held_notes[j + 1];
            }
            arp->num_held--;
            if (i <= arp->current_step || arp->current_step >= arp->num_held) {
                arp->current_step = 0;
            }
            break;
        }
    }
}

void arp_process(Arpeggiator* arp, SynthEngine* synth, double time, double tempo) {
    if (!arp->enabled || arp->num_held == 0) {
        if (arp->last_played_note >= 0) {
            synth_note_off(synth, arp->last_played_note);
            arp->last_played_note = -1;
        }
        return;
    }
    
    double beat_duration = 60.0 / tempo;
    double step_duration = beat_duration / arp->rate;
    
    if (time >= arp->next_step_time) {
        if (arp->last_played_note >= 0) {
            synth_note_off(synth, arp->last_played_note);
        }
        
        if (arp->mode == ARP_UP) {
            arp->current_step = (arp->current_step + 1) % arp->num_held;
        } else if (arp->mode == ARP_DOWN) {
            arp->current_step = (arp->current_step - 1 + arp->num_held) % arp->num_held;
        } else if (arp->mode == ARP_RANDOM) {
            arp->current_step = rand() % arp->num_held;
        }
        
        int note_to_play = arp->held_notes[arp->current_step];
        synth_note_on(synth, note_to_play, 0.8f);
        arp->last_played_note = note_to_play;
        
        arp->next_step_time = time + step_duration * arp->gate;
    }
}

// ============================================================================
// APPLICATION STATE
// ============================================================================

typedef struct {
    SynthEngine synth;
    EffectsRack fx;
    Arpeggiator arp;
    LoopRecorder loop_recorder;
    DrumMachine drums;
    
    ma_device audio_device;
    GLFWwindow* window;
    struct nk_glfw glfw;
    struct nk_context* nk_ctx;
    struct nk_colorf bg;
    
    double current_time;
    float tempo;
    int playing;
    int active_tab;
    
    // Keyboard state tracking
    int keys_pressed[KEYMAP_SIZE];
    bool record_mode;  // Recording keyboard to sequencer
    
    // UI state
    float osc1_detune;
    int osc1_unison;
    float osc1_pwm;
    WaveformType osc1_wave;
    
    float filter_cutoff;
    float filter_resonance;
    FilterMode filter_mode;
    float filter_env;
    
    float env_attack;
    float env_decay;
    float env_sustain;
    float env_release;
    
    float master_volume;
    
    int arp_enabled;
} AppState;

AppState g_app = {0};

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    float* out = (float*)output;
    (void)input;
    
    double sample_duration = 1.0 / g_app.synth.sample_rate;
    
    for (ma_uint32 i = 0; i < frameCount; i++) {
        // Process loop recorder
        loop_recorder_process(&g_app.loop_recorder, &g_app.synth, sample_duration);
        
        // Process drum machine
        drum_machine_process(&g_app.drums, g_app.current_time, g_app.tempo);
        
        // Process arpeggiator
        arp_process(&g_app.arp, &g_app.synth, g_app.current_time, g_app.tempo);
        
        // Generate synth audio
        float temp_buffer[2];
        synth_process(&g_app.synth, temp_buffer, 1);
        float left = temp_buffer[0];
        float right = temp_buffer[1];
        
        // Add drum machine
        float drums = drum_machine_render(&g_app.drums);
        left += drums;
        right += drums;
        
        // Apply effects
        fx_distortion_process(&g_app.fx.distortion, &left, &right);
        fx_chorus_process(&g_app.fx.chorus, &left, &right, g_app.synth.sample_rate);
        fx_delay_process(&g_app.fx.delay, &left, &right, g_app.synth.sample_rate);
        fx_reverb_process(&g_app.fx.reverb, &left, &right);
        fx_compressor_process(&g_app.fx.compressor, &left, &right, g_app.synth.sample_rate);
        
        // Master volume
        left *= g_app.master_volume;
        right *= g_app.master_volume;
        
        out[i * 2 + 0] = left;
        out[i * 2 + 1] = right;
        
        g_app.current_time += sample_duration;
    }
}

// ============================================================================
// GUI
// ============================================================================

void draw_gui(struct nk_context* ctx) {
    const char* tab_names[] = {"SYNTH", "FX", "SEQUENCER", "DRUMS", "PRESETS"};
    
    if (nk_begin(ctx, "Professional Synthesizer", nk_rect(0, 0, 1600, 900),
                 NK_WINDOW_NO_SCROLLBAR)) {
        
        // Tab bar
        nk_layout_row_static(ctx, 30, 150, 5);
        for (int i = 0; i < 5; i++) {
            if (nk_button_label(ctx, tab_names[i])) {
                g_app.active_tab = i;
            }
        }
        
        nk_layout_row_dynamic(ctx, 5, 1);
        nk_label(ctx, "", NK_TEXT_LEFT);
        
        // Content based on active tab
        switch (g_app.active_tab) {
            case 0: { // SYNTH
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "═══ SYNTH ENGINE ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Master Volume:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.0f, &g_app.master_volume, 1.0f, 0.01f);
                
                // Voice count display
                nk_layout_row_dynamic(ctx, 25, 1);
                char voice_text[64];
                snprintf(voice_text, sizeof(voice_text), "Active Voices: %d / 8", 
                        g_app.synth.num_active_voices);
                nk_label(ctx, voice_text, NK_TEXT_CENTERED);
                
                break;
            }
            
            case 1: { // FX
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "═══ EFFECTS RACK ═══", NK_TEXT_CENTERED);
                
                // Distortion
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_checkbox_label(ctx, "Distortion", (int*)&g_app.fx.distortion.enabled);
                nk_label(ctx, "", NK_TEXT_LEFT);
                
                if (g_app.fx.distortion.enabled) {
                    nk_label(ctx, "Drive:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 1.0f, &g_app.fx.distortion.drive, 20.0f, 0.1f);
                    nk_label(ctx, "Mix:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.mix, 1.0f, 0.01f);
                }
                
                // Chorus
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_checkbox_label(ctx, "Chorus", (int*)&g_app.fx.chorus.enabled);
                nk_label(ctx, "", NK_TEXT_LEFT);
                
                if (g_app.fx.chorus.enabled) {
                    nk_label(ctx, "Rate:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 0.1f, &g_app.fx.chorus.rate, 5.0f, 0.1f);
                    nk_label(ctx, "Depth:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 1.0f, &g_app.fx.chorus.depth, 50.0f, 1.0f);
                    nk_label(ctx, "Mix:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 0.0f, &g_app.fx.chorus.mix, 1.0f, 0.01f);
                }
                
                // Compressor
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_checkbox_label(ctx, "Compressor", (int*)&g_app.fx.compressor.enabled);
                nk_label(ctx, "", NK_TEXT_LEFT);
                
                if (g_app.fx.compressor.enabled) {
                    nk_label(ctx, "Threshold:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 0.0f, &g_app.fx.compressor.threshold, 1.0f, 0.01f);
                    nk_label(ctx, "Ratio:", NK_TEXT_LEFT);
                    nk_slider_float(ctx, 1.0f, &g_app.fx.compressor.ratio, 20.0f, 0.1f);
                }
                
                break;
            }
            
            case 2: { // LOOP RECORDER
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "═══ LOOP RECORDER ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 30, 4);
                
                // Play/Stop button
                if (nk_button_label(ctx, g_app.loop_recorder.playing ? "⏸ STOP" : "▶ PLAY")) {
                    if (g_app.loop_recorder.playing) {
                        loop_recorder_stop_playback(&g_app.loop_recorder, &g_app.synth);
                    } else {
                        loop_recorder_start_playback(&g_app.loop_recorder);
                    }
                }
                
                // Record button
                struct nk_style_button rec_style = ctx->style.button;
                if (g_app.loop_recorder.recording) {
                    rec_style.normal.data.color = nk_rgb(255, 0, 0);  // Red when recording
                    rec_style.hover.data.color = nk_rgb(255, 50, 50);
                }
                if (nk_button_label_styled(ctx, &rec_style, g_app.loop_recorder.recording ? "🔴 REC" : "⚫ REC")) {
                    if (g_app.loop_recorder.recording) {
                        loop_recorder_stop_recording(&g_app.loop_recorder, &g_app.synth);
                    } else {
                        loop_recorder_start_recording(&g_app.loop_recorder);
                    }
                }
                
                // Clear button
                if (nk_button_label(ctx, "🗑 Clear")) {
                    loop_recorder_clear(&g_app.loop_recorder, &g_app.synth);
                }
                
                // Tempo control
                nk_label(ctx, "BPM:", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Tempo:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 60.0f, &g_app.tempo, 200.0f, 1.0f);
                
                char tempo_text[64];
                snprintf(tempo_text, sizeof(tempo_text), "%.0f BPM", g_app.tempo);
                nk_label(ctx, tempo_text, NK_TEXT_LEFT);
                
                // Loop info
                Loop* loop = &g_app.loop_recorder.loops[g_app.loop_recorder.current_loop];
                char loop_info[128];
                snprintf(loop_info, sizeof(loop_info), "Loop: %s", loop->name);
                nk_label(ctx, loop_info, NK_TEXT_LEFT);
                
                snprintf(loop_info, sizeof(loop_info), "Events: %d | Length: %.2fs", 
                         loop->num_events, loop->loop_length);
                nk_label(ctx, loop_info, NK_TEXT_LEFT);
                
                if (g_app.loop_recorder.playing || g_app.loop_recorder.recording) {
                    snprintf(loop_info, sizeof(loop_info), "Position: %.2fs", 
                             g_app.loop_recorder.loop_position);
                    nk_label(ctx, loop_info, NK_TEXT_LEFT);
                }
                
                // Instructions
                nk_layout_row_dynamic(ctx, 40, 1);
                if (g_app.loop_recorder.recording) {
                    nk_label(ctx, "🔴 RECORDING: Play keyboard to record notes\nNotes sustain naturally until you release the keys", 
                             NK_TEXT_CENTERED);
                } else if (g_app.loop_recorder.playing) {
                    nk_label(ctx, "▶ PLAYING: Loop is playing back your recording", 
                             NK_TEXT_CENTERED);
                } else {
                    nk_label(ctx, "💡 Press REC and play keyboard to record a loop\nPress PLAY to hear your recording loop", 
                             NK_TEXT_CENTERED);
                }
                
                break;
            }
            
            case 3: { // DRUMS
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "═══ DRUM MACHINE ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 30, 3);
                if (nk_button_label(ctx, g_app.drums.playing ? "⏸ STOP" : "▶ PLAY")) {
                    g_app.drums.playing = !g_app.drums.playing;
                }
                
                char step_info[64];
                snprintf(step_info, sizeof(step_info), "Step: %d/16", g_app.drums.current_step + 1);
                nk_label(ctx, step_info, NK_TEXT_CENTERED);
                nk_label(ctx, "", NK_TEXT_LEFT);
                
                // Drum grid with current step highlight
                const char* drum_names[] = {"KICK", "SNARE", "CL-HH", "OP-HH", 
                                           "CLAP", "TOM-H", "TOM-M", "TOM-L"};
                
                DrumPattern* dpat = &g_app.drums.patterns[g_app.drums.current_pattern];
                int current_step = g_app.drums.current_step;
                
                for (int d = 0; d < DRUM_VOICES; d++) {
                    nk_layout_row_begin(ctx, NK_STATIC, 30, 17);
                    nk_layout_row_push(ctx, 60);
                    nk_label(ctx, drum_names[d], NK_TEXT_LEFT);
                    
                    for (int s = 0; s < 16; s++) {
                        nk_layout_row_push(ctx, 30);
                        
                        // Highlight current playing step
                        struct nk_style_button style = ctx->style.button;
                        if (s == current_step && g_app.drums.playing) {
                            style.normal.data.color = nk_rgb(255, 200, 0);  // Yellow highlight
                            style.hover.data.color = nk_rgb(255, 220, 50);
                        } else if (dpat->steps[d][s]) {
                            style.normal.data.color = nk_rgb(100, 200, 100);  // Green when active
                            style.hover.data.color = nk_rgb(120, 220, 120);
                        }
                        
                        if (nk_button_label_styled(ctx, &style, dpat->steps[d][s] ? "●" : "○")) {
                            dpat->steps[d][s] = !dpat->steps[d][s];
                            printf("🥁 Drum %s Step %d: %s\n", drum_names[d], s + 1, 
                                   dpat->steps[d][s] ? "ON" : "OFF");
                        }
                    }
                    nk_layout_row_end(ctx);
                }
                
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "💡 Click ● or ○ to toggle drum hits", NK_TEXT_CENTERED);
                
                break;
            }
            
            case 4: { // PRESETS
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "═══ PRESET BROWSER ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Online preset library coming soon!", NK_TEXT_CENTERED);
                nk_label(ctx, "Will feature:", NK_TEXT_LEFT);
                nk_label(ctx, "• Download presets from GitHub", NK_TEXT_LEFT);
                nk_label(ctx, "• Browse by category (Bass/Lead/Pad/Keys/FX)", NK_TEXT_LEFT);
                nk_label(ctx, "• Search and filter", NK_TEXT_LEFT);
                nk_label(ctx, "• Save and share your own presets", NK_TEXT_LEFT);
                
                break;
            }
        }
    }
    nk_end(ctx);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("🎹 Professional Synthesizer Workstation\n");
    printf("========================================\n\n");
    
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    g_app.window = glfwCreateWindow(1600, 900, "Pro Synth", NULL, NULL);
    if (!g_app.window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(g_app.window);
    glfwSwapInterval(1);
    
    // Initialize Nuklear (using direct polling for keyboard instead of callbacks)
    g_app.nk_ctx = nk_glfw3_init(&g_app.glfw, g_app.window, NK_GLFW3_INSTALL_CALLBACKS);
    
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(&g_app.glfw, &atlas);
    nk_glfw3_font_stash_end(&g_app.glfw);
    
    g_app.bg.r = 0.10f; g_app.bg.g = 0.18f;
    g_app.bg.b = 0.24f; g_app.bg.a = 1.0f;
    
    // Initialize synth engine
    synth_init(&g_app.synth, 44100.0f);
    
    // Initialize effects
    g_app.fx.distortion.drive = 5.0f;
    g_app.fx.distortion.mix = 0.5f;
    fx_chorus_init(&g_app.fx.chorus);
    g_app.fx.delay.time = 0.3f;
    g_app.fx.delay.feedback = 0.4f;
    g_app.fx.delay.mix = 0.3f;
    g_app.fx.reverb.size = 0.5f;
    g_app.fx.reverb.damping = 0.5f;
    g_app.fx.reverb.mix = 0.3f;
    g_app.fx.compressor.threshold = 0.7f;
    g_app.fx.compressor.ratio = 4.0f;
    g_app.fx.compressor.attack = 0.005f;
    g_app.fx.compressor.release = 0.1f;
    g_app.fx.compressor.makeup_gain = 1.5f;
    
    // Initialize loop recorder, drums, arp
    loop_recorder_init(&g_app.loop_recorder);
    drum_machine_init(&g_app.drums, 44100.0f);
    arp_init(&g_app.arp);
    
    // Set defaults
    g_app.tempo = 120.0f;
    g_app.master_volume = 0.7f;
    
    // Initialize audio
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    
    if (ma_device_init(NULL, &config, &g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device\n");
        return -1;
    }
    
    if (ma_device_start(&g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start audio device\n");
        ma_device_uninit(&g_app.audio_device);
        return -1;
    }
    
    printf("✅ Audio started\n");
    printf("✅ GUI initialized\n\n");
    printf("Features:\n");
    printf("• 16-Step Sequencer with patterns\n");
    printf("• 8-Voice Drum Machine\n");
    printf("• Extended Effects Rack\n");
    printf("• Arpeggiator\n");
    printf("• No keyboard input - pure GUI workflow\n\n");
    
    // Main loop
    while (!glfwWindowShouldClose(g_app.window)) {
        glfwPollEvents();
        
        // Poll keyboard directly (bypass Nuklear stealing input)
        for (size_t i = 0; i < KEYMAP_SIZE; i++) {
            int state = glfwGetKey(g_app.window, g_keymap[i].glfw_key);
            
            // Key just pressed
            if (state == GLFW_PRESS && !g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 1;
                
                // Record note on if recording
                if (g_app.loop_recorder.recording) {
                    loop_recorder_record_note_on(&g_app.loop_recorder, 
                                                 g_keymap[i].midi_note, 1.0f);
                }
                
                if (g_app.arp_enabled) {
                    arp_note_on(&g_app.arp, g_keymap[i].midi_note);
                } else {
                    synth_note_on(&g_app.synth, g_keymap[i].midi_note, 1.0f);
                }
                
                printf("🎹 %s ON\n", g_keymap[i].label);
            }
            // Key just released
            else if (state == GLFW_RELEASE && g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 0;
                
                // Record note off if recording
                if (g_app.loop_recorder.recording) {
                    loop_recorder_record_note_off(&g_app.loop_recorder, 
                                                  g_keymap[i].midi_note);
                }
                
                if (g_app.arp_enabled) {
                    arp_note_off(&g_app.arp, g_keymap[i].midi_note);
                } else {
                    synth_note_off(&g_app.synth, g_keymap[i].midi_note);
                }
                
                printf("🎹 %s OFF\n", g_keymap[i].label);
            }
        }
        
        // Space = toggle loop playback
        if (glfwGetKey(g_app.window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            static bool space_was_pressed = false;
            if (!space_was_pressed) {
                if (g_app.loop_recorder.playing) {
                    loop_recorder_stop_playback(&g_app.loop_recorder, &g_app.synth);
                } else {
                    loop_recorder_start_playback(&g_app.loop_recorder);
                }
                space_was_pressed = true;
            }
        } else {
            static bool space_was_pressed = false;
            space_was_pressed = false;
        }
        
        // ESC = panic
        if (glfwGetKey(g_app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            synth_all_notes_off(&g_app.synth);
            g_app.arp.num_held = 0;
        }
        
        nk_glfw3_new_frame(&g_app.glfw);
        draw_gui(g_app.nk_ctx);
        
        int width, height;
        glfwGetWindowSize(g_app.window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(g_app.bg.r, g_app.bg.g, g_app.bg.b, g_app.bg.a);
        
        nk_glfw3_render(&g_app.glfw, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
        glfwSwapBuffers(g_app.window);
    }
    
    // Cleanup
    ma_device_uninit(&g_app.audio_device);
    nk_glfw3_shutdown(&g_app.glfw);
    glfwTerminate();
    
    printf("\n👋 Goodbye!\n");
    return 0;
}
