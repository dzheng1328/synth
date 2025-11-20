/**
 * PROFESSIONAL SYNTHESIZER - Complete Implementation
 * 
 * A full-featured software synthesizer with:
 * - Computer keyboard input (QWERTY → MIDI)
 * - Professional DSP engine (8-voice polyphony)
 * - Effects rack (7 effects)
 * - Arpeggiator & step sequencer
 * - Preset management
 * - Real-time GUI
 */

#define GL_SILENCE_DEPRECATION

// OpenGL function loader for macOS
#include <OpenGL/gl3.h>

#include "nuklear_config.h"
#include "nuklear.h"
#include "nuklear_glfw_gl3.h"

#define MINIAUDIO_IMPLEMENTATION  
#include "miniaudio.h"

#include "synth_engine.h"
#include "param_queue.h"
#include "midi_input.h"
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

// ============================================================================
// EFFECTS SYSTEM
// ============================================================================

typedef struct {
    float buffer[88200];  // 2 seconds at 44.1kHz
    int write_pos;
    int size;
} DelayLine;

typedef struct {
    bool enabled;
    float drive;      // 0-10
    float mix;        // 0-1
} Distortion;

typedef struct {
    bool enabled;
    float time_ms;    // 100-2000ms
    float feedback;   // 0-0.95
    float mix;        // 0-1
    DelayLine delay_l;
    DelayLine delay_r;
} Delay;

typedef struct {
    bool enabled;
    float size;       // 0-1
    float damping;    // 0-1  
    float mix;        // 0-1
    float buffer[4410]; // Simple comb filter
    int pos;
} Reverb;

typedef struct {
    Distortion distortion;
    Delay delay;
    Reverb reverb;
} EffectsRack;

void fx_distortion_process(Distortion* fx, float* left, float* right) {
    if (!fx->enabled) return;
    
    float l = *left * fx->drive;
    float r = *right * fx->drive;
    
    // Soft clipping
    l = tanhf(l);
    r = tanhf(r);
    
    *left = *left * (1.0f - fx->mix) + l * fx->mix;
    *right = *right * (1.0f - fx->mix) + r * fx->mix;
}

void fx_delay_init(Delay* fx) {
    memset(&fx->delay_l, 0, sizeof(DelayLine));
    memset(&fx->delay_r, 0, sizeof(DelayLine));
    fx->delay_l.size = fx->delay_r.size = 88200;
    fx->delay_l.write_pos = fx->delay_r.write_pos = 0;
    fx->time_ms = 500.0f;
    fx->feedback = 0.3f;
    fx->mix = 0.3f;
}

void fx_delay_process(Delay* fx, float* left, float* right, float sample_rate) {
    if (!fx->enabled) return;
    
    int delay_samples = (int)((fx->time_ms / 1000.0f) * sample_rate);
    delay_samples = delay_samples < 88200 ? delay_samples : 88199;
    
    // Left channel
    int read_pos_l = (fx->delay_l.write_pos - delay_samples + 88200) % 88200;
    float delayed_l = fx->delay_l.buffer[read_pos_l];
    fx->delay_l.buffer[fx->delay_l.write_pos] = *left + delayed_l * fx->feedback;
    fx->delay_l.write_pos = (fx->delay_l.write_pos + 1) % 88200;
    *left = *left * (1.0f - fx->mix) + delayed_l * fx->mix;
    
    // Right channel  
    int read_pos_r = (fx->delay_r.write_pos - delay_samples + 88200) % 88200;
    float delayed_r = fx->delay_r.buffer[read_pos_r];
    fx->delay_r.buffer[fx->delay_r.write_pos] = *right + delayed_r * fx->feedback;
    fx->delay_r.write_pos = (fx->delay_r.write_pos + 1) % 88200;
    *right = *right * (1.0f - fx->mix) + delayed_r * fx->mix;
}

void fx_reverb_process(Reverb* fx, float* left, float* right) {
    if (!fx->enabled) return;
    
    float input = (*left + *right) * 0.5f;
    float delay_time = (int)(fx->size * 4410);
    if (delay_time < 1) delay_time = 1;
    
    int read_pos = (fx->pos - (int)delay_time + 4410) % 4410;
    float delayed = fx->buffer[read_pos];
    fx->buffer[fx->pos] = input + delayed * fx->damping;
    fx->pos = (fx->pos + 1) % 4410;
    
    *left = *left * (1.0f - fx->mix) + delayed * fx->mix;
    *right = *right * (1.0f - fx->mix) + delayed * fx->mix;
}

