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
 * - Hardware MIDI input via CoreMIDI
 * 
 * Keyboard Layout: Q-P = C4-E5, Z-M = C3-B3
 * Shortcuts: SPACE = toggle sequencer, ESC = panic (all notes off)
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "synth_engine.h"
#include "param_queue.h"
#include "midi_input.h"
#include "sample_io.h"
#include "preset.h"
#include "project.h"

#include "nuklear_config.h"
#include "nuklear.h"

#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include "nuklear_glfw_gl3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <limits.h>
#include <stdint.h>

static inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void enqueue_param_float(ParamId id, float value) {
    ParamMsg msg = {.id = (uint32_t)id, .type = PARAM_FLOAT};
    msg.value.f = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "[PARAM] drop float id=%u\n", msg.id);
    }
}

static void enqueue_param_bool(ParamId id, bool value) {
    ParamMsg msg = {.id = (uint32_t)id, .type = PARAM_BOOL};
    msg.value.b = value ? 1 : 0;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "[PARAM] drop bool id=%u\n", msg.id);
    }
}

static void enqueue_param_int(ParamId id, int value) {
    ParamMsg msg = {.id = (uint32_t)id, .type = PARAM_INT};
    msg.value.i = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "[PARAM] drop int id=%u\n", msg.id);
    }
}

static float param_msg_as_float(const ParamMsg* msg) {
    if (!msg) return 0.0f;
    switch (msg->type) {
        case PARAM_FLOAT: return msg->value.f;
        case PARAM_INT:   return (float)msg->value.i;
        case PARAM_BOOL:  return msg->value.b ? 1.0f : 0.0f;
        default:          return 0.0f;
    }
}

static int param_msg_as_int(const ParamMsg* msg) {
    if (!msg) return 0;
    switch (msg->type) {
        case PARAM_INT:   return msg->value.i;
        case PARAM_FLOAT: return (int)lroundf(msg->value.f);
        case PARAM_BOOL:  return msg->value.b != 0;
        default:          return 0;
    }
}

static bool param_msg_as_bool(const ParamMsg* msg) {
    if (!msg) return false;
    switch (msg->type) {
        case PARAM_BOOL:  return msg->value.b != 0;
        case PARAM_INT:   return msg->value.i != 0;
        case PARAM_FLOAT: return fabsf(msg->value.f) > 0.5f;
        default:          return false;
    }
}

static void dispatch_note_on(uint8_t note, float velocity);
static void dispatch_note_off(uint8_t note);
static void apply_param_change(const ParamMsg* change, void* userdata);

static bool is_black_key(int midi_note) {
    int mod = midi_note % 12;
    return mod == 1 || mod == 3 || mod == 6 || mod == 8 || mod == 10;
}

static const char* midi_note_label(int midi_note, char* buffer, size_t size) {
    static const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int mod = (midi_note % 12 + 12) % 12;
    int octave = midi_note / 12 - 1;
    snprintf(buffer, size, "%s%d", note_names[mod], octave);
    return buffer;
}

typedef struct {
    int base_note;
    int num_keys;
    bool note_active[128];
    bool mouse_dragging;
    int mouse_active_note;
} KeyboardUIState;

static void ui_apply_theme(struct nk_context* ctx);
static void draw_transport_bar(struct nk_context* ctx);

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_PATTERNS 4
#define MAX_RECORDED_EVENTS 2048

typedef enum {
    EVENT_NOTE_ON = 0,
    EVENT_NOTE_OFF = 1
} EventType;

typedef struct {
    int note;
    float velocity;
    double time;  // Time in seconds from loop start
    EventType type;
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
    double loop_start_time;
    double current_time;
    double loop_position;
    int next_event_to_play;
    uint8_t active_notes[128];
} LoopRecorder;