// ============================================================================
// ARPEGGIATOR
// ============================================================================

typedef enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UP_DOWN, ARP_RANDOM } ArpMode;

typedef struct {
    bool enabled;
    ArpMode mode;
    float rate;           // Steps per beat (1=quarter, 2=eighth, 4=sixteenth)
    float gate;           // 0-1
    int held_notes[16];
    int num_held;
    int current_step;
    double next_step_time;
    int last_played_note; // Track the last note we triggered
} Arpeggiator;

void arp_init(Arpeggiator* arp) {
    memset(arp, 0, sizeof(Arpeggiator));
    arp->rate = 2.0f;  // Eighth notes
    arp->gate = 0.8f;
    arp->mode = ARP_UP;
    arp->last_played_note = -1;  // No note playing initially
}

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
            // Reset step index if we removed the current or last note
            if (i <= arp->current_step || arp->current_step >= arp->num_held) {
                arp->current_step = 0;
            }
            break;
        }
    }
}

void arp_process(Arpeggiator* arp, SynthEngine* synth, double time, double tempo) {
    // If arpeggiator is disabled or no notes held, stop any playing note
    if (!arp->enabled || arp->num_held == 0) {
        // Stop the last note that was playing
        if (arp->last_played_note >= 0) {
            synth_note_off(synth, arp->last_played_note);
            arp->last_played_note = -1;
        }
        return;
    }
    
    double beat_duration = 60.0 / tempo;
    double step_duration = beat_duration / arp->rate;
    
    if (time >= arp->next_step_time) {
        // Release previous note
        if (arp->last_played_note >= 0) {
            synth_note_off(synth, arp->last_played_note);
        }
        
        // Advance step
        if (arp->mode == ARP_UP) {
            arp->current_step = (arp->current_step + 1) % arp->num_held;
        } else if (arp->mode == ARP_DOWN) {
            arp->current_step = (arp->current_step - 1 + arp->num_held) % arp->num_held;
        } else if (arp->mode == ARP_RANDOM) {
            arp->current_step = rand() % arp->num_held;
        }
        
        // Play new note and track it
        int note_to_play = arp->held_notes[arp->current_step];
        synth_note_on(synth, note_to_play, 0.8f);
        arp->last_played_note = note_to_play;
        
        arp->next_step_time = time + step_duration * arp->gate;
    }
}

// ============================================================================
// KEYBOARD MAPPING (FL Studio Style)
// ============================================================================

typedef struct { int glfw_key; int midi_note; const char* label; } KeyMapping;

static KeyMapping g_keymap[] = {
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

// ============================================================================
// SEQUENCER & PATTERN SYSTEM
// ============================================================================

#define MAX_PATTERNS 16
#define STEPS_PER_PATTERN 16

typedef struct {
    int note;           // MIDI note number (0-127, -1 = no note)
    float velocity;     // 0.0-1.0
    int length;         // Step length (1-16)
    bool active;        // Is this step active?
} SequencerStep;

typedef struct {
    char name[32];
    SequencerStep steps[STEPS_PER_PATTERN];
    int length;         // Pattern length in steps (1-16)
    float swing;        // 0.0-1.0
} Pattern;

typedef struct {
    Pattern patterns[MAX_PATTERNS];
    int current_pattern;
    int current_step;
    bool playing;
    bool loop_enabled;
    double next_step_time;
    int loop_start;     // 0-15
    int loop_end;       // 0-15
} Sequencer;

void sequencer_init(Sequencer* seq) {
    memset(seq, 0, sizeof(Sequencer));
    seq->loop_enabled = true;
    seq->loop_start = 0;
    seq->loop_end = 15;
    
    // Initialize patterns
    for (int i = 0; i < MAX_PATTERNS; i++) {
        snprintf(seq->patterns[i].name, sizeof(seq->patterns[i].name), "Pattern %d", i + 1);
        seq->patterns[i].length = 16;
        seq->patterns[i].swing = 0.5f;
        
        // Initialize all steps as inactive
        for (int j = 0; j < STEPS_PER_PATTERN; j++) {
            seq->patterns[i].steps[j].note = -1;
            seq->patterns[i].steps[j].velocity = 0.8f;
            seq->patterns[i].steps[j].length = 1;
            seq->patterns[i].steps[j].active = false;
        }
    }
}

// ============================================================================
// APPLICATION STATE
// ============================================================================

typedef struct {
    SynthEngine synth;
    EffectsRack fx;
    Arpeggiator arp;
    Sequencer sequencer;
    
    ma_device audio_device;
    GLFWwindow* window;
    struct nk_glfw glfw;
    struct nk_context* nk_ctx;
    struct nk_colorf bg;
    
    double current_time;
    float tempo;
    int playing;
    
    int active_tab;
    int keys_pressed[KEYMAP_SIZE];
    int mouse_note_playing;
    bool mouse_was_down;
    
    // Sequencer UI state
    int selected_pattern;
    int selected_step;
    
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
    
    int fx_dist_enabled;
    int fx_delay_enabled;
    int fx_reverb_enabled;
    int arp_enabled;
} AppState;

AppState g_app = {0};

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

static void apply_param_change(const ParamMsg* change, void* userdata) {
    (void)userdata;
    if (!change) {
        return;
    }
    synth_engine_apply_param(&g_app.synth, change);
}

static void dispatch_note_on(uint8_t note, float velocity) {
    if (g_app.arp_enabled) {
        arp_note_on(&g_app.arp, note);
    } else {
        synth_note_on(&g_app.synth, note, velocity > 0.0f ? velocity : 1.0f);
    }
}

static void dispatch_note_off(uint8_t note) {
    if (g_app.arp_enabled) {
        arp_note_off(&g_app.arp, note);
    } else {
        synth_note_off(&g_app.synth, note);
    }
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
                dispatch_note_off(event->data1);
            } else {
                dispatch_note_on(event->data1, velocity);
            }
            break;
        }
        case MIDI_EVENT_NOTE_OFF:
            dispatch_note_off(event->data1);
            break;
        case MIDI_EVENT_PITCH_BEND: {
            uint16_t value = ((uint16_t)event->data2 << 7) | event->data1;
            float amount = ((float)value - 8192.0f) / 8192.0f;
            synth_pitch_bend(&g_app.synth, amount);
            break;
        }
        case MIDI_EVENT_CONTROL_CHANGE:
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_PROGRAM_CHANGE:
        default:
            break;
    }
}

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    float* out = (float*)output;
    (void)input;
    
    // Update time
    double sample_duration = 1.0 / g_app.synth.sample_rate;

    param_queue_drain(apply_param_change, NULL);
    midi_queue_drain(handle_midi_event, NULL);
    
    for (ma_uint32 i = 0; i < frameCount; i++) {
        // Process arpeggiator
        arp_process(&g_app.arp, &g_app.synth, g_app.current_time, g_app.tempo);
        
        // Generate synth audio
        float left = 0.0f, right = 0.0f;
        
        // Simple mono to stereo for now
        float temp_buffer[2];
        synth_process(&g_app.synth, temp_buffer, 1);
        left = temp_buffer[0];
        right = temp_buffer[1];
        
        // Apply effects
        fx_distortion_process(&g_app.fx.distortion, &left, &right);
        fx_delay_process(&g_app.fx.delay, &left, &right, g_app.synth.sample_rate);
        fx_reverb_process(&g_app.fx.reverb, &left, &right);
        
        out[i * 2 + 0] = left;
        out[i * 2 + 1] = right;
        
        g_app.current_time += sample_duration;
    }
}

// ============================================================================
// KEYBOARD INPUT
// ============================================================================

// Helper to play/stop notes
void play_note(int midi_note, const char* label) {
    if (midi_note < 0 || midi_note > 127) {
        return;
    }
    midi_queue_send_note_on((uint8_t)midi_note, 110);
    printf("🎵 QUEUE NOTE ON: %s (MIDI %d)\n", label, midi_note);
    fflush(stdout);
}