void loop_recorder_init(LoopRecorder* lr) {
    if (!lr) return;
    memset(lr, 0, sizeof(LoopRecorder));
    lr->current_loop = 0;
    for (int i = 0; i < 16; i++) {
        snprintf(lr->loops[i].name, sizeof(lr->loops[i].name), "Loop %02d", i + 1);
        lr->loops[i].num_events = 0;
        lr->loops[i].loop_length = 0.0;
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
    (void)synth;
    
    lr->recording = false;
    Loop* loop = &lr->loops[lr->current_loop];
    loop->loop_length = lr->current_time - lr->loop_start_time;
    double stop_time = loop->loop_length;
    
    // Stop all active notes
    for (int i = 0; i < 128; i++) {
        if (lr->active_notes[i]) {
            if (loop->num_events < MAX_RECORDED_EVENTS) {
                RecordedEvent* event = &loop->events[loop->num_events++];
                event->type = EVENT_NOTE_OFF;
                event->note = i;
                event->velocity = 0.0f;
                event->time = stop_time;
            } else {
                printf("⚠️ Loop full while stopping recording - dropping note offs\n");
            }
            dispatch_note_off((uint8_t)i);
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
    (void)synth;
    
    lr->playing = false;
    
    // Stop all active notes
    for (int i = 0; i < 128; i++) {
        if (lr->active_notes[i]) {
            dispatch_note_off((uint8_t)i);
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
    (void)synth;
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
                dispatch_note_off((uint8_t)i);
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
            dispatch_note_on((uint8_t)event->note, event->velocity);
            lr->active_notes[event->note] = 1;
        } else if (event->type == EVENT_NOTE_OFF) {
            dispatch_note_off((uint8_t)event->note);
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

// Lightweight copies that hold only UI-facing parameter values
typedef struct {
    nk_bool enabled;
    float drive;
    float mix;
} FX_DistortionUI;

typedef struct {
    nk_bool enabled;
    float rate;
    float depth;
    float mix;
} FX_ChorusUI;

typedef struct {
    nk_bool enabled;
    float time;
    float feedback;
    float mix;
} FX_DelayUI;

typedef struct {
    nk_bool enabled;
    float size;
    float damping;
    float mix;
} FX_ReverbUI;

typedef struct {
    nk_bool enabled;
    float threshold;
    float ratio;
} FX_CompressorUI;

typedef struct {
    FX_DistortionUI distortion;
    FX_ChorusUI chorus;
    FX_DelayUI delay;
    FX_ReverbUI reverb;
    FX_CompressorUI compressor;
} FX_UIState;

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

static const float k_arp_rate_multipliers[] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f};
static const int k_arp_rate_multiplier_count = (int)(sizeof(k_arp_rate_multipliers) / sizeof(k_arp_rate_multipliers[0]));
static const int k_arp_rate_default_index = 1;

static float arp_multiplier_value(int index) {
    if (index < 0) index = 0;
    if (index >= k_arp_rate_multiplier_count) index = k_arp_rate_multiplier_count - 1;
    return k_arp_rate_multipliers[index];
}

static int arp_multiplier_index_for_value(float value) {
    int best = 0;
    float best_diff = fabsf(value - k_arp_rate_multipliers[0]);
    for (int i = 1; i < k_arp_rate_multiplier_count; i++) {
        float diff = fabsf(value - k_arp_rate_multipliers[i]);
        if (diff < best_diff) {
            best = i;
            best_diff = diff;
        }
    }
    return best;
}

void arp_init(Arpeggiator* arp) {
    memset(arp, 0, sizeof(Arpeggiator));
    arp->rate = arp_multiplier_value(k_arp_rate_default_index);
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

typedef struct {
    SampleBuffer buffer;
    double playhead;
    double playback_step;
    atomic_bool trigger_pending;
    atomic_bool stop_pending;
    atomic_bool playing;
    float gain;
    char last_loaded_path[512];
} SampleSlot;

typedef struct {
    atomic_bool arm_request;
    atomic_bool capture_active;
    atomic_bool capture_complete;
    atomic_bool capture_failed;
    float* buffer;
    uint32_t max_frames;
    uint32_t frames_captured;
    uint32_t channels;
    uint32_t sample_rate;
    float duration_seconds;
    char output_path[512];
} SampleExportState;

static void sample_slot_mix(SampleSlot* slot, float sample_rate, float* left, float* right);
static void sample_export_process_frame(SampleExportState* state, float left, float right);
static bool is_black_key(int midi_note);
static const char* midi_note_label(int midi_note, char* buffer, size_t size);
static void keyboard_set_note_active(uint8_t note, bool down);
static void ui_note_on(uint8_t note);
static void ui_note_off(uint8_t note);

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
    FX_UIState fx_ui;
    Arpeggiator arp;
    LoopRecorder loop_recorder;
    DrumMachine drums;
    SampleSlot sampler;
    SampleExportState export_state;
    
    ma_device audio_device;
    GLFWwindow* window;
    struct nk_glfw glfw;
    struct nk_context* nk_ctx;
    struct nk_colorf bg;
    
    double current_time;
    float tempo;
    float tempo_ui;
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
    float master_volume_ui;

    struct {
        nk_bool enabled;
        int rate_index;
        int mode;
    } arp_ui;

    KeyboardUIState keyboard_ui;

    char sample_path_input[512];
    char export_path_input[512];
    char preset_path_input[512];
    char project_path_input[512];
    float export_duration_seconds;
    char sample_status[256];
    char export_status[256];
    char preset_status[256];
    char project_status[256];

    PresetData current_preset;
    ProjectData current_project;
    bool preset_dirty;
    bool project_dirty;
    int dirty_suppression_count;
} AppState;

AppState g_app = {0};

static inline void begin_dirty_suppression(void) {
    g_app.dirty_suppression_count++;
}

static inline void end_dirty_suppression(void) {
    if (g_app.dirty_suppression_count > 0) {
        g_app.dirty_suppression_count--;
    }
}

static inline bool dirty_updates_blocked(void) {
    return g_app.dirty_suppression_count > 0;
}

static inline void mark_project_dirty(void) {
    if (!dirty_updates_blocked()) {
        g_app.project_dirty = true;
    }
}

static inline void mark_preset_dirty(void) {
    if (!dirty_updates_blocked()) {
        g_app.preset_dirty = true;
        g_app.project_dirty = true;
    }
}

static void enqueue_param_float_user(ParamId id, float value) {
    enqueue_param_float(id, value);
    mark_preset_dirty();
}

static void enqueue_param_bool_user(ParamId id, bool value) {
    enqueue_param_bool(id, value);
    mark_preset_dirty();
}

static void enqueue_param_int_user(ParamId id, int value) {
    enqueue_param_int(id, value);
    mark_preset_dirty();
}

static void capture_preset_from_app(PresetData* preset) {
    if (!preset) {
        return;
    }

    preset->version = PRESET_SCHEMA_VERSION;
    preset->tempo = g_app.tempo_ui;
    preset->master_volume = g_app.master_volume_ui;
    preset->filter_cutoff = g_app.filter_cutoff;
    preset->filter_resonance = g_app.filter_resonance;
    preset->filter_mode = (int)g_app.filter_mode;
    preset->filter_env_amount = g_app.filter_env;
    preset->env_attack = g_app.env_attack;
    preset->env_decay = g_app.env_decay;
    preset->env_sustain = g_app.env_sustain;
    preset->env_release = g_app.env_release;

    preset->distortion.enabled = g_app.fx_ui.distortion.enabled ? true : false;
    preset->distortion.drive = g_app.fx_ui.distortion.drive;
    preset->distortion.mix = g_app.fx_ui.distortion.mix;

    preset->chorus.enabled = g_app.fx_ui.chorus.enabled ? true : false;
    preset->chorus.rate = g_app.fx_ui.chorus.rate;
    preset->chorus.depth = g_app.fx_ui.chorus.depth;
    preset->chorus.mix = g_app.fx_ui.chorus.mix;

    preset->delay.enabled = g_app.fx_ui.delay.enabled ? true : false;
    preset->delay.time = g_app.fx_ui.delay.time;
    preset->delay.feedback = g_app.fx_ui.delay.feedback;
    preset->delay.mix = g_app.fx_ui.delay.mix;

    preset->reverb.enabled = g_app.fx_ui.reverb.enabled ? true : false;
    preset->reverb.size = g_app.fx_ui.reverb.size;
    preset->reverb.damping = g_app.fx_ui.reverb.damping;
    preset->reverb.mix = g_app.fx_ui.reverb.mix;

    preset->compressor.enabled = g_app.fx_ui.compressor.enabled ? true : false;
    preset->compressor.threshold = g_app.fx_ui.compressor.threshold;
    preset->compressor.ratio = g_app.fx_ui.compressor.ratio;

    preset->arp.enabled = g_app.arp_ui.enabled ? true : false;
    preset->arp.rate_multiplier = arp_multiplier_value(g_app.arp_ui.rate_index);
    preset->arp.mode = g_app.arp_ui.mode;
}

static void apply_preset_to_app(const PresetData* preset) {
    if (!preset) {
        return;
    }

    begin_dirty_suppression();
    g_app.current_preset = *preset;

    g_app.master_volume_ui = preset->master_volume;
    enqueue_param_float(PARAM_MASTER_VOLUME, preset->master_volume);

    g_app.filter_cutoff = preset->filter_cutoff;
    enqueue_param_float(PARAM_FILTER_CUTOFF, preset->filter_cutoff);

    g_app.filter_resonance = preset->filter_resonance;
    enqueue_param_float(PARAM_FILTER_RESONANCE, preset->filter_resonance);

    g_app.filter_mode = (FilterMode)preset->filter_mode;
    enqueue_param_int(PARAM_FILTER_MODE, preset->filter_mode);

    g_app.filter_env = preset->filter_env_amount;
    enqueue_param_float(PARAM_FILTER_ENV_AMOUNT, preset->filter_env_amount);

    g_app.env_attack = preset->env_attack;
    enqueue_param_float(PARAM_ENV_ATTACK, preset->env_attack);

    g_app.env_decay = preset->env_decay;
    enqueue_param_float(PARAM_ENV_DECAY, preset->env_decay);

    g_app.env_sustain = preset->env_sustain;
    enqueue_param_float(PARAM_ENV_SUSTAIN, preset->env_sustain);

    g_app.env_release = preset->env_release;
    enqueue_param_float(PARAM_ENV_RELEASE, preset->env_release);

    g_app.fx_ui.distortion.enabled = preset->distortion.enabled ? nk_true : nk_false;
    g_app.fx_ui.distortion.drive = preset->distortion.drive;
    g_app.fx_ui.distortion.mix = preset->distortion.mix;
    enqueue_param_bool(PARAM_FX_DISTORTION_ENABLED, preset->distortion.enabled);
    enqueue_param_float(PARAM_FX_DISTORTION_DRIVE, preset->distortion.drive);
    enqueue_param_float(PARAM_FX_DISTORTION_MIX, preset->distortion.mix);

    g_app.fx_ui.chorus.enabled = preset->chorus.enabled ? nk_true : nk_false;
    g_app.fx_ui.chorus.rate = preset->chorus.rate;
    g_app.fx_ui.chorus.depth = preset->chorus.depth;
    g_app.fx_ui.chorus.mix = preset->chorus.mix;
    enqueue_param_bool(PARAM_FX_CHORUS_ENABLED, preset->chorus.enabled);
    enqueue_param_float(PARAM_FX_CHORUS_RATE, preset->chorus.rate);
    enqueue_param_float(PARAM_FX_CHORUS_DEPTH, preset->chorus.depth);
    enqueue_param_float(PARAM_FX_CHORUS_MIX, preset->chorus.mix);

    g_app.fx_ui.delay.enabled = preset->delay.enabled ? nk_true : nk_false;
    g_app.fx_ui.delay.time = preset->delay.time;
    g_app.fx_ui.delay.feedback = preset->delay.feedback;
    g_app.fx_ui.delay.mix = preset->delay.mix;
    enqueue_param_bool(PARAM_FX_DELAY_ENABLED, preset->delay.enabled);
    enqueue_param_float(PARAM_FX_DELAY_TIME, preset->delay.time);
    enqueue_param_float(PARAM_FX_DELAY_FEEDBACK, preset->delay.feedback);
    enqueue_param_float(PARAM_FX_DELAY_MIX, preset->delay.mix);

    g_app.fx_ui.reverb.enabled = preset->reverb.enabled ? nk_true : nk_false;
    g_app.fx_ui.reverb.size = preset->reverb.size;
    g_app.fx_ui.reverb.damping = preset->reverb.damping;
    g_app.fx_ui.reverb.mix = preset->reverb.mix;
    enqueue_param_bool(PARAM_FX_REVERB_ENABLED, preset->reverb.enabled);
    enqueue_param_float(PARAM_FX_REVERB_SIZE, preset->reverb.size);
    enqueue_param_float(PARAM_FX_REVERB_DAMPING, preset->reverb.damping);
    enqueue_param_float(PARAM_FX_REVERB_MIX, preset->reverb.mix);

    g_app.fx_ui.compressor.enabled = preset->compressor.enabled ? nk_true : nk_false;
    g_app.fx_ui.compressor.threshold = preset->compressor.threshold;
    g_app.fx_ui.compressor.ratio = preset->compressor.ratio;
    enqueue_param_bool(PARAM_FX_COMP_ENABLED, preset->compressor.enabled);
    enqueue_param_float(PARAM_FX_COMP_THRESHOLD, preset->compressor.threshold);
    enqueue_param_float(PARAM_FX_COMP_RATIO, preset->compressor.ratio);

    g_app.arp_ui.enabled = preset->arp.enabled ? nk_true : nk_false;
    g_app.arp_ui.rate_index = arp_multiplier_index_for_value(preset->arp.rate_multiplier);
    g_app.arp.rate = arp_multiplier_value(g_app.arp_ui.rate_index);
    g_app.arp_ui.mode = preset->arp.mode;
    enqueue_param_bool(PARAM_ARP_ENABLED, preset->arp.enabled);
    enqueue_param_float(PARAM_ARP_RATE, preset->arp.rate_multiplier);
    enqueue_param_int(PARAM_ARP_MODE, preset->arp.mode);

    g_app.tempo_ui = preset->tempo;
    enqueue_param_float(PARAM_TEMPO, preset->tempo);

    end_dirty_suppression();
    g_app.preset_dirty = false;

    const char* preset_name = preset->meta.name[0] ? preset->meta.name : "(unnamed)";
    snprintf(g_app.preset_status, sizeof(g_app.preset_status), "✅ Applied preset %s", preset_name);
}

static void capture_project_from_app(ProjectData* project) {
    if (!project) {
        return;
    }

    project->tempo = g_app.tempo_ui;
    project->export_duration_seconds = g_app.export_duration_seconds;
    strncpy(project->preset_path, g_app.preset_path_input, sizeof(project->preset_path) - 1);
    project->preset_path[sizeof(project->preset_path) - 1] = '\0';
    strncpy(project->sample_path, g_app.sample_path_input, sizeof(project->sample_path) - 1);
    project->sample_path[sizeof(project->sample_path) - 1] = '\0';
    strncpy(project->export_path, g_app.export_path_input, sizeof(project->export_path) - 1);
    project->export_path[sizeof(project->export_path) - 1] = '\0';

    capture_preset_from_app(&project->preset);
}

static void apply_project_to_app(const ProjectData* project) {
    if (!project) {
        return;
    }

    begin_dirty_suppression();
    g_app.current_project = *project;
    g_app.current_preset = project->preset;

    strncpy(g_app.preset_path_input, project->preset_path, sizeof(g_app.preset_path_input) - 1);
    g_app.preset_path_input[sizeof(g_app.preset_path_input) - 1] = '\0';
    strncpy(g_app.sample_path_input, project->sample_path, sizeof(g_app.sample_path_input) - 1);
    g_app.sample_path_input[sizeof(g_app.sample_path_input) - 1] = '\0';
    strncpy(g_app.export_path_input, project->export_path, sizeof(g_app.export_path_input) - 1);
    g_app.export_path_input[sizeof(g_app.export_path_input) - 1] = '\0';

    g_app.export_duration_seconds = project->export_duration_seconds;
    g_app.tempo_ui = project->tempo;
    enqueue_param_float(PARAM_TEMPO, project->tempo);

    if (project->sample_path[0] != '\0') {
        load_sample_file(project->sample_path);
    }

    apply_preset_to_app(&project->preset);

    end_dirty_suppression();
    g_app.project_dirty = false;
    g_app.preset_dirty = false;

    const char* project_name = project->meta.name[0] ? project->meta.name : "(untitled project)";
    snprintf(g_app.project_status, sizeof(g_app.project_status), "✅ Applied project %s", project_name);
}

static void save_current_preset(const char* path, bool pretty) {
    if (!path || path[0] == '\0') {
        snprintf(g_app.preset_status, sizeof(g_app.preset_status), "❌ Provide a preset path");
        return;
    }

    capture_preset_from_app(&g_app.current_preset);
    if (!preset_save_file(&g_app.current_preset, path, pretty)) {
        snprintf(g_app.preset_status, sizeof(g_app.preset_status), "❌ Failed to save %s", path);
        return;
    }

    strncpy(g_app.preset_path_input, path, sizeof(g_app.preset_path_input) - 1);
    g_app.preset_path_input[sizeof(g_app.preset_path_input) - 1] = '\0';
    g_app.preset_dirty = false;
    snprintf(g_app.preset_status, sizeof(g_app.preset_status), "💾 Preset saved to %s", path);
}

static void load_preset_from_path(const char* path) {
    if (!path || path[0] == '\0') {
        snprintf(g_app.preset_status, sizeof(g_app.preset_status), "❌ Provide a preset path");
        return;
    }

    PresetData loaded;
    if (!preset_load_file(&loaded, path)) {
        snprintf(g_app.preset_status, sizeof(g_app.preset_status), "❌ Failed to load %s", path);
        return;
    }

    apply_preset_to_app(&loaded);
    strncpy(g_app.preset_path_input, path, sizeof(g_app.preset_path_input) - 1);
    g_app.preset_path_input[sizeof(g_app.preset_path_input) - 1] = '\0';
    g_app.current_project.preset = g_app.current_preset;
    mark_project_dirty();
}

static void save_current_project(const char* path, bool pretty) {
    if (!path || path[0] == '\0') {
        snprintf(g_app.project_status, sizeof(g_app.project_status), "❌ Provide a project path");
        return;
    }

    capture_project_from_app(&g_app.current_project);
    if (!project_save_file(&g_app.current_project, path, pretty)) {
        snprintf(g_app.project_status, sizeof(g_app.project_status), "❌ Failed to save %s", path);
        return;
    }

    strncpy(g_app.project_path_input, path, sizeof(g_app.project_path_input) - 1);
    g_app.project_path_input[sizeof(g_app.project_path_input) - 1] = '\0';
    g_app.project_dirty = false;
    snprintf(g_app.project_status, sizeof(g_app.project_status), "💾 Project saved to %s", path);
}

static void load_project_from_path(const char* path) {
    if (!path || path[0] == '\0') {
        snprintf(g_app.project_status, sizeof(g_app.project_status), "❌ Provide a project path");
        return;
    }

    ProjectData loaded;
    if (!project_load_file(&loaded, path)) {
        snprintf(g_app.project_status, sizeof(g_app.project_status), "❌ Failed to load %s", path);
        return;
    }

    apply_project_to_app(&loaded);
    strncpy(g_app.project_path_input, path, sizeof(g_app.project_path_input) - 1);
    g_app.project_path_input[sizeof(g_app.project_path_input) - 1] = '\0';
    snprintf(g_app.project_status, sizeof(g_app.project_status), "✅ Loaded project %s", path);
}

static inline void sample_slot_frame(const SampleBuffer* buffer, uint32_t index,
                                     float* out_left, float* out_right) {
    if (!buffer || !buffer->data || buffer->frame_count == 0) {
        *out_left = 0.0f;
        *out_right = 0.0f;
        return;
    }
    uint32_t channels = buffer->channels ? buffer->channels : 1;
    size_t base = (size_t)index * channels;
    if (channels == 1) {
        float v = buffer->data[base];
        *out_left = v;
        *out_right = v;
    } else {
        *out_left = buffer->data[base];
        *out_right = buffer->data[base + 1];
    }
}

static void sample_slot_mix(SampleSlot* slot, float sample_rate, float* left, float* right) {
    if (!slot || !slot->buffer.data || slot->buffer.frame_count == 0) {
        return;
    }

    if (atomic_exchange_explicit(&slot->trigger_pending, false, memory_order_acq_rel)) {
        slot->playhead = 0.0;
        slot->playback_step = slot->buffer.sample_rate > 0
                                   ? (double)slot->buffer.sample_rate / (double)sample_rate
                                   : 1.0;
        atomic_store_explicit(&slot->playing, true, memory_order_release);
    }

    if (atomic_exchange_explicit(&slot->stop_pending, false, memory_order_acq_rel)) {
        atomic_store_explicit(&slot->playing, false, memory_order_release);
    }

    if (!atomic_load_explicit(&slot->playing, memory_order_acquire)) {
        return;
    }

    const SampleBuffer* buffer = &slot->buffer;
    if (slot->playhead >= buffer->frame_count) {
        atomic_store_explicit(&slot->playing, false, memory_order_release);
        return;
    }

    double clamped = slot->playhead;
    if (clamped < 0.0) clamped = 0.0;
    double max_index = (double)(buffer->frame_count - 1);
    if (clamped > max_index) clamped = max_index;

    uint32_t index = (uint32_t)clamped;
    uint32_t next_index = (index + 1 < buffer->frame_count) ? (index + 1) : index;
    double frac = clamped - (double)index;

    float s0L, s0R, s1L, s1R;
    sample_slot_frame(buffer, index, &s0L, &s0R);
    sample_slot_frame(buffer, next_index, &s1L, &s1R);

    float mixed_left = s0L + (s1L - s0L) * (float)frac;
    float mixed_right = s0R + (s1R - s0R) * (float)frac;

    *left += mixed_left * slot->gain;
    *right += mixed_right * slot->gain;

    slot->playhead += slot->playback_step;
    if (slot->playhead >= buffer->frame_count) {
        atomic_store_explicit(&slot->playing, false, memory_order_release);
    }
}

static void sample_export_process_frame(SampleExportState* state, float left, float right) {
    if (!state) {
        return;
    }

    if (atomic_exchange_explicit(&state->arm_request, false, memory_order_acq_rel)) {
        state->frames_captured = 0;
        if (state->buffer && state->max_frames > 0) {
            atomic_store_explicit(&state->capture_failed, false, memory_order_release);
            atomic_store_explicit(&state->capture_active, true, memory_order_release);
        }
    }

    if (!atomic_load_explicit(&state->capture_active, memory_order_acquire)) {
        return;
    }

    if (!state->buffer || state->max_frames == 0) {
        atomic_store_explicit(&state->capture_active, false, memory_order_release);
        atomic_store_explicit(&state->capture_failed, true, memory_order_release);
        return;
    }

    if (state->frames_captured >= state->max_frames) {
        atomic_store_explicit(&state->capture_active, false, memory_order_release);
        atomic_store_explicit(&state->capture_complete, true, memory_order_release);
        return;
    }

    size_t base = (size_t)state->frames_captured * state->channels;
    state->buffer[base] = left;
    if (state->channels > 1) {
        state->buffer[base + 1] = right;
    }

    state->frames_captured++;
    if (state->frames_captured >= state->max_frames) {
        atomic_store_explicit(&state->capture_active, false, memory_order_release);
        atomic_store_explicit(&state->capture_complete, true, memory_order_release);
    }
}

static bool load_sample_file(const char* path) {
    SampleBuffer new_buffer;
    sample_buffer_init(&new_buffer);
    if (!sample_buffer_load_wav(&new_buffer, path)) {
        sample_buffer_free(&new_buffer);
        snprintf(g_app.sample_status, sizeof(g_app.sample_status),
                 "❌ Failed to load %s", path);
        return false;
    }

    sample_buffer_free(&g_app.sampler.buffer);
    g_app.sampler.buffer = new_buffer;
    g_app.sampler.playhead = 0.0;
    atomic_store(&g_app.sampler.playing, false);
    strncpy(g_app.sampler.last_loaded_path, path, sizeof(g_app.sampler.last_loaded_path) - 1);
    g_app.sampler.last_loaded_path[sizeof(g_app.sampler.last_loaded_path) - 1] = '\0';
    double duration = (double)g_app.sampler.buffer.frame_count /
                      (double)g_app.sampler.buffer.sample_rate;
    snprintf(g_app.sample_status, sizeof(g_app.sample_status),
             "✅ Loaded %.2fs sample (%u ch @ %u Hz)", duration,
             g_app.sampler.buffer.channels, g_app.sampler.buffer.sample_rate);
    return true;
}

static bool start_export_job(float seconds, const char* path) {
    if (!path || path[0] == '\0') {
        snprintf(g_app.export_status, sizeof(g_app.export_status),
                 "❌ Provide an export path");
        return false;
    }

    if (seconds < 0.1f) seconds = 0.1f;
    if (seconds > 120.0f) seconds = 120.0f;

    SampleExportState* state = &g_app.export_state;
    if (atomic_load(&state->capture_active) ||
        atomic_load(&state->arm_request)) {
        snprintf(g_app.export_status, sizeof(g_app.export_status),
                 "⚠️ Export already in progress");
        return false;
    }

    uint32_t sample_rate = (uint32_t)g_app.synth.sample_rate;
    uint64_t frames64 = (uint64_t)(seconds * (float)sample_rate);
    if (frames64 == 0) {
        frames64 = sample_rate;
    }
    if (frames64 > UINT32_MAX) {
        frames64 = UINT32_MAX;
    }
    uint32_t frames = (uint32_t)frames64;
    uint32_t channels = 2;

    size_t total_samples = (size_t)frames * channels;
    float* buffer = (float*)malloc(total_samples * sizeof(float));
    if (!buffer) {
        snprintf(g_app.export_status, sizeof(g_app.export_status),
                 "❌ Not enough memory for %.1fs export", seconds);
        return false;
    }

    if (state->buffer) {
        free(state->buffer);
    }

    state->buffer = buffer;
    state->max_frames = frames;
    state->frames_captured = 0;
    state->channels = channels;
    state->sample_rate = sample_rate;
    state->duration_seconds = seconds;
    strncpy(state->output_path, path, sizeof(state->output_path) - 1);
    state->output_path[sizeof(state->output_path) - 1] = '\0';

    atomic_store(&state->capture_complete, false);
    atomic_store(&state->capture_failed, false);
    atomic_store(&state->capture_active, false);
    atomic_store(&state->arm_request, true);

    snprintf(g_app.export_status, sizeof(g_app.export_status),
             "⏺ Recording %.1fs to %s", seconds, state->output_path);
    return true;
}

static void poll_export_job(void) {
    SampleExportState* state = &g_app.export_state;
    if (atomic_exchange(&state->capture_complete, false)) {
        bool ok = sample_buffer_write_wav(state->output_path, state->buffer,
                                          state->frames_captured,
                                          state->channels,
                                          state->sample_rate);
        if (ok) {
            snprintf(g_app.export_status, sizeof(g_app.export_status),
                     "✅ Exported %.1fs (%u frames) to %s", state->duration_seconds,
                     state->frames_captured, state->output_path);
        } else {
            snprintf(g_app.export_status, sizeof(g_app.export_status),
                     "❌ Failed writing %s", state->output_path);
        }
        free(state->buffer);
        state->buffer = NULL;
        state->max_frames = 0;
    }

    if (atomic_exchange(&state->capture_failed, false)) {
        snprintf(g_app.export_status, sizeof(g_app.export_status),
                 "❌ Export failed (buffer overflow)");
        free(state->buffer);
        state->buffer = NULL;
        state->max_frames = 0;
    }
}

static void ui_apply_theme(struct nk_context* ctx) {
    if (!ctx) return;
    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(236, 239, 244, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(18, 22, 33, 255);
    table[NK_COLOR_HEADER] = nk_rgba(34, 38, 52, 255);
    table[NK_COLOR_BORDER] = nk_rgba(60, 66, 92, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(70, 78, 104, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(92, 101, 132, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(64, 73, 96, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(82, 92, 120, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_SELECT] = nk_rgba(64, 73, 96, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(46, 52, 70, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(248, 190, 120, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(255, 208, 120, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(30, 34, 48, 255);
    table[NK_COLOR_EDIT] = nk_rgba(30, 34, 48, 255);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(245, 245, 245, 255);
    table[NK_COLOR_COMBO] = nk_rgba(30, 34, 48, 255);
    table[NK_COLOR_CHART] = nk_rgba(64, 73, 96, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(250, 214, 165, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(24, 28, 40, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 73, 96, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(90, 101, 132, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(235, 151, 78, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(34, 38, 52, 255);
    nk_style_from_table(ctx, table);
    ctx->style.window.padding = nk_vec2(16.0f, 16.0f);
    ctx->style.window.spacing = nk_vec2(12.0f, 12.0f);
    ctx->style.window.border = 2.0f;
    ctx->style.button.padding = nk_vec2(12.0f, 8.0f);
    ctx->style.button.rounding = 4.0f;
}

static void draw_status_text(struct nk_context* ctx, const char* label, const char* value) {
    nk_layout_row_dynamic(ctx, 18, 2);
    nk_label(ctx, label, NK_TEXT_LEFT);
    nk_label(ctx, value ? value : "", NK_TEXT_RIGHT);
}

static bool draw_knob(struct nk_context* ctx, const char* label, float* value,
                      float min, float max, float step);
static bool draw_knob(struct nk_context* ctx, const char* label, float* value,
                      float min, float max, float step) {
    if (!ctx || !value) {
        return false;
    }

    struct nk_rect bounds = nk_widget_bounds(ctx);
    enum nk_widget_layout_states state = nk_widget(&bounds, ctx);
    if (state == NK_WIDGET_INVALID) {
        return false;
    }

    struct nk_command_buffer* out = nk_window_get_canvas(ctx);
    struct nk_input* in = &ctx->input;
    bool hover = nk_input_is_mouse_hovering_rect(in, bounds);
    bool active = hover && nk_input_is_mouse_down(in, NK_BUTTON_LEFT);
    bool changed = false;
    float range = max - min;
    if (range <= 0.0f) {
        range = 1.0f;
    }

    if (active) {
        float delta = -in->mouse.delta.y * step * 0.5f;
        if (fabsf(delta) > 0.0f) {
            float new_val = clampf(*value + delta, min, max);
            if (fabsf(new_val - *value) > 0.0001f) {
                *value = new_val;
                changed = true;
            }
        }
    }

    if (hover && fabsf(in->mouse.scroll_delta.y) > 0.0f) {
        float delta = in->mouse.scroll_delta.y * step;
        float new_val = clampf(*value + delta, min, max);
        if (fabsf(new_val - *value) > 0.0001f) {
            *value = new_val;
            changed = true;
        }
        in->mouse.scroll_delta.y = 0.0f;
    }

    float cx = bounds.x + bounds.w * 0.5f;
    float cy = bounds.y + bounds.h * 0.5f - 8.0f;
    float radius = fminf(bounds.w, bounds.h - 18.0f) * 0.5f;
    if (radius < 10.0f) radius = 10.0f;

    struct nk_color face = hover ? nk_rgb(86, 94, 128) : nk_rgb(64, 72, 104);
    struct nk_color edge = nk_rgb(18, 22, 33);
    struct nk_color indicator = nk_rgb(235, 151, 78);

    nk_fill_circle(out, nk_rect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f), face);
    nk_stroke_circle(out, nk_rect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f), 2.0f, edge);

    float t = (*value - min) / range;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float angle = (-140.0f + 280.0f * t) * (float)M_PI / 180.0f;
    float x2 = cx + cosf(angle) * radius * 0.8f;
    float y2 = cy + sinf(angle) * radius * 0.8f;
    nk_stroke_line(out, cx, cy, x2, y2, 3.0f, indicator);

    char value_buf[32];
    snprintf(value_buf, sizeof(value_buf), "%.2f", *value);
    struct nk_color text_color = ctx->style.text.color;
    struct nk_rect value_rect = nk_rect(bounds.x, bounds.y + 4.0f, bounds.w, 16.0f);
    nk_draw_text(out, value_rect, value_buf, (int)strlen(value_buf), ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);

    struct nk_rect label_rect = nk_rect(bounds.x, bounds.y + bounds.h - 16.0f, bounds.w, 16.0f);
    nk_draw_text(out, label_rect, label, (int)strlen(label), ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);

    return changed;
}
static void draw_keyboard_panel(struct nk_context* ctx);

static void draw_distortion_card(struct nk_context* ctx);
static void draw_chorus_card(struct nk_context* ctx);
static void draw_delay_card(struct nk_context* ctx);
static void draw_reverb_card(struct nk_context* ctx);
static void draw_compressor_card(struct nk_context* ctx);
static void draw_synth_panel(struct nk_context* ctx);
static void draw_fx_panel(struct nk_context* ctx);
static void draw_loop_panel(struct nk_context* ctx);
static void draw_drums_panel(struct nk_context* ctx);
static void draw_sample_loader_panel(struct nk_context* ctx);
static void draw_export_panel(struct nk_context* ctx);
static void draw_performance_panel(struct nk_context* ctx);
static void draw_preset_panel(struct nk_context* ctx);
static void draw_project_panel(struct nk_context* ctx);

static void capture_preset_from_app(PresetData* preset);
static void apply_preset_to_app(const PresetData* preset);
static void capture_project_from_app(ProjectData* project);
static void apply_project_to_app(const ProjectData* project);
static void save_current_preset(const char* path, bool pretty);
static void load_preset_from_path(const char* path);
static void save_current_project(const char* path, bool pretty);
static void load_project_from_path(const char* path);
static bool load_sample_file(const char* path);
static bool start_export_job(float seconds, const char* path);
static void poll_export_job(void);

static void draw_keyboard_panel(struct nk_context* ctx) {
    KeyboardUIState* kb = &g_app.keyboard_ui;
    if (!kb) {
        return;
    }

    int max_base = 127 - kb->num_keys;
    if (max_base < 0) max_base = 0;
    if (kb->base_note < 0) kb->base_note = 0;
    if (kb->base_note > max_base) kb->base_note = max_base;

    nk_flags panel_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
    if (!nk_group_begin_titled(ctx, "GUI_KEYBOARD", "Performance Keyboard", panel_flags)) {
        return;
    }

    nk_layout_row_template_begin(ctx, 32.0f);
    nk_layout_row_template_push_static(ctx, 110.0f);
    nk_layout_row_template_push_variable(ctx, 1.0f);
    nk_layout_row_template_push_static(ctx, 110.0f);
    nk_layout_row_template_end(ctx);

    if (nk_button_label(ctx, "Octave -")) {
        kb->base_note = (kb->base_note - 12 < 0) ? 0 : kb->base_note - 12;
    }

    char start_label[8];
    char end_label[8];
    char range_text[64];
    midi_note_label(kb->base_note, start_label, sizeof(start_label));
    int end_note = kb->base_note + kb->num_keys - 1;
    if (end_note > 127) end_note = 127;
    midi_note_label(end_note, end_label, sizeof(end_label));
    snprintf(range_text, sizeof(range_text), "Range: %s – %s", start_label, end_label);
    nk_label(ctx, range_text, NK_TEXT_CENTERED);

    if (nk_button_label(ctx, "Octave +")) {
        int proposed = kb->base_note + 12;
        kb->base_note = (proposed > max_base) ? max_base : proposed;
    }

    nk_layout_row_dynamic(ctx, 20, 2);
    nk_label(ctx, "Click & drag across keys to glide notes.", NK_TEXT_LEFT);
    nk_label(ctx, "Shift octaves to reach bass or lead ranges.", NK_TEXT_RIGHT);

    nk_layout_row_dynamic(ctx, 140, 1);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    enum nk_widget_layout_states state = nk_widget(&bounds, ctx);
    if (state != NK_WIDGET_INVALID) {
        const float padding = 8.0f;
        struct nk_rect inner = nk_rect(bounds.x + padding, bounds.y + padding,
                                       bounds.w - padding * 2.0f, bounds.h - padding * 2.0f);
        if (inner.w < 10.0f || inner.h < 10.0f) {
            nk_group_end(ctx);
            return;
        }

        struct nk_command_buffer* out = nk_window_get_canvas(ctx);
        nk_fill_rect(out, bounds, 8.0f, nk_rgb(14, 18, 30));

        int start_note = kb->base_note;
        int stop_note = kb->base_note + kb->num_keys;
        if (stop_note > 128) stop_note = 128;

        int white_count = 0;
        for (int note = start_note; note < stop_note; note++) {
            if (!is_black_key(note)) {
                white_count++;
            }
        }
        if (white_count <= 0) white_count = 1;

        float white_width = inner.w / (float)white_count;
        float white_height = inner.h;
        float black_width = white_width * 0.6f;
        if (black_width < 9.0f) black_width = 9.0f;
        float black_height = inner.h * 0.62f;

        struct KeyVisual {
            struct nk_rect rect;
            bool visible;
            bool black;
        } key_visual[128] = {0};

        float cursor_x = inner.x;
        for (int note = start_note; note < stop_note; note++) {
            if (is_black_key(note)) continue;
            struct nk_rect key_rect = nk_rect(cursor_x, inner.y, white_width - 1.0f, white_height);
            key_visual[note].rect = key_rect;
            key_visual[note].visible = true;
            key_visual[note].black = false;
            cursor_x += white_width;
        }

        for (int note = start_note; note < stop_note; note++) {
            if (!is_black_key(note)) continue;
            int prev = note - 1;
            while (prev >= start_note && is_black_key(prev)) prev--;
            int next = note + 1;
            while (next < stop_note && is_black_key(next)) next++;
            if (prev < start_note || next >= stop_note) continue;
            if (!key_visual[prev].visible || !key_visual[next].visible) continue;
            float left = key_visual[prev].rect.x + key_visual[prev].rect.w;
            float right = key_visual[next].rect.x;
            float center = (left + right) * 0.5f;
            struct nk_rect key_rect = nk_rect(center - black_width * 0.5f, inner.y,
                                              black_width, black_height);
            key_visual[note].rect = key_rect;
            key_visual[note].visible = true;
            key_visual[note].black = true;
        }

        int hovered_note = -1;
        struct nk_input* in = &ctx->input;
        if (nk_input_is_mouse_hovering_rect(in, bounds)) {
            for (int note = start_note; note < stop_note; note++) {
                if (key_visual[note].visible && key_visual[note].black &&
                    nk_input_is_mouse_hovering_rect(in, key_visual[note].rect)) {
                    hovered_note = note;
                    break;
                }
            }
            if (hovered_note == -1) {
                for (int note = start_note; note < stop_note; note++) {
                    if (key_visual[note].visible && !key_visual[note].black &&
                        nk_input_is_mouse_hovering_rect(in, key_visual[note].rect)) {
                        hovered_note = note;
                        break;
                    }
                }
            }
        }

        for (int note = start_note; note < stop_note; note++) {
            if (!key_visual[note].visible || key_visual[note].black) continue;
            bool active = kb->note_active[note];
            bool hovered = (hovered_note == note);
            struct nk_color fill = active ? nk_rgb(255, 196, 92) : nk_rgb(238, 241, 247);
            if (hovered && !active) {
                fill = nk_rgb(250, 230, 180);
            }
            struct nk_color border = nk_rgb(48, 58, 82);
            nk_fill_rect(out, key_visual[note].rect, 4.0f, fill);
            nk_stroke_rect(out, key_visual[note].rect, 4.0f, 1.5f, border);

            char label[8];
            midi_note_label(note, label, sizeof(label));
            struct nk_rect text_rect = nk_rect(key_visual[note].rect.x + 4.0f,
                                               key_visual[note].rect.y + key_visual[note].rect.h - 18.0f,
                                               key_visual[note].rect.w - 8.0f, 14.0f);
            nk_draw_text(out, text_rect, label, (int)strlen(label), ctx->style.font,
                         nk_rgba(0, 0, 0, 0), nk_rgb(22, 30, 44));
        }

        for (int note = start_note; note < stop_note; note++) {
            if (!key_visual[note].visible || !key_visual[note].black) continue;
            bool active = kb->note_active[note];
            bool hovered = (hovered_note == note);
            struct nk_color fill = active ? nk_rgb(255, 170, 60) : nk_rgb(30, 34, 47);
            if (hovered && !active) {
                fill = nk_rgb(80, 90, 120);
            }
            nk_fill_rect(out, key_visual[note].rect, 4.0f, fill);
            nk_stroke_rect(out, key_visual[note].rect, 4.0f, 1.5f, nk_rgb(10, 12, 20));
        }

        bool mouse_down = nk_input_is_mouse_down(in, NK_BUTTON_LEFT);
        if (!mouse_down && kb->mouse_dragging) {
            if (kb->mouse_active_note >= 0) {
                ui_note_off((uint8_t)kb->mouse_active_note);
            }
            kb->mouse_active_note = -1;
            kb->mouse_dragging = false;
        } else if (mouse_down) {
            if (!kb->mouse_dragging && hovered_note >= 0) {
                ui_note_on((uint8_t)hovered_note);
                kb->mouse_active_note = hovered_note;
                kb->mouse_dragging = true;
            } else if (kb->mouse_dragging && hovered_note >= 0 && hovered_note != kb->mouse_active_note) {
                if (kb->mouse_active_note >= 0) {
                    ui_note_off((uint8_t)kb->mouse_active_note);
                }
                ui_note_on((uint8_t)hovered_note);
                kb->mouse_active_note = hovered_note;
            }
        }
    }

    nk_group_end(ctx);
}

static void draw_distortion_card(struct nk_context* ctx) {
    if (nk_group_begin_titled(ctx, "FX_DISTORTION", "Distortion", NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_bool prev_enabled = g_app.fx_ui.distortion.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.fx_ui.distortion.enabled);
        if (prev_enabled != g_app.fx_ui.distortion.enabled) {
            enqueue_param_bool_user(PARAM_FX_DISTORTION_ENABLED, g_app.fx_ui.distortion.enabled);
        }
        if (g_app.fx_ui.distortion.enabled) {
            nk_layout_row_dynamic(ctx, 120, 2);
            float prev_drive = g_app.fx_ui.distortion.drive;
            float prev_mix = g_app.fx_ui.distortion.mix;
            if (draw_knob(ctx, "Drive", &g_app.fx_ui.distortion.drive, 1.0f, 20.0f, 0.1f) &&
                fabsf(g_app.fx_ui.distortion.drive - prev_drive) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_DISTORTION_DRIVE, g_app.fx_ui.distortion.drive);
            }
            if (draw_knob(ctx, "Mix", &g_app.fx_ui.distortion.mix, 0.0f, 1.0f, 0.01f) &&
                fabsf(g_app.fx_ui.distortion.mix - prev_mix) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_DISTORTION_MIX, g_app.fx_ui.distortion.mix);
            }
        }
        nk_group_end(ctx);
    }
}

static void draw_chorus_card(struct nk_context* ctx) {
    if (nk_group_begin_titled(ctx, "FX_CHORUS", "Chorus", NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_bool prev_enabled = g_app.fx_ui.chorus.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.fx_ui.chorus.enabled);
        if (prev_enabled != g_app.fx_ui.chorus.enabled) {
            enqueue_param_bool_user(PARAM_FX_CHORUS_ENABLED, g_app.fx_ui.chorus.enabled);
        }
        if (g_app.fx_ui.chorus.enabled) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Rate (Hz)", NK_TEXT_LEFT);
            float prev_rate = g_app.fx_ui.chorus.rate;
            nk_slider_float(ctx, 0.1f, &g_app.fx_ui.chorus.rate, 5.0f, 0.1f);
            if (fabsf(g_app.fx_ui.chorus.rate - prev_rate) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_CHORUS_RATE, g_app.fx_ui.chorus.rate);
            }
            nk_label(ctx, "Depth", NK_TEXT_LEFT);
            float prev_depth = g_app.fx_ui.chorus.depth;
            nk_slider_float(ctx, 1.0f, &g_app.fx_ui.chorus.depth, 50.0f, 1.0f);
            if (fabsf(g_app.fx_ui.chorus.depth - prev_depth) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_CHORUS_DEPTH, g_app.fx_ui.chorus.depth);
            }
            nk_label(ctx, "Mix", NK_TEXT_LEFT);
            float prev_mix = g_app.fx_ui.chorus.mix;
            nk_slider_float(ctx, 0.0f, &g_app.fx_ui.chorus.mix, 1.0f, 0.01f);
            if (fabsf(g_app.fx_ui.chorus.mix - prev_mix) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_CHORUS_MIX, g_app.fx_ui.chorus.mix);
            }
        }
        nk_group_end(ctx);
    }
}

static void draw_delay_card(struct nk_context* ctx) {
    if (nk_group_begin_titled(ctx, "FX_DELAY", "Delay", NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_bool prev_enabled = g_app.fx_ui.delay.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.fx_ui.delay.enabled);
        if (prev_enabled != g_app.fx_ui.delay.enabled) {
            enqueue_param_bool_user(PARAM_FX_DELAY_ENABLED, g_app.fx_ui.delay.enabled);
        }
        if (g_app.fx_ui.delay.enabled) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Time (s)", NK_TEXT_LEFT);
            float prev_time = g_app.fx_ui.delay.time;
            nk_slider_float(ctx, 0.05f, &g_app.fx_ui.delay.time, 2.0f, 0.01f);
            if (fabsf(g_app.fx_ui.delay.time - prev_time) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_DELAY_TIME, g_app.fx_ui.delay.time);
            }
            nk_label(ctx, "Feedback", NK_TEXT_LEFT);
            float prev_feedback = g_app.fx_ui.delay.feedback;
            nk_slider_float(ctx, 0.0f, &g_app.fx_ui.delay.feedback, 0.95f, 0.01f);
            if (fabsf(g_app.fx_ui.delay.feedback - prev_feedback) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_DELAY_FEEDBACK, g_app.fx_ui.delay.feedback);
            }
            nk_label(ctx, "Mix", NK_TEXT_LEFT);
            float prev_mix = g_app.fx_ui.delay.mix;
            nk_slider_float(ctx, 0.0f, &g_app.fx_ui.delay.mix, 1.0f, 0.01f);
            if (fabsf(g_app.fx_ui.delay.mix - prev_mix) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_DELAY_MIX, g_app.fx_ui.delay.mix);
            }
        }
        nk_group_end(ctx);
    }
}

static void draw_reverb_card(struct nk_context* ctx) {
    if (nk_group_begin_titled(ctx, "FX_REVERB", "Reverb", NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_bool prev_enabled = g_app.fx_ui.reverb.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.fx_ui.reverb.enabled);
        if (prev_enabled != g_app.fx_ui.reverb.enabled) {
            enqueue_param_bool_user(PARAM_FX_REVERB_ENABLED, g_app.fx_ui.reverb.enabled);
        }
        if (g_app.fx_ui.reverb.enabled) {
            nk_layout_row_dynamic(ctx, 120, 3);
            float prev_size = g_app.fx_ui.reverb.size;
            float prev_damping = g_app.fx_ui.reverb.damping;
            float prev_mix = g_app.fx_ui.reverb.mix;
            if (draw_knob(ctx, "Size", &g_app.fx_ui.reverb.size, 0.1f, 0.95f, 0.01f) &&
                fabsf(g_app.fx_ui.reverb.size - prev_size) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_REVERB_SIZE, g_app.fx_ui.reverb.size);
            }
            if (draw_knob(ctx, "Damping", &g_app.fx_ui.reverb.damping, 0.0f, 0.99f, 0.01f) &&
                fabsf(g_app.fx_ui.reverb.damping - prev_damping) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_REVERB_DAMPING, g_app.fx_ui.reverb.damping);
            }
            if (draw_knob(ctx, "Mix", &g_app.fx_ui.reverb.mix, 0.0f, 1.0f, 0.01f) &&
                fabsf(g_app.fx_ui.reverb.mix - prev_mix) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_REVERB_MIX, g_app.fx_ui.reverb.mix);
            }
        }
        nk_group_end(ctx);
    }
}

static void draw_compressor_card(struct nk_context* ctx) {
    if (nk_group_begin_titled(ctx, "FX_COMP", "Compressor", NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_bool prev_enabled = g_app.fx_ui.compressor.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.fx_ui.compressor.enabled);
        if (prev_enabled != g_app.fx_ui.compressor.enabled) {
            enqueue_param_bool_user(PARAM_FX_COMP_ENABLED, g_app.fx_ui.compressor.enabled);
        }
        if (g_app.fx_ui.compressor.enabled) {
            nk_layout_row_dynamic(ctx, 120, 2);
            float prev_threshold = g_app.fx_ui.compressor.threshold;
            float prev_ratio = g_app.fx_ui.compressor.ratio;
            if (draw_knob(ctx, "Threshold", &g_app.fx_ui.compressor.threshold, 0.0f, 1.0f, 0.01f) &&
                fabsf(g_app.fx_ui.compressor.threshold - prev_threshold) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_COMP_THRESHOLD, g_app.fx_ui.compressor.threshold);
            }
            if (draw_knob(ctx, "Ratio", &g_app.fx_ui.compressor.ratio, 1.0f, 20.0f, 0.1f) &&
                fabsf(g_app.fx_ui.compressor.ratio - prev_ratio) > 0.0001f) {
                enqueue_param_float_user(PARAM_FX_COMP_RATIO, g_app.fx_ui.compressor.ratio);
            }
        }
        nk_group_end(ctx);
    }
}

static void draw_synth_panel(struct nk_context* ctx) {
    nk_flags panel_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

    nk_layout_row_dynamic(ctx, 150, 2);
    if (nk_group_begin_titled(ctx, "SYNTH_TONE", "Tone & Filter", panel_flags)) {
        nk_layout_row_dynamic(ctx, 110, 4);
        float prev_master = g_app.master_volume_ui;
        float prev_cutoff = g_app.filter_cutoff;
        float prev_res = g_app.filter_resonance;
        float prev_env_amt = g_app.filter_env;

        if (draw_knob(ctx, "Master", &g_app.master_volume_ui, 0.0f, 1.0f, 0.01f) &&
            fabsf(g_app.master_volume_ui - prev_master) > 0.0001f) {
            enqueue_param_float_user(PARAM_MASTER_VOLUME, g_app.master_volume_ui);
        }
        if (draw_knob(ctx, "Cutoff", &g_app.filter_cutoff, 20.0f, 20000.0f, 50.0f) &&
            fabsf(g_app.filter_cutoff - prev_cutoff) > 1.0f) {
            enqueue_param_float_user(PARAM_FILTER_CUTOFF, g_app.filter_cutoff);
        }
        if (draw_knob(ctx, "Resonance", &g_app.filter_resonance, 0.0f, 1.0f, 0.01f) &&
            fabsf(g_app.filter_resonance - prev_res) > 0.0001f) {
            enqueue_param_float_user(PARAM_FILTER_RESONANCE, g_app.filter_resonance);
        }
        if (draw_knob(ctx, "Env Amt", &g_app.filter_env, -1.0f, 1.0f, 0.01f) &&
            fabsf(g_app.filter_env - prev_env_amt) > 0.0001f) {
            enqueue_param_float_user(PARAM_FILTER_ENV_AMOUNT, g_app.filter_env);
        }

        const char* filter_modes[] = {"LP", "HP", "BP", "Notch", "Allpass"};
        nk_layout_row_dynamic(ctx, 26, 2);
        nk_label(ctx, "Filter Mode", NK_TEXT_LEFT);
        int prev_filter_mode = (int)g_app.filter_mode;
        int selected_mode = nk_combo(ctx, filter_modes, 5, prev_filter_mode, 24, nk_vec2(180, 140));
        if (selected_mode != prev_filter_mode) {
            g_app.filter_mode = (FilterMode)selected_mode;
            enqueue_param_int_user(PARAM_FILTER_MODE, selected_mode);
        }
        nk_group_end(ctx);
    }

    if (nk_group_begin_titled(ctx, "SYNTH_ENV", "Envelope & Dynamics", panel_flags)) {
        nk_layout_row_dynamic(ctx, 110, 4);
        float prev_attack = g_app.env_attack;
        float prev_decay = g_app.env_decay;
        float prev_sustain = g_app.env_sustain;
        float prev_release = g_app.env_release;

        if (draw_knob(ctx, "Attack", &g_app.env_attack, 0.001f, 2.0f, 0.005f) &&
            fabsf(g_app.env_attack - prev_attack) > 0.0001f) {
            enqueue_param_float_user(PARAM_ENV_ATTACK, g_app.env_attack);
        }
        if (draw_knob(ctx, "Decay", &g_app.env_decay, 0.001f, 2.0f, 0.005f) &&
            fabsf(g_app.env_decay - prev_decay) > 0.0001f) {
            enqueue_param_float_user(PARAM_ENV_DECAY, g_app.env_decay);
        }
        if (draw_knob(ctx, "Sustain", &g_app.env_sustain, 0.0f, 1.0f, 0.01f) &&
            fabsf(g_app.env_sustain - prev_sustain) > 0.0001f) {
            enqueue_param_float_user(PARAM_ENV_SUSTAIN, g_app.env_sustain);
        }
        if (draw_knob(ctx, "Release", &g_app.env_release, 0.001f, 5.0f, 0.01f) &&
            fabsf(g_app.env_release - prev_release) > 0.0001f) {
            enqueue_param_float_user(PARAM_ENV_RELEASE, g_app.env_release);
        }

        nk_layout_row_dynamic(ctx, 26, 1);
        char voice_text[64];
        snprintf(voice_text, sizeof(voice_text), "Active Voices: %d / %d", g_app.synth.num_active_voices, MAX_VOICES);
        nk_label(ctx, voice_text, NK_TEXT_LEFT);
        nk_size voice_bar = (nk_size)g_app.synth.num_active_voices;
        nk_progress(ctx, &voice_bar, MAX_VOICES, NK_FIXED);
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 110, 1);
    if (nk_group_begin_titled(ctx, "SYNTH_ARP", "Arpeggiator", panel_flags)) {
        nk_layout_row_dynamic(ctx, 26, 2);
        nk_bool prev_arp_enabled = g_app.arp_ui.enabled;
        nk_checkbox_label(ctx, "Enabled", &g_app.arp_ui.enabled);
        if (prev_arp_enabled != g_app.arp_ui.enabled) {
            enqueue_param_bool_user(PARAM_ARP_ENABLED, g_app.arp_ui.enabled);
        }

        nk_layout_row_dynamic(ctx, 24, 1);
        nk_label(ctx, "Tempo Multiplier", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 32, k_arp_rate_multiplier_count);
        for (int i = 0; i < k_arp_rate_multiplier_count; i++) {
            struct nk_style_button style = ctx->style.button;
            if (i == g_app.arp_ui.rate_index) {
                style.normal.data.color = nk_rgb(70, 110, 190);
                style.hover.data.color = nk_rgb(90, 140, 220);
                style.text_normal = nk_rgb(255, 255, 255);
            }
            float multiplier = arp_multiplier_value(i);
            char label[16];
            if (fabsf(multiplier - roundf(multiplier)) < 0.05f) {
                snprintf(label, sizeof(label), "%.0fx", multiplier);
            } else {
                snprintf(label, sizeof(label), "%.1fx", multiplier);
            }
            if (nk_button_label_styled(ctx, &style, label) && g_app.arp_ui.rate_index != i) {
                g_app.arp_ui.rate_index = i;
                enqueue_param_float_user(PARAM_ARP_RATE, multiplier);
            }
        }

        const char* arp_modes[] = {"Off", "Up", "Down", "Up-Down", "Random"};
        nk_layout_row_dynamic(ctx, 26, 2);
        nk_label(ctx, "Mode", NK_TEXT_LEFT);
        int prev_arp_mode = g_app.arp_ui.mode;
        g_app.arp_ui.mode = nk_combo(ctx, arp_modes, 5, g_app.arp_ui.mode, 24, nk_vec2(200, 140));
        if (g_app.arp_ui.mode != prev_arp_mode) {
            enqueue_param_int_user(PARAM_ARP_MODE, g_app.arp_ui.mode);
        }
        nk_group_end(ctx);
    }
}

static void draw_fx_panel(struct nk_context* ctx) {
    nk_layout_row_dynamic(ctx, 150, 3);
    draw_distortion_card(ctx);
    draw_chorus_card(ctx);
    draw_delay_card(ctx);

    nk_layout_row_dynamic(ctx, 150, 2);
    draw_reverb_card(ctx);
    draw_compressor_card(ctx);
}

static void draw_loop_panel(struct nk_context* ctx) {
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

    nk_layout_row_dynamic(ctx, 130, 1);
    if (nk_group_begin_titled(ctx, "LOOP_CTRL", "Loop Controls", flags)) {
        nk_layout_row_dynamic(ctx, 28, 4);
        if (nk_button_label(ctx, g_app.loop_recorder.playing ? "⏸ PLAY" : "▶ PLAY")) {
            if (g_app.loop_recorder.playing) {
                loop_recorder_stop_playback(&g_app.loop_recorder, &g_app.synth);
            } else {
                loop_recorder_start_playback(&g_app.loop_recorder);
            }
        }

        struct nk_style_button rec_style = ctx->style.button;
        if (g_app.loop_recorder.recording) {
            rec_style.normal.data.color = nk_rgb(200, 40, 40);
            rec_style.hover.data.color = nk_rgb(230, 70, 70);
        }
        if (nk_button_label_styled(ctx, &rec_style, g_app.loop_recorder.recording ? "🔴 REC" : "⚫ REC")) {
            if (g_app.loop_recorder.recording) {
                loop_recorder_stop_recording(&g_app.loop_recorder, &g_app.synth);
            } else {
                loop_recorder_start_recording(&g_app.loop_recorder);
            }
        }

        if (nk_button_label(ctx, "🗑 CLEAR")) {
            loop_recorder_clear(&g_app.loop_recorder, &g_app.synth);
        }

        if (nk_button_label(ctx, "PANIC")) {
            enqueue_param_bool(PARAM_PANIC, true);
        }

        Loop* loop = &g_app.loop_recorder.loops[g_app.loop_recorder.current_loop];
        nk_layout_row_dynamic(ctx, 18, 2);
        char info[64];
        snprintf(info, sizeof(info), "%s", loop->name);
        nk_label(ctx, "Loop", NK_TEXT_LEFT);
        nk_label(ctx, info, NK_TEXT_RIGHT);

        snprintf(info, sizeof(info), "%d events", loop->num_events);
        nk_label(ctx, "Events", NK_TEXT_LEFT);
        nk_label(ctx, info, NK_TEXT_RIGHT);

        snprintf(info, sizeof(info), "%.2fs", loop->loop_length);
        nk_label(ctx, "Length", NK_TEXT_LEFT);
        nk_label(ctx, info, NK_TEXT_RIGHT);

        double normalized = (loop->loop_length > 0.0) ? g_app.loop_recorder.loop_position / loop->loop_length : 0.0;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;
        nk_size progress = (nk_size)(normalized * 1000.0);
        nk_layout_row_dynamic(ctx, 14, 1);
        nk_progress(ctx, &progress, 1000, NK_FIXED);

        nk_layout_row_dynamic(ctx, 32, 1);
        if (g_app.loop_recorder.recording) {
            nk_label_wrap(ctx, "Recording: play the keyboard to capture a loop.");
        } else if (g_app.loop_recorder.playing) {
            nk_label_wrap(ctx, "Playing back your captured notes.");
        } else {
            nk_label_wrap(ctx, "Tip: Hit REC, play a chord progression, then press PLAY.");
        }
        nk_group_end(ctx);
    }
}

static void draw_drums_panel(struct nk_context* ctx) {
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
    nk_layout_row_dynamic(ctx, 40, 3);
    if (nk_button_label(ctx, g_app.drums.playing ? "⏸ DRUMS" : "▶ DRUMS")) {
        g_app.drums.playing = !g_app.drums.playing;
    }
    char step_info[64];
    snprintf(step_info, sizeof(step_info), "Step %d / %d", g_app.drums.current_step + 1, DRUM_STEPS);
    nk_label(ctx, step_info, NK_TEXT_CENTERED);
    nk_label(ctx, "Toggle steps to build your beat", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 200, 1);
    if (nk_group_begin_titled(ctx, "DRUM_GRID", "Step Grid", flags)) {
        const char* drum_names[] = {"KICK", "SNARE", "CL-HH", "OP-HH", "CLAP", "TOM-H", "TOM-M", "TOM-L"};
        DrumPattern* dpat = &g_app.drums.patterns[g_app.drums.current_pattern];
        int current_step = g_app.drums.current_step;
        for (int d = 0; d < DRUM_VOICES; d++) {
            nk_layout_row_begin(ctx, NK_STATIC, 22, 17);
            nk_layout_row_push(ctx, 70);
            nk_label(ctx, drum_names[d], NK_TEXT_LEFT);
            for (int s = 0; s < DRUM_STEPS; s++) {
                nk_layout_row_push(ctx, 24);
                struct nk_style_button style = ctx->style.button;
                if (s == current_step && g_app.drums.playing) {
                    style.normal.data.color = nk_rgb(255, 200, 0);
                    style.hover.data.color = nk_rgb(255, 220, 80);
                } else if (dpat->steps[d][s]) {
                    style.normal.data.color = nk_rgb(90, 170, 110);
                    style.hover.data.color = nk_rgb(110, 200, 140);
                }
                if (nk_button_label_styled(ctx, &style, dpat->steps[d][s] ? "●" : "○")) {
                    dpat->steps[d][s] = !dpat->steps[d][s];
                }
            }
            nk_layout_row_end(ctx);
        }
        nk_group_end(ctx);
    }
}

static void draw_sample_loader_panel(struct nk_context* ctx) {
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
    if (nk_group_begin_titled(ctx, "SAMPLE_PANEL", "Sample Loader", flags)) {
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label(ctx, "WAV File", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 26, 1);
        nk_flags sample_flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, g_app.sample_path_input,
                                                              sizeof(g_app.sample_path_input), nk_filter_default);
        if (sample_flags & NK_EDIT_CHANGED) {
            mark_project_dirty();
        }

        nk_layout_row_dynamic(ctx, 26, 3);
        if (nk_button_label(ctx, "Load")) {
            load_sample_file(g_app.sample_path_input);
        }
        if (nk_button_label(ctx, "Play")) {
            atomic_store(&g_app.sampler.trigger_pending, true);
        }
        if (nk_button_label(ctx, "Stop")) {
            atomic_store(&g_app.sampler.stop_pending, true);
        }

        draw_status_text(ctx, "Status", g_app.sample_status);

        if (g_app.sampler.buffer.data) {
            char info[64];
            double duration = (double)g_app.sampler.buffer.frame_count / (double)g_app.sampler.buffer.sample_rate;
            snprintf(info, sizeof(info), "%.2fs", duration);
            draw_status_text(ctx, "Duration", info);
            snprintf(info, sizeof(info), "%u ch @ %u Hz", g_app.sampler.buffer.channels, g_app.sampler.buffer.sample_rate);
            draw_status_text(ctx, "Format", info);
        }
        nk_group_end(ctx);
    }
}

static void draw_export_panel(struct nk_context* ctx) {
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
    if (nk_group_begin_titled(ctx, "EXPORT_PANEL", "Audio Export", flags)) {
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label(ctx, "Output Path", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 26, 1);
        nk_flags export_flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, g_app.export_path_input,
                                                              sizeof(g_app.export_path_input), nk_filter_default);
        if (export_flags & NK_EDIT_CHANGED) {
            mark_project_dirty();
        }

        nk_layout_row_dynamic(ctx, 24, 1);
        float prev_duration = g_app.export_duration_seconds;
        nk_property_float(ctx, "Length (s)", 1.0f, &g_app.export_duration_seconds, 60.0f, 0.5f, 0.1f);
        if (fabsf(g_app.export_duration_seconds - prev_duration) > 0.0001f) {
            mark_project_dirty();
        }

        nk_layout_row_dynamic(ctx, 28, 1);
        if (nk_button_label(ctx, "Render to WAV")) {
            start_export_job(g_app.export_duration_seconds, g_app.export_path_input);
        }

        bool exporting = atomic_load(&g_app.export_state.capture_active) || atomic_load(&g_app.export_state.arm_request);
        draw_status_text(ctx, "Recorder", exporting ? "Recording..." : "Ready");
        draw_status_text(ctx, "Result", g_app.export_status);
        nk_group_end(ctx);
    }
}

static void draw_performance_panel(struct nk_context* ctx) {
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

    nk_layout_row_dynamic(ctx, 130, 1);
    if (nk_group_begin_titled(ctx, "PERF_STATUS", "Session Status", flags)) {
        char tempo_text[32];
        snprintf(tempo_text, sizeof(tempo_text), "%.1f BPM", g_app.tempo);
        draw_status_text(ctx, "Tempo", tempo_text);

        Loop* loop = &g_app.loop_recorder.loops[g_app.loop_recorder.current_loop];
        char loop_text[64];
        snprintf(loop_text, sizeof(loop_text), "%s | %d events", loop->name, loop->num_events);
        draw_status_text(ctx, "Loop", loop_text);

        snprintf(loop_text, sizeof(loop_text), "Len %.2fs", loop->loop_length);
        draw_status_text(ctx, "Loop Length", loop_text);

        draw_status_text(ctx, "Sample", g_app.sample_status);
        draw_status_text(ctx, "Export", g_app.export_status);
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 70, 1);
    if (nk_group_begin_titled(ctx, "PERF_TIPS", "Quick Tips", flags)) {
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label_wrap(ctx, "Space = loop toggle\nESC = panic\nDrag across the keyboard to glide notes");
        nk_group_end(ctx);
    }
}

static void draw_transport_bar(struct nk_context* ctx) {
    nk_layout_row_dynamic(ctx, 90, 1);
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
    if (nk_group_begin_titled(ctx, "TRANSPORT", "Global Controls", flags)) {
        nk_layout_row_dynamic(ctx, 36, 6);
        if (nk_button_label(ctx, g_app.loop_recorder.playing ? "⏸ Loop" : "▶ Loop")) {
            if (g_app.loop_recorder.playing) {
                loop_recorder_stop_playback(&g_app.loop_recorder, &g_app.synth);
            } else {
                loop_recorder_start_playback(&g_app.loop_recorder);
            }
        }

        struct nk_style_button rec_style = ctx->style.button;
        if (g_app.loop_recorder.recording) {
            rec_style.normal.data.color = nk_rgb(200, 40, 40);
            rec_style.hover.data.color = nk_rgb(230, 70, 70);
        }
        if (nk_button_label_styled(ctx, &rec_style, g_app.loop_recorder.recording ? "REC" : "● REC")) {
            if (g_app.loop_recorder.recording) {
                loop_recorder_stop_recording(&g_app.loop_recorder, &g_app.synth);
            } else {
                loop_recorder_start_recording(&g_app.loop_recorder);
            }
        }

        if (nk_button_label(ctx, g_app.drums.playing ? "⏸ Drums" : "▶ Drums")) {
            g_app.drums.playing = !g_app.drums.playing;
        }

        if (nk_button_label(ctx, "▶ Sample")) {
            atomic_store(&g_app.sampler.trigger_pending, true);
        }
        if (nk_button_label(ctx, "■ Sample")) {
            atomic_store(&g_app.sampler.stop_pending, true);
        }
        if (nk_button_label(ctx, "PANIC")) {
            enqueue_param_bool(PARAM_PANIC, true);
        }

        nk_layout_row_dynamic(ctx, 30, 3);
        nk_label(ctx, "Tempo", NK_TEXT_LEFT);
        float prev_tempo = g_app.tempo_ui;
        nk_slider_float(ctx, 60.0f, &g_app.tempo_ui, 200.0f, 0.5f);
        if (fabsf(g_app.tempo_ui - prev_tempo) > 0.01f) {
            enqueue_param_float_user(PARAM_TEMPO, g_app.tempo_ui);
        }
        char tempo_text[32];
        snprintf(tempo_text, sizeof(tempo_text), "%.1f BPM", g_app.tempo_ui);
        nk_label(ctx, tempo_text, NK_TEXT_RIGHT);

        draw_status_text(ctx, "Sample", g_app.sample_status);
        draw_status_text(ctx, "Export", g_app.export_status);

        nk_group_end(ctx);
    }
    nk_layout_row_dynamic(ctx, 8, 1);
    nk_label(ctx, "", NK_TEXT_LEFT);
}

static void draw_fx_panel(struct nk_context* ctx);
static void draw_loop_panel(struct nk_context* ctx);
static void draw_drums_panel(struct nk_context* ctx);

static void apply_param_change(const ParamMsg* change, void* userdata) {
    (void)userdata;
    if (!change) return;

    float fvalue = param_msg_as_float(change);
    bool bvalue = param_msg_as_bool(change);
    int  ivalue = param_msg_as_int(change);

    switch ((ParamId)change->id) {
        case PARAM_MASTER_VOLUME:
            g_app.master_volume = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_TEMPO:
            g_app.tempo = clampf(fvalue, 40.0f, 260.0f);
            break;
        case PARAM_FX_DISTORTION_ENABLED:
            g_app.fx.distortion.enabled = bvalue;
            break;
        case PARAM_FX_DISTORTION_DRIVE:
            g_app.fx.distortion.drive = clampf(fvalue, 1.0f, 20.0f);
            break;
        case PARAM_FX_DISTORTION_MIX:
            g_app.fx.distortion.mix = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_FX_CHORUS_ENABLED:
            g_app.fx.chorus.enabled = bvalue;
            break;
        case PARAM_FX_CHORUS_RATE:
            g_app.fx.chorus.rate = clampf(fvalue, 0.1f, 5.0f);
            break;
        case PARAM_FX_CHORUS_DEPTH:
            g_app.fx.chorus.depth = clampf(fvalue, 1.0f, 50.0f);
            break;
        case PARAM_FX_CHORUS_MIX:
            g_app.fx.chorus.mix = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_FX_COMP_ENABLED:
            g_app.fx.compressor.enabled = bvalue;
            break;
        case PARAM_FX_COMP_THRESHOLD:
            g_app.fx.compressor.threshold = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_FX_COMP_RATIO:
            g_app.fx.compressor.ratio = clampf(fvalue, 1.0f, 20.0f);
            break;
        case PARAM_FX_DELAY_ENABLED:
            g_app.fx.delay.enabled = bvalue;
            break;
        case PARAM_FX_DELAY_TIME:
            g_app.fx.delay.time = clampf(fvalue, 0.05f, 2.0f);
            break;
        case PARAM_FX_DELAY_FEEDBACK:
            g_app.fx.delay.feedback = clampf(fvalue, 0.0f, 0.99f);
            break;
        case PARAM_FX_DELAY_MIX:
            g_app.fx.delay.mix = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_FX_REVERB_ENABLED:
            g_app.fx.reverb.enabled = bvalue;
            break;
        case PARAM_FX_REVERB_SIZE:
            g_app.fx.reverb.size = clampf(fvalue, 0.1f, 0.95f);
            break;
        case PARAM_FX_REVERB_DAMPING:
            g_app.fx.reverb.damping = clampf(fvalue, 0.0f, 0.99f);
            break;
        case PARAM_FX_REVERB_MIX:
            g_app.fx.reverb.mix = clampf(fvalue, 0.0f, 1.0f);
            break;
        case PARAM_ARP_ENABLED:
            g_app.arp.enabled = bvalue;
            if (!g_app.arp.enabled && g_app.arp.last_played_note >= 0) {
                synth_note_off(&g_app.synth, g_app.arp.last_played_note);
                g_app.arp.last_played_note = -1;
            }
            break;
        case PARAM_ARP_RATE: {
                int idx = arp_multiplier_index_for_value(fvalue);
                g_app.arp.rate = arp_multiplier_value(idx);
                break;
            }
        case PARAM_ARP_MODE: {
                int mode = ivalue;
                if (mode < ARP_OFF) mode = ARP_OFF;
                if (mode > ARP_RANDOM) mode = ARP_RANDOM;
                g_app.arp.mode = (ArpMode)mode;
                break;
            }
        case PARAM_FILTER_CUTOFF: {
                float cutoff = clampf(fvalue, 20.0f, 20000.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].filter.cutoff = cutoff;
                }
                break;
            }
        case PARAM_FILTER_RESONANCE: {
                float resonance = clampf(fvalue, 0.0f, 1.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].filter.resonance = resonance;
                }
                break;
            }
        case PARAM_FILTER_MODE: {
                int mode = ivalue;
                if (mode < FILTER_LP) mode = FILTER_LP;
                if (mode >= FILTER_COUNT) mode = FILTER_COUNT - 1;
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].filter.mode = (FilterMode)mode;
                }
                break;
            }
        case PARAM_FILTER_ENV_AMOUNT: {
                float amount = clampf(fvalue, -1.0f, 1.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].filter.env_amount = amount;
                }
                break;
            }
        case PARAM_ENV_ATTACK: {
                float attack = clampf(fvalue, 0.001f, 2.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].env_amp.attack = attack;
                }
                break;
            }
        case PARAM_ENV_DECAY: {
                float decay = clampf(fvalue, 0.001f, 2.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].env_amp.decay = decay;
                }
                break;
            }
        case PARAM_ENV_SUSTAIN: {
                float sustain = clampf(fvalue, 0.0f, 1.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].env_amp.sustain = sustain;
                }
                break;
            }
        case PARAM_ENV_RELEASE: {
                float release = clampf(fvalue, 0.001f, 5.0f);
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].env_amp.release = release;
                }
                break;
            }
        case PARAM_PANIC:
            synth_all_notes_off(&g_app.synth);
            g_app.arp.num_held = 0;
            g_app.arp.last_played_note = -1;
            memset(g_app.keyboard_ui.note_active, 0, sizeof(g_app.keyboard_ui.note_active));
            g_app.keyboard_ui.mouse_dragging = false;
            g_app.keyboard_ui.mouse_active_note = -1;
            break;
        default:
            break;
    }
}