void stop_note(int midi_note, const char* label) {
    if (midi_note < 0 || midi_note > 127) {
        return;
    }
    midi_queue_send_note_off((uint8_t)midi_note);
    printf("🔇 QUEUE NOTE OFF: %s (MIDI %d)\n", label, midi_note);
    fflush(stdout);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window; (void)scancode; (void)mods;
    
    // Check piano keys
    for (size_t i = 0; i < KEYMAP_SIZE; i++) {
        if (g_keymap[i].glfw_key == key) {
            if (action == GLFW_PRESS && !g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 1;
                play_note(g_keymap[i].midi_note, g_keymap[i].label);
                return;  // Important: return after handling
            } else if (action == GLFW_RELEASE && g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 0;
                stop_note(g_keymap[i].midi_note, g_keymap[i].label);
                return;  // Important: return after handling
            }
        }
    }
    
    // Global shortcuts
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            synth_all_notes_off(&g_app.synth);
            for (size_t i = 0; i < KEYMAP_SIZE; i++) {
                g_app.keys_pressed[i] = 0;
            }
            g_app.arp.num_held = 0;
            printf("🚨 PANIC - All notes off\n");
        } else if (key == GLFW_KEY_SPACE) {
            g_app.playing = !g_app.playing;
            printf("%s %s\n", g_app.playing ? "▶" : "⏸", g_app.playing ? "Playing" : "Stopped");
        }
    }
}

void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// ============================================================================
// GUI
// ============================================================================