static void dispatch_note_on(uint8_t note, float velocity) {
    if (g_app.arp.enabled) {
        arp_note_on(&g_app.arp, note);
    } else {
        synth_note_on(&g_app.synth, note, velocity);
    }
}

static void dispatch_note_off(uint8_t note) {
    if (g_app.arp.enabled) {
        arp_note_off(&g_app.arp, note);
    } else {
        synth_note_off(&g_app.synth, note);
    }
}

static void keyboard_set_note_active(uint8_t note, bool down) {
    if (note < 128) {
        g_app.keyboard_ui.note_active[note] = down;
    }
}

static void ui_note_on(uint8_t note) {
    if (note >= 128) return;
    if (g_app.keyboard_ui.note_active[note]) return;
    keyboard_set_note_active(note, true);
    if (g_app.loop_recorder.recording) {
        loop_recorder_record_note_on(&g_app.loop_recorder, note, 1.0f);
    }
    midi_queue_send_note_on(note, 110);
}

static void ui_note_off(uint8_t note) {
    if (note >= 128) return;
    if (!g_app.keyboard_ui.note_active[note]) return;
    keyboard_set_note_active(note, false);
    if (g_app.loop_recorder.recording) {
        loop_recorder_record_note_off(&g_app.loop_recorder, note);
    }
    midi_queue_send_note_off(note);
}

static void handle_midi_event(const MidiEvent* event, void* userdata) {
    (void)userdata;
    if (!event) {
        return;
    }

    switch (event->type) {
        case MIDI_EVENT_NOTE_ON: {
            float velocity = (float)event->data2 / 127.0f;
            if (velocity <= 0.0f) {
                keyboard_set_note_active(event->data1, false);
                dispatch_note_off(event->data1);
            } else {
                keyboard_set_note_active(event->data1, true);
                dispatch_note_on(event->data1, velocity);
            }
            break;
        }
        case MIDI_EVENT_NOTE_OFF:
            keyboard_set_note_active(event->data1, false);
            dispatch_note_off(event->data1);
            break;
        case MIDI_EVENT_PITCH_BEND: {
            uint16_t value = ((uint16_t)event->data2 << 7) | event->data1;
            float amount = ((float)value - 8192.0f) / 8192.0f;
            synth_pitch_bend(&g_app.synth, amount);
            break;
        }
        case MIDI_EVENT_CONTROL_CHANGE: {
            // Map common CCs to parameters (e.g., CC1 = filter cutoff)
            if (event->data1 == 1) { // Mod wheel → filter cutoff sweep
                float normalized = (float)event->data2 / 127.0f;
                float cutoff = 200.0f + normalized * 19800.0f;
                enqueue_param_float(PARAM_FILTER_CUTOFF, cutoff);
            } else if (event->data1 == 74) { // CC74 often "brightness"
                float normalized = (float)event->data2 / 127.0f;
                float resonance = normalized;
                enqueue_param_float(PARAM_FILTER_RESONANCE, resonance);
            }
            break;
        }
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_PROGRAM_CHANGE:
        default:
            break;
    }
}

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    (void)device;
    float* out = (float*)output;
    (void)input;
    
    double sample_duration = 1.0 / g_app.synth.sample_rate;
    param_queue_drain(apply_param_change, NULL);
    midi_queue_drain(handle_midi_event, NULL);
    
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

    // Mix sampler playback (shares FX with synth voices)
    sample_slot_mix(&g_app.sampler, g_app.synth.sample_rate, &left, &right);

    // Keep drums dry: render now but sum after the FX chain
    float drums = drum_machine_render(&g_app.drums);
        
    // Apply effects to synth/sampler bus only
    fx_distortion_process(&g_app.fx.distortion, &left, &right);
    fx_chorus_process(&g_app.fx.chorus, &left, &right, g_app.synth.sample_rate);
    fx_delay_process(&g_app.fx.delay, &left, &right, g_app.synth.sample_rate);
    fx_reverb_process(&g_app.fx.reverb, &left, &right);
    fx_compressor_process(&g_app.fx.compressor, &left, &right, g_app.synth.sample_rate);

    // Finally add drums post-FX so their tone stays dry
    left += drums;
    right += drums;
        
        // Master volume
        left *= g_app.master_volume;
        right *= g_app.master_volume;
        
        // Capture export buffer if requested
        sample_export_process_frame(&g_app.export_state, left, right);

        out[i * 2 + 0] = left;
        out[i * 2 + 1] = right;
        
        g_app.current_time += sample_duration;
    }
}