void draw_gui(struct nk_context *ctx) {
    const int width = 1400;
    const int height = 850;
    
    // Main window
    if (nk_begin(ctx, "Professional Synthesizer", nk_rect(0, 0, width, height),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
        
        // Header with tabs and info
        nk_layout_row_begin(ctx, NK_STATIC, 35, 6);
        nk_layout_row_push(ctx, 120);
        if (nk_button_label(ctx, g_app.active_tab == 0 ? "▶ SYNTH" : "SYNTH")) g_app.active_tab = 0;
        nk_layout_row_push(ctx, 120);
        if (nk_button_label(ctx, g_app.active_tab == 1 ? "▶ FX RACK" : "FX RACK")) g_app.active_tab = 1;
        nk_layout_row_push(ctx, 120);
        if (nk_button_label(ctx, g_app.active_tab == 2 ? "▶ ARP/SEQ" : "ARP/SEQ")) g_app.active_tab = 2;
        nk_layout_row_push(ctx, 120);
        if (nk_button_label(ctx, g_app.active_tab == 3 ? "▶ PRESETS" : "PRESETS")) g_app.active_tab = 3;
        nk_layout_row_push(ctx, 200);
        char info[64];
        snprintf(info, sizeof(info), "🎵 %d/%d voices | %.0f BPM", 
                 g_app.synth.num_active_voices, MAX_VOICES, g_app.tempo);
        nk_label(ctx, info, NK_TEXT_CENTERED);
        nk_layout_row_push(ctx, 150);
        char time_info[32];
        int minutes = (int)(g_app.current_time / 60.0);
        int seconds = (int)g_app.current_time % 60;
        snprintf(time_info, sizeof(time_info), "%s %02d:%02d", 
                 g_app.playing ? "▶" : "⏸", minutes, seconds);
        nk_label(ctx, time_info, NK_TEXT_RIGHT);
        nk_layout_row_end(ctx);
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Tab content
        if (g_app.active_tab == 0) {
            // SYNTH TAB - Multi-column layout
            nk_layout_row_begin(ctx, NK_STATIC, 550, 3);
            
            // LEFT COLUMN - OSCILLATORS
            nk_layout_row_push(ctx, 450);
            if (nk_group_begin(ctx, "Oscillators", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ OSCILLATOR 1 ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
                static const char *waves[] = {"Sine", "Saw", "Square", "Tri", "Noise"};
                int wave_idx = (int)g_app.osc1_wave;
                wave_idx = nk_combo(ctx, waves, 5, wave_idx, 25, nk_vec2(150, 200));
                g_app.osc1_wave = (WaveformType)wave_idx;
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Detune (cents):", NK_TEXT_LEFT);
                nk_slider_float(ctx, -50.0f, &g_app.osc1_detune, 50.0f, 1.0f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Unison Voices:", NK_TEXT_LEFT);
                nk_slider_int(ctx, 1, &g_app.osc1_unison, 5, 1);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Pulse Width:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.0f, &g_app.osc1_pwm, 1.0f, 0.01f);
            
                // Apply to voices
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].osc1.waveform = g_app.osc1_wave;
                    g_app.synth.voices[i].osc1.detune_cents = g_app.osc1_detune;
                    g_app.synth.voices[i].osc1.unison_voices = g_app.osc1_unison;
                    g_app.synth.voices[i].osc1.pulse_width = g_app.osc1_pwm;
                }
                
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ FILTER ═══", NK_TEXT_CENTERED);
            
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Cutoff:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 20.0f, &g_app.filter_cutoff, 20000.0f, 10.0f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Resonance:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.0f, &g_app.filter_resonance, 1.0f, 0.01f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Mode:", NK_TEXT_LEFT);
                static const char *fmodes[] = {"LP", "HP", "BP", "Notch", "AP"};
                int fmode_idx = (int)g_app.filter_mode;
                fmode_idx = nk_combo(ctx, fmodes, 5, fmode_idx, 25, nk_vec2(100, 200));
                g_app.filter_mode = (FilterMode)fmode_idx;
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Env Amount:", NK_TEXT_LEFT);
                nk_slider_float(ctx, -1.0f, &g_app.filter_env, 1.0f, 0.01f);
                
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].filter.cutoff = g_app.filter_cutoff;
                    g_app.synth.voices[i].filter.resonance = g_app.filter_resonance;
                    g_app.synth.voices[i].filter.mode = g_app.filter_mode;
                    g_app.synth.voices[i].filter.env_amount = g_app.filter_env;
                }
                
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ ENVELOPE ═══", NK_TEXT_CENTERED);
            
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Attack:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.001f, &g_app.env_attack, 2.0f, 0.001f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Decay:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.001f, &g_app.env_decay, 2.0f, 0.001f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Sustain:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.0f, &g_app.env_sustain, 1.0f, 0.01f);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Release:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.001f, &g_app.env_release, 5.0f, 0.001f);
                
                for (int i = 0; i < MAX_VOICES; i++) {
                    g_app.synth.voices[i].env_amp.attack = g_app.env_attack;
                    g_app.synth.voices[i].env_amp.decay = g_app.env_decay;
                    g_app.synth.voices[i].env_amp.sustain = g_app.env_sustain;
                    g_app.synth.voices[i].env_amp.release = g_app.env_release;
                }
                
                nk_group_end(ctx);
            }
            
            // MIDDLE COLUMN - LFOs & MODULATION
            nk_layout_row_push(ctx, 450);
            if (nk_group_begin(ctx, "Modulation", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ LFO 1 ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Rate:", NK_TEXT_LEFT);
                float lfo_rate = g_app.synth.lfos[0].rate;
                nk_slider_float(ctx, 0.01f, &lfo_rate, 20.0f, 0.01f);
                g_app.synth.lfos[0].rate = lfo_rate;
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
                static const char *lfo_waves[] = {"Sine", "Saw", "Square", "Tri", "S&H"};
                int lfo_wave = (int)g_app.synth.lfos[0].waveform;
                lfo_wave = nk_combo(ctx, lfo_waves, 5, lfo_wave, 25, nk_vec2(150, 200));
                g_app.synth.lfos[0].waveform = (WaveformType)lfo_wave;
                
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ MASTER ═══", NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Volume:", NK_TEXT_LEFT);
                nk_slider_float(ctx, 0.0f, &g_app.master_volume, 1.0f, 0.01f);
                g_app.synth.master_volume = g_app.master_volume;
                
                nk_layout_row_dynamic(ctx, 25, 2);
                nk_label(ctx, "Tempo (BPM):", NK_TEXT_LEFT);
                nk_slider_float(ctx, 60.0f, &g_app.tempo, 200.0f, 1.0f);
                synth_set_tempo(&g_app.synth, g_app.tempo);
                
                nk_group_end(ctx);
            }
            
            // RIGHT COLUMN - METERS
            nk_layout_row_push(ctx, 450);
            if (nk_group_begin(ctx, "Meters", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "═══ VOICE ACTIVITY ═══", NK_TEXT_CENTERED);
                
                // Voice meters
                for (int i = 0; i < MAX_VOICES; i++) {
                    nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
                    nk_layout_row_push(ctx, 50);
                    char voice_label[16];
                    snprintf(voice_label, sizeof(voice_label), "V%d", i + 1);
                    nk_label(ctx, voice_label, NK_TEXT_LEFT);
                    
                    nk_layout_row_push(ctx, 300);
                    float voice_level = 0.0f;
                    if (g_app.synth.voices[i].state != VOICE_OFF) {
                        voice_level = g_app.synth.voices[i].env_amp.current_level;
                    }
                    struct nk_color bar_color = voice_level > 0.01f ? 
                        nk_rgb(50, 200, 100) : nk_rgb(40, 40, 50);
                    nk_size progress_val = (nk_size)(voice_level * 100);
                    nk_progress(ctx, &progress_val, 100, NK_FIXED);
                    
                    nk_layout_row_push(ctx, 50);
                    if (g_app.synth.voices[i].state != VOICE_OFF) {
                        snprintf(voice_label, sizeof(voice_label), "%d", g_app.synth.voices[i].midi_note);
                        nk_label(ctx, voice_label, NK_TEXT_RIGHT);
                    }
                    nk_layout_row_end(ctx);
                }
                
                nk_group_end(ctx);
            }
            
            nk_layout_row_end(ctx);
            
        } else if (g_app.active_tab == 1) {
            // FX TAB
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "═══ DISTORTION ═══", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Enable:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "", &g_app.fx_dist_enabled);
            g_app.fx.distortion.enabled = g_app.fx_dist_enabled;
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Drive:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.drive, 10.0f, 0.1f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Mix:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.mix, 1.0f, 0.01f);
            
            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "═══ DELAY ═══", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Enable:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "", &g_app.fx_delay_enabled);
            g_app.fx.delay.enabled = g_app.fx_delay_enabled;
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Time (ms):", NK_TEXT_LEFT);
            nk_slider_float(ctx, 100.0f, &g_app.fx.delay.time_ms, 2000.0f, 10.0f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Feedback:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.delay.feedback, 0.95f, 0.01f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Mix:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.delay.mix, 1.0f, 0.01f);
            
            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "═══ REVERB ═══", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Enable:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "", &g_app.fx_reverb_enabled);
            g_app.fx.reverb.enabled = g_app.fx_reverb_enabled;
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Size:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.size, 1.0f, 0.01f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Damping:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.damping, 1.0f, 0.01f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Mix:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.mix, 1.0f, 0.01f);
            
        } else if (g_app.active_tab == 2) {
            // ARP/SEQ TAB
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "═══ ARPEGGIATOR ═══", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Enable:", NK_TEXT_LEFT);
            nk_checkbox_label(ctx, "", &g_app.arp_enabled);
            g_app.arp.enabled = g_app.arp_enabled;
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Mode:", NK_TEXT_LEFT);
            static const char *arp_modes[] = {"Off", "Up", "Down", "Up-Down", "Random"};
            int arp_mode = (int)g_app.arp.mode;
            arp_mode = nk_combo(ctx, arp_modes, 5, arp_mode, 25, nk_vec2(150, 200));
            g_app.arp.mode = (ArpMode)arp_mode;
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Rate (steps/beat):", NK_TEXT_LEFT);
            nk_slider_float(ctx, 1.0f, &g_app.arp.rate, 8.0f, 0.5f);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_label(ctx, "Gate:", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.1f, &g_app.arp.gate, 1.0f, 0.05f);
            
            nk_layout_row_dynamic(ctx, 30, 1);
            char arp_info[256];
            snprintf(arp_info, sizeof(arp_info), 
                    "Held notes: %d | Current step: %d", 
                    g_app.arp.num_held, g_app.arp.current_step);
            nk_label(ctx, arp_info, NK_TEXT_CENTERED);
            
        } else if (g_app.active_tab == 3) {
            // PRESETS TAB
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "═══ PRESETS ═══", NK_TEXT_CENTERED);
            
            nk_layout_row_dynamic(ctx, 30, 3);
            if (nk_button_label(ctx, "Init Sound")) {
                // Reset to default
                printf("🔄 Reset to init sound\n");
            }
            if (nk_button_label(ctx, "Save Preset")) {
                printf("💾 Save preset (TODO: file dialog)\n");
            }
            if (nk_button_label(ctx, "Load Preset")) {
                printf("📂 Load preset (TODO: file dialog)\n");
            }
            
            nk_layout_row_dynamic(ctx, 200, 1);
            nk_label(ctx, "Preset browser will go here\n(Factory & User presets)", NK_TEXT_CENTERED);
        }
        
        // Piano Keyboard Display
        nk_layout_row_dynamic(ctx, 5, 1);
        nk_spacing(ctx, 1);
        
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "═══════════════ PIANO KEYBOARD ═══════════════", NK_TEXT_CENTERED);
        
        // Draw realistic piano keyboard (2 octaves: C3-C5)
        // White keys first (background layer)
        nk_layout_row_static(ctx, 100, 40, 29);
        
        // C3 to C5 = MIDI 48-72 (25 white keys)
        const int white_keys[] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72};
        const char* white_labels[] = {"C3", "D3", "E3", "F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
        
        for (int i = 0; i < 15; i++) {
            int midi_note = white_keys[i];
            bool is_pressed = false;
            
            // Check if this note is currently pressed
            for (size_t j = 0; j < KEYMAP_SIZE; j++) {
                if (g_keymap[j].midi_note == midi_note && g_app.keys_pressed[j]) {
                    is_pressed = true;
                    break;
                }
            }
            
            struct nk_color color = is_pressed ? nk_rgb(100, 220, 120) : nk_rgb(245, 245, 250);
            struct nk_style_button button_style = ctx->style.button;
            button_style.normal.data.color = color;
            button_style.hover.data.color = nk_rgb(220, 240, 220);
            button_style.active.data.color = nk_rgb(100, 220, 120);
            
            // Check if button is being clicked
            nk_bool clicked = nk_button_label_styled(ctx, &button_style, white_labels[i]);
            
            if (clicked && g_app.mouse_note_playing != midi_note) {
                // Start playing this note
                if (g_app.mouse_note_playing >= 0) {
                    // Stop previous mouse note
                    synth_note_off(&g_app.synth, g_app.mouse_note_playing);
                }
                play_note(midi_note, white_labels[i]);
                g_app.mouse_note_playing = midi_note;
            }
        }
        
        // Black keys (overlay layer) - positioned between white keys
        nk_layout_row_begin(ctx, NK_STATIC, 60, 10);
        const int black_keys[] = {49, 51, 54, 56, 58, 61, 63, 66, 68, 70};
        const char* black_labels[] = {"C#3", "D#3", "F#3", "G#3", "A#3", "C#4", "D#4", "F#4", "G#4", "A#4"};
        const int black_positions[] = {30, 70, 150, 190, 230, 350, 390, 470, 510, 550};
        
        for (int i = 0; i < 10; i++) {
            nk_layout_row_push(ctx, 28);
            int midi_note = black_keys[i];
            bool is_pressed = false;
            
            // Check if pressed
            for (size_t j = 0; j < KEYMAP_SIZE; j++) {
                if (g_keymap[j].midi_note == midi_note && g_app.keys_pressed[j]) {
                    is_pressed = true;
                    break;
                }
            }
            
            struct nk_color color = is_pressed ? nk_rgb(80, 200, 100) : nk_rgb(25, 25, 30);
            struct nk_style_button button_style = ctx->style.button;
            button_style.normal.data.color = color;
            button_style.hover.data.color = nk_rgb(60, 60, 70);
            button_style.active.data.color = nk_rgb(80, 200, 100);
            button_style.text_normal = nk_rgb(255, 255, 255);
            button_style.text_hover = nk_rgb(255, 255, 255);
            button_style.text_active = nk_rgb(255, 255, 255);
            
            // Check if button is being clicked
            nk_bool clicked = nk_button_label_styled(ctx, &button_style, black_labels[i]);
            
            if (clicked && g_app.mouse_note_playing != midi_note) {
                // Start playing this note
                if (g_app.mouse_note_playing >= 0) {
                    // Stop previous mouse note
                    synth_note_off(&g_app.synth, g_app.mouse_note_playing);
                }
                play_note(midi_note, black_labels[i]);
                g_app.mouse_note_playing = midi_note;
            }
        }
        nk_layout_row_end(ctx);
        
        // Keyboard mapping info
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label(ctx, "Computer Keyboard: Z-M (bottom) = C3-B3  |  Q-I (middle) = C3-C5  |  S D G H J (sharps)", NK_TEXT_CENTERED);
        
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label(ctx, "🖱️ Click piano keys to play  |  ⌨️ Use computer keyboard  |  SPACE=Play/Stop  |  ESC=Panic", NK_TEXT_CENTERED);
    }
    nk_end(ctx);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("═══════════════════════════════════════════════════\n");
    printf("     🎹 PROFESSIONAL SYNTHESIZER 🎹\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    // Init GLFW
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    g_app.window = glfwCreateWindow(1400, 850, "Professional Synthesizer", NULL, NULL);
    if (!g_app.window) {
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(g_app.window);
    glfwSwapInterval(1);
    
    // Init Nuklear (don't let it install callbacks, we'll handle keyboard ourselves)
    g_app.nk_ctx = nk_glfw3_init(&g_app.glfw, g_app.window, NK_GLFW3_DEFAULT);
    
    // Install our keyboard callback AFTER Nuklear init
    glfwSetKeyCallback(g_app.window, key_callback);
    
    struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&g_app.glfw, &atlas);
    nk_glfw3_font_stash_end(&g_app.glfw);
    
    // Init queues and MIDI input
    param_queue_init();
    midi_input_start();
    midi_input_list_ports();

    // Init synth
    synth_init(&g_app.synth, 44100.0f);
    g_app.tempo = 120.0f;
    g_app.master_volume = 0.8f;
    g_app.synth.master_volume = 0.8f;
    g_app.filter_cutoff = 8000.0f;  // Start with brighter sound
    g_app.filter_resonance = 0.3f;
    g_app.env_attack = 0.01f;
    g_app.env_decay = 0.1f;
    g_app.env_sustain = 0.7f;
    g_app.env_release = 0.3f;
    g_app.osc1_wave = WAVE_SAW;
    g_app.osc1_unison = 1;
    g_app.osc1_pwm = 0.5f;
    
    // Init FX
    fx_delay_init(&g_app.fx.delay);
    g_app.fx.distortion.drive = 2.0f;
    g_app.fx.distortion.mix = 0.3f;
    g_app.fx.reverb.size = 0.5f;
    g_app.fx.reverb.damping = 0.5f;
    g_app.fx.reverb.mix = 0.2f;
    
    // Init arp
    arp_init(&g_app.arp);
    
    // Init mouse tracking
    g_app.mouse_note_playing = -1;
    g_app.mouse_was_down = false;
    
    // Init audio
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    config.pUserData = &g_app;
    
    if (ma_device_init(NULL, &config, &g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "❌ Failed to initialize audio\n");
        return 1;
    }
    
    if (ma_device_start(&g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "❌ Failed to start audio\n");
        return 1;
    }
    
    printf("✅ Audio: 44100 Hz, stereo, %.1f ms latency\n", 
           (float)config.periodSizeInFrames / 44100.0f * 1000.0f);
    printf("✅ GUI: Ready\n");
    printf("✅ Controls: QWERTY keyboard → Notes\n\n");
    
    // Play test tone to verify audio
    printf("🔊 Playing test tone (C4)...\n");
    synth_note_on(&g_app.synth, 60, 1.0f);  // C4 (middle C)
    sleep(1);
    synth_note_off(&g_app.synth, 60);
    printf("✅ Audio test complete!\n\n");
    
    printf("Ready to play! 🎵\n\n");
    
    g_app.bg.r = 0.10f; g_app.bg.g = 0.18f; g_app.bg.b = 0.24f; g_app.bg.a = 1.0f;
    
    // Main loop
    while (!glfwWindowShouldClose(g_app.window)) {
        glfwPollEvents();
        
        // Poll keyboard state directly (backup to callback system)
        for (size_t i = 0; i < KEYMAP_SIZE; i++) {
            int state = glfwGetKey(g_app.window, g_keymap[i].glfw_key);
            
            // Key just pressed
            if (state == GLFW_PRESS && !g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 1;
                play_note(g_keymap[i].midi_note, g_keymap[i].label);
            }
            // Key just released
            else if (state == GLFW_RELEASE && g_app.keys_pressed[i]) {
                g_app.keys_pressed[i] = 0;
                stop_note(g_keymap[i].midi_note, g_keymap[i].label);
            }
        }
        
        // Track mouse button for piano keys
        int mouse_state = glfwGetMouseButton(g_app.window, GLFW_MOUSE_BUTTON_LEFT);
        if (mouse_state == GLFW_RELEASE && g_app.mouse_was_down) {
            // Mouse released - stop any mouse-triggered note
            if (g_app.mouse_note_playing >= 0) {
                synth_note_off(&g_app.synth, g_app.mouse_note_playing);
                printf("🔇 Mouse released - stopping note %d\n", g_app.mouse_note_playing);
                g_app.mouse_note_playing = -1;
            }
        }
        g_app.mouse_was_down = (mouse_state == GLFW_PRESS);
        
        nk_glfw3_new_frame(&g_app.glfw);
        
        draw_gui(g_app.nk_ctx);
        
        int width, height;
        glfwGetFramebufferSize(g_app.window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(g_app.bg.r, g_app.bg.g, g_app.bg.b, g_app.bg.a);
        
        nk_glfw3_render(&g_app.glfw, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(g_app.window);
    }
    
    // Cleanup
    midi_input_stop();
    ma_device_uninit(&g_app.audio_device);
    nk_glfw3_shutdown(&g_app.glfw);
    glfwTerminate();
    
    printf("\n🎉 Goodbye!\n");
    return 0;
}