// ============================================================================
// GUI
// ============================================================================

void draw_gui(struct nk_context* ctx) {
    ui_apply_theme(ctx);

    if (nk_begin(ctx, "Professional Synthesizer", nk_rect(0, 0, 1600, 900),
                 NK_WINDOW_NO_SCROLLBAR)) {
        draw_transport_bar(ctx);

    nk_layout_row_dynamic(ctx, 380, 3);
        nk_flags column_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;
        if (nk_group_begin_titled(ctx, "COL_SYNTH", "Synth Engine", column_flags)) {
            draw_synth_panel(ctx);
            nk_group_end(ctx);
        }
        if (nk_group_begin_titled(ctx, "COL_FX", "Effects Rack", column_flags)) {
            draw_fx_panel(ctx);
            nk_group_end(ctx);
        }
        if (nk_group_begin_titled(ctx, "COL_RHYTHM", "Groove Lab", column_flags)) {
            draw_loop_panel(ctx);
            nk_layout_row_dynamic(ctx, 8, 1);
            nk_label(ctx, "", NK_TEXT_LEFT);
            draw_drums_panel(ctx);
            nk_group_end(ctx);
        }

    nk_layout_row_dynamic(ctx, 6, 1);
        nk_label(ctx, "", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 190, 3);
        if (nk_group_begin_titled(ctx, "COL_SAMPLE", "Samples", column_flags)) {
            draw_sample_loader_panel(ctx);
            nk_group_end(ctx);
        }
        if (nk_group_begin_titled(ctx, "COL_EXPORT", "Export", column_flags)) {
            draw_export_panel(ctx);
            nk_group_end(ctx);
        }
        if (nk_group_begin_titled(ctx, "COL_PERF", "Performance", column_flags)) {
            draw_performance_panel(ctx);
            nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 200, 1);
        draw_keyboard_panel(ctx);
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

    sample_buffer_init(&g_app.sampler.buffer);
    g_app.sampler.gain = 0.85f;
    g_app.sampler.last_loaded_path[0] = '\0';
    snprintf(g_app.sample_path_input, sizeof(g_app.sample_path_input),
             "samples/demo.wav");
    snprintf(g_app.export_path_input, sizeof(g_app.export_path_input),
             "exports/bounce.wav");
    g_app.export_duration_seconds = 8.0f;
    snprintf(g_app.sample_status, sizeof(g_app.sample_status),
             "No sample loaded");
    snprintf(g_app.export_status, sizeof(g_app.export_status), "Idle");

    g_app.keyboard_ui.base_note = 48;
    g_app.keyboard_ui.num_keys = 36;
    memset(g_app.keyboard_ui.note_active, 0, sizeof(g_app.keyboard_ui.note_active));
    g_app.keyboard_ui.mouse_dragging = false;
    g_app.keyboard_ui.mouse_active_note = -1;
    
    // Initialize synth engine
    synth_init(&g_app.synth, 44100.0f);
    g_app.filter_cutoff = 8000.0f;
    g_app.filter_resonance = 0.3f;
    g_app.filter_mode = FILTER_LP;
    g_app.filter_env = 0.0f;
    g_app.env_attack = 0.01f;
    g_app.env_decay = 0.1f;
    g_app.env_sustain = 0.7f;
    g_app.env_release = 0.3f;
    for (int i = 0; i < MAX_VOICES; i++) {
        g_app.synth.voices[i].filter.cutoff = g_app.filter_cutoff;
        g_app.synth.voices[i].filter.resonance = g_app.filter_resonance;
        g_app.synth.voices[i].filter.mode = g_app.filter_mode;
        g_app.synth.voices[i].filter.env_amount = g_app.filter_env;
        g_app.synth.voices[i].env_amp.attack = g_app.env_attack;
        g_app.synth.voices[i].env_amp.decay = g_app.env_decay;
        g_app.synth.voices[i].env_amp.sustain = g_app.env_sustain;
        g_app.synth.voices[i].env_amp.release = g_app.env_release;
    }
    
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

    // Mirror effect parameters into UI copies
    g_app.fx_ui.distortion.enabled = g_app.fx.distortion.enabled;
    g_app.fx_ui.distortion.drive = g_app.fx.distortion.drive;
    g_app.fx_ui.distortion.mix = g_app.fx.distortion.mix;
    g_app.fx_ui.chorus.enabled = g_app.fx.chorus.enabled;
    g_app.fx_ui.chorus.rate = g_app.fx.chorus.rate;
    g_app.fx_ui.chorus.depth = g_app.fx.chorus.depth;
    g_app.fx_ui.chorus.mix = g_app.fx.chorus.mix;
    g_app.fx_ui.delay.enabled = g_app.fx.delay.enabled;
    g_app.fx_ui.delay.time = g_app.fx.delay.time;
    g_app.fx_ui.delay.feedback = g_app.fx.delay.feedback;
    g_app.fx_ui.delay.mix = g_app.fx.delay.mix;
    g_app.fx_ui.reverb.enabled = g_app.fx.reverb.enabled;
    g_app.fx_ui.reverb.size = g_app.fx.reverb.size;
    g_app.fx_ui.reverb.damping = g_app.fx.reverb.damping;
    g_app.fx_ui.reverb.mix = g_app.fx.reverb.mix;
    g_app.fx_ui.compressor.enabled = g_app.fx.compressor.enabled;
    g_app.fx_ui.compressor.threshold = g_app.fx.compressor.threshold;
    g_app.fx_ui.compressor.ratio = g_app.fx.compressor.ratio;
    
    // Initialize loop recorder, drums, arp
    loop_recorder_init(&g_app.loop_recorder);
    drum_machine_init(&g_app.drums, 44100.0f);
    arp_init(&g_app.arp);
    g_app.arp_ui.enabled = g_app.arp.enabled;
    g_app.arp_ui.rate_index = arp_multiplier_index_for_value(g_app.arp.rate);
    g_app.arp_ui.mode = g_app.arp.mode;
    
    // Initialize parameter queue
    param_queue_init();
    midi_input_start();
    
    // Set defaults
    g_app.tempo = 120.0f;
    g_app.tempo_ui = g_app.tempo;
    g_app.master_volume = 0.7f;
    g_app.master_volume_ui = g_app.master_volume;
    
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

    bool space_was_pressed = false;
    bool esc_was_pressed = false;
    
    // Main loop
    while (!glfwWindowShouldClose(g_app.window)) {
        glfwPollEvents();
        poll_export_job();
        
        // Poll keyboard directly (bypass Nuklear stealing input)
        for (size_t i = 0; i < KEYMAP_SIZE; i++) {
            int state = glfwGetKey(g_app.window, g_keymap[i].glfw_key);
            uint8_t midi_note = (uint8_t)g_keymap[i].midi_note;
            
            // Key just pressed
            if (state == GLFW_PRESS && !g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 1;
                
                // Record note on if recording
                if (g_app.loop_recorder.recording) {
                    loop_recorder_record_note_on(&g_app.loop_recorder, 
                                                 g_keymap[i].midi_note, 1.0f);
                }
                
                keyboard_set_note_active(midi_note, true);
                midi_queue_send_note_on(midi_note, 110);
                
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
                
                keyboard_set_note_active(midi_note, false);
                midi_queue_send_note_off(midi_note);
                
                printf("🎹 %s OFF\n", g_keymap[i].label);
            }
        }
        
        // Space = toggle loop playback
        if (glfwGetKey(g_app.window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!space_was_pressed) {
                if (g_app.loop_recorder.playing) {
                    loop_recorder_stop_playback(&g_app.loop_recorder, &g_app.synth);
                } else {
                    loop_recorder_start_playback(&g_app.loop_recorder);
                }
                space_was_pressed = true;
            }
        } else {
            space_was_pressed = false;
        }
        
        // ESC = panic
        if (glfwGetKey(g_app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (!esc_was_pressed) {
                enqueue_param_bool(PARAM_PANIC, true);
                esc_was_pressed = true;
            }
        } else {
            esc_was_pressed = false;
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
    midi_input_stop();
    ma_device_uninit(&g_app.audio_device);
    nk_glfw3_shutdown(&g_app.glfw);
    glfwTerminate();
    sample_buffer_free(&g_app.sampler.buffer);
    if (g_app.export_state.buffer) {
        free(g_app.export_state.buffer);
        g_app.export_state.buffer = NULL;
    }
    
    printf("\n👋 Goodbye!\n");
    return 0;
}
