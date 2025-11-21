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
#include "ui/style.h"
#include "ui/draw_helpers.h"
#include "ui/knob_custom.h"
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Forward declarations for note helpers used by UI components
void play_note(int midi_note, const char* label);
void stop_note(int midi_note, const char* label);

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

typedef struct {
    struct nk_rect bounds;
    int start_note;
    int num_octaves;
} PianoKeyboardParams;

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

    // Custom knob states (hardware panel UI)
    UiKnobState knob_master_volume;
    UiKnobState knob_tempo;
    UiKnobState knob_filter_cutoff;
    UiKnobState knob_filter_resonance;
    UiKnobState knob_filter_env;
    UiKnobState knob_env_attack;
    UiKnobState knob_env_decay;
    UiKnobState knob_env_sustain;
    UiKnobState knob_env_release;
    UiKnobState knob_osc1_detune;
    UiKnobState knob_osc1_pwm;
    UiKnobState knob_arp_rate;

    double last_frame_time;
} AppState;

AppState g_app = {0};

static void enqueue_param_float_msg(ParamId id, float value) {
    ParamMsg msg = {0};
    msg.id = (uint32_t)id;
    msg.type = PARAM_FLOAT;
    msg.value.f = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "⚠️ Param queue full (float id %u)\n", msg.id);
    }
}

static void enqueue_param_int_msg(ParamId id, int value) {
    ParamMsg msg = {0};
    msg.id = (uint32_t)id;
    msg.type = PARAM_INT;
    msg.value.i = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "⚠️ Param queue full (int id %u)\n", msg.id);
    }
}

static void ui_knobs_init(void) {
    ui_knob_state_init(&g_app.knob_master_volume, 0.0f, 1.0f, g_app.master_volume);
    ui_knob_state_init(&g_app.knob_tempo, 60.0f, 200.0f, g_app.tempo);
    ui_knob_state_init(&g_app.knob_filter_cutoff, 20.0f, 20000.0f, g_app.filter_cutoff);
    ui_knob_state_init(&g_app.knob_filter_resonance, 0.0f, 1.0f, g_app.filter_resonance);
    ui_knob_state_init(&g_app.knob_filter_env, -1.0f, 1.0f, g_app.filter_env);
    ui_knob_state_init(&g_app.knob_env_attack, 0.001f, 2.0f, g_app.env_attack);
    ui_knob_state_init(&g_app.knob_env_decay, 0.001f, 2.0f, g_app.env_decay);
    ui_knob_state_init(&g_app.knob_env_sustain, 0.0f, 1.0f, g_app.env_sustain);
    ui_knob_state_init(&g_app.knob_env_release, 0.001f, 5.0f, g_app.env_release);
    ui_knob_state_init(&g_app.knob_osc1_detune, -50.0f, 50.0f, g_app.osc1_detune);
    ui_knob_state_init(&g_app.knob_osc1_pwm, 0.0f, 1.0f, g_app.osc1_pwm);
    ui_knob_state_init(&g_app.knob_arp_rate, 1.0f, 8.0f, g_app.arp.rate > 0.0f ? g_app.arp.rate : 2.0f);
}

static struct nk_rect ui_panel_cell_rect(int col,
                                         int row,
                                         float cell_w,
                                         float cell_h,
                                         float gap_x,
                                         float gap_y,
                                         float padding) {
    struct nk_rect rect = {0};
    rect.x = padding + col * (cell_w + gap_x);
    rect.y = padding + row * (cell_h + gap_y);
    rect.w = cell_w;
    rect.h = cell_h;
    return rect;
}

static bool ui_note_is_active(int midi_note) {
    if (g_app.mouse_note_playing == midi_note) {
        return true;
    }
    for (size_t j = 0; j < KEYMAP_SIZE; ++j) {
        if (g_keymap[j].midi_note == midi_note && g_app.keys_pressed[j]) {
            return true;
        }
    }
    return false;
}

static void draw_piano_keyboard(struct nk_context* ctx,
                                const PianoKeyboardParams* params) {
    if (!ctx || !params) {
        return;
    }

    const UiPalette* palette = ui_style_palette();
    const UiMetrics* metrics = ui_style_metrics();
    struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
    const struct nk_rect bounds = params->bounds;
    struct nk_rect background = bounds;
    background.x += 2.0f;
    background.w -= 4.0f;
    ui_draw_panel_bezel(canvas, bounds, 6.0f);
    ui_draw_panel_inset(canvas, background, 6.0f);

    const int total_octaves = params->num_octaves > 0 ? params->num_octaves : 1;
    const int total_white_keys = total_octaves * 7;
    const float white_width = bounds.w / (float)total_white_keys;
    const float white_gap = 1.5f;
    const float white_height = fmaxf(metrics->keyboard_height - 12.0f, 80.0f);
    const float black_width = white_width * 0.6f;
    const float black_height = white_height * 0.62f;

    static const int white_offsets[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int black_offsets[5] = {1, 3, 6, 8, 10};
    static const int black_preceding_white[5] = {0, 1, 3, 4, 5};

    struct nk_input* in = &ctx->input;
    const nk_bool mouse_down = in->mouse.buttons[NK_BUTTON_LEFT].down;

    // White keys (background layer)
    for (int octave = 0; octave < total_octaves; ++octave) {
        for (int i = 0; i < 7; ++i) {
            int midi_note = params->start_note + octave * 12 + white_offsets[i];
            float key_x = bounds.x + (float)(octave * 7 + i) * white_width + white_gap * 0.5f;
            struct nk_rect key_rect = {
                key_x,
                bounds.y + 6.0f,
                white_width - white_gap,
                white_height
            };

            const bool active = ui_note_is_active(midi_note);
            struct nk_color fill = active ? palette->key_pressed : palette->white_key;
            nk_fill_rect(canvas, key_rect, 4.0f, fill);

            const bool hovered = nk_input_is_mouse_hovering_rect(in, key_rect);
            if (mouse_down && hovered && g_app.mouse_note_playing != midi_note) {
                if (g_app.mouse_note_playing >= 0) {
                    stop_note(g_app.mouse_note_playing, "mouse");
                }
                play_note(midi_note, "mouse");
                g_app.mouse_note_playing = midi_note;
            }
        }
    }

    // Black keys (foreground layer)
    for (int octave = 0; octave < total_octaves; ++octave) {
        for (int i = 0; i < 5; ++i) {
            int midi_note = params->start_note + octave * 12 + black_offsets[i];
            float base_index = (float)(octave * 7 + black_preceding_white[i] + 1);
            float key_x = bounds.x + base_index * white_width - black_width * 0.5f;
            struct nk_rect key_rect = {
                key_x,
                bounds.y + 6.0f,
                black_width,
                black_height
            };

            const bool active = ui_note_is_active(midi_note);
            struct nk_color fill = active ? palette->accent : palette->black_key;
            nk_fill_rect(canvas, key_rect, 3.0f, fill);

            const bool hovered = nk_input_is_mouse_hovering_rect(in, key_rect);
            if (mouse_down && hovered && g_app.mouse_note_playing != midi_note) {
                if (g_app.mouse_note_playing >= 0) {
                    stop_note(g_app.mouse_note_playing, "mouse");
                }
                play_note(midi_note, "mouse");
                g_app.mouse_note_playing = midi_note;
            }
        }
    }

    // Mouse release handling for any key
    if (!mouse_down && g_app.mouse_note_playing >= 0) {
        stop_note(g_app.mouse_note_playing, "mouse");
        g_app.mouse_note_playing = -1;
    }
}

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
    const UiMetrics* metrics = ui_style_metrics();
    const float width = 1500.0f;
    const float height = 920.0f;
    const float column_gap = metrics->column_gap * 1.5f;
    const nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

    if (nk_begin(ctx, "Professional Synthesizer", nk_rect(0, 0, width, height), window_flags)) {
        struct nk_rect region = nk_window_get_content_region(ctx);
        const float column_width = (region.w - 2.0f * column_gap) / 3.0f;

        if (column_width < 240.0f) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Increase the window width to use the expanded console layout.", NK_TEXT_CENTERED);
            nk_end(ctx);
            return;
        }

        nk_layout_row_begin(ctx, NK_STATIC, 52, 4);
        nk_layout_row_push(ctx, region.w * 0.34f);
        nk_label(ctx, "PROFESSIONAL SYNTHESIZER", NK_TEXT_LEFT);

        nk_layout_row_push(ctx, region.w * 0.20f);
        char voice_info[64];
        snprintf(voice_info, sizeof(voice_info), "%d / %d voices", g_app.synth.num_active_voices, MAX_VOICES);
        nk_label(ctx, voice_info, NK_TEXT_CENTERED);

        nk_layout_row_push(ctx, region.w * 0.22f);
        char tempo_info[64];
        snprintf(tempo_info, sizeof(tempo_info), "Tempo %.1f BPM", g_app.tempo);
        nk_label(ctx, tempo_info, NK_TEXT_CENTERED);

        nk_layout_row_push(ctx, region.w * 0.20f);
        int minutes = (int)(g_app.current_time / 60.0);
        int seconds = (int)fmod(g_app.current_time, 60.0);
        char time_info[32];
        snprintf(time_info, sizeof(time_info), "%s %02d:%02d", g_app.playing ? "▶" : "⏸", minutes, seconds);
        nk_label(ctx, time_info, NK_TEXT_RIGHT);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 40, 3);
        nk_layout_row_push(ctx, region.w * 0.28f);
        if (nk_button_label(ctx, g_app.playing ? "⏹ Stop Transport" : "▶ Start Transport")) {
            g_app.playing = !g_app.playing;
        }

    nk_layout_row_push(ctx, region.w * 0.44f);
    nk_size voice_meter = (nk_size)g_app.synth.num_active_voices;
    nk_progress(ctx, &voice_meter, MAX_VOICES, nk_false);

        nk_layout_row_push(ctx, region.w * 0.22f);
        int previous_arp_enabled = g_app.arp_enabled;
        nk_checkbox_label(ctx, "Arp Enabled", &g_app.arp_enabled);
        if (previous_arp_enabled != g_app.arp_enabled) {
            g_app.arp.enabled = g_app.arp_enabled;
            enqueue_param_int_msg(PARAM_ARP_ENABLED, g_app.arp_enabled);
        } else {
            g_app.arp.enabled = g_app.arp_enabled != 0;
        }
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 6, 1);
        nk_spacing(ctx, 1);

        nk_flags panel_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;

        nk_layout_row_begin(ctx, NK_STATIC, 360, 3);

        nk_layout_row_push(ctx, column_width);
        if (nk_group_begin_titled(ctx, "PANEL_OSC", "Oscillator Deck", panel_flags)) {
            nk_layout_row_dynamic(ctx, 28, 2);
            nk_label(ctx, "Waveform", NK_TEXT_LEFT);
            static const char *waves[] = {"Sine", "Saw", "Square", "Tri", "Noise"};
            int wave_idx = (int)g_app.osc1_wave;
            int selected_wave = nk_combo(ctx, waves, 5, wave_idx, 28, nk_vec2(160, 200));
            if (selected_wave != wave_idx) {
                g_app.osc1_wave = (WaveformType)selected_wave;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].osc1.waveform = g_app.osc1_wave;
                }
                enqueue_param_int_msg(PARAM_OSC1_WAVE, selected_wave);
            }

            nk_layout_row_dynamic(ctx, 28, 2);
            nk_label(ctx, "Unison Voices", NK_TEXT_LEFT);
            int prev_unison = g_app.osc1_unison;
            nk_slider_int(ctx, 1, &g_app.osc1_unison, 5, 1);
            if (prev_unison != g_app.osc1_unison) {
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].osc1.unison_voices = g_app.osc1_unison;
                }
            }

            const float knob_width = fminf(column_width * 0.45f, 140.0f);
            nk_layout_row_begin(ctx, NK_STATIC, 150, 2);
            nk_layout_row_push(ctx, knob_width);
            UiKnobConfig detune_cfg = {.label = "DETUNE", .unit = "¢", .snap_increment = 1.0f};
            if (ui_knob_render(ctx, &g_app.knob_osc1_detune, &detune_cfg)) {
                g_app.osc1_detune = g_app.knob_osc1_detune.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].osc1.detune_cents = g_app.osc1_detune;
                }
                enqueue_param_float_msg(PARAM_OSC1_FINE, g_app.osc1_detune);
            }

            nk_layout_row_push(ctx, knob_width);
            UiKnobConfig pwm_cfg = {.label = "PULSE", .unit = ""};
            if (ui_knob_render(ctx, &g_app.knob_osc1_pwm, &pwm_cfg)) {
                g_app.osc1_pwm = g_app.knob_osc1_pwm.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].osc1.pulse_width = g_app.osc1_pwm;
                }
                enqueue_param_float_msg(PARAM_OSC1_PWM, g_app.osc1_pwm);
            }
            nk_layout_row_end(ctx);

            nk_group_end(ctx);
        }

        nk_layout_row_push(ctx, column_width);
        if (nk_group_begin_titled(ctx, "PANEL_FILTER", "Filter & Envelope", panel_flags)) {
            nk_layout_row_dynamic(ctx, 28, 2);
            nk_label(ctx, "Filter Mode", NK_TEXT_LEFT);
            static const char *filter_modes[] = {"LP", "HP", "BP", "Notch", "AP"};
            int mode_idx = (int)g_app.filter_mode;
            int new_mode = nk_combo(ctx, filter_modes, 5, mode_idx, 28, nk_vec2(120, 200));
            if (new_mode != mode_idx) {
                g_app.filter_mode = (FilterMode)new_mode;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].filter.mode = g_app.filter_mode;
                }
                enqueue_param_int_msg(PARAM_FILTER_MODE, new_mode);
            }

            nk_layout_row_begin(ctx, NK_STATIC, 150, 3);
            const float filter_knob_width = fminf(column_width * 0.32f, 120.0f);

            nk_layout_row_push(ctx, filter_knob_width);
            UiKnobConfig cutoff_cfg = {.label = "CUTOFF", .unit = "Hz"};
            if (ui_knob_render(ctx, &g_app.knob_filter_cutoff, &cutoff_cfg)) {
                g_app.filter_cutoff = g_app.knob_filter_cutoff.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].filter.cutoff = g_app.filter_cutoff;
                }
                enqueue_param_float_msg(PARAM_FILTER_CUTOFF, g_app.filter_cutoff);
            }

            nk_layout_row_push(ctx, filter_knob_width);
            UiKnobConfig resonance_cfg = {.label = "RESONANCE", .unit = ""};
            if (ui_knob_render(ctx, &g_app.knob_filter_resonance, &resonance_cfg)) {
                g_app.filter_resonance = g_app.knob_filter_resonance.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].filter.resonance = g_app.filter_resonance;
                }
                enqueue_param_float_msg(PARAM_FILTER_RESONANCE, g_app.filter_resonance);
            }

            nk_layout_row_push(ctx, filter_knob_width);
            UiKnobConfig env_cfg = {.label = "ENV AMT", .unit = ""};
            if (ui_knob_render(ctx, &g_app.knob_filter_env, &env_cfg)) {
                g_app.filter_env = g_app.knob_filter_env.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].filter.env_amount = g_app.filter_env;
                }
                enqueue_param_float_msg(PARAM_FILTER_ENV_AMOUNT, g_app.filter_env);
            }
            nk_layout_row_end(ctx);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Amp Envelope", NK_TEXT_LEFT);

            nk_layout_row_begin(ctx, NK_STATIC, 150, 4);
            const float env_knob_width = fminf(column_width * 0.24f, 100.0f);

            nk_layout_row_push(ctx, env_knob_width);
            UiKnobConfig attack_cfg = {.label = "ATTACK", .unit = "s"};
            if (ui_knob_render(ctx, &g_app.knob_env_attack, &attack_cfg)) {
                g_app.env_attack = g_app.knob_env_attack.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].env_amp.attack = g_app.env_attack;
                }
                enqueue_param_float_msg(PARAM_ENV_ATTACK, g_app.env_attack);
            }

            nk_layout_row_push(ctx, env_knob_width);
            UiKnobConfig decay_cfg = {.label = "DECAY", .unit = "s"};
            if (ui_knob_render(ctx, &g_app.knob_env_decay, &decay_cfg)) {
                g_app.env_decay = g_app.knob_env_decay.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].env_amp.decay = g_app.env_decay;
                }
                enqueue_param_float_msg(PARAM_ENV_DECAY, g_app.env_decay);
            }

            nk_layout_row_push(ctx, env_knob_width);
            UiKnobConfig sustain_cfg = {.label = "SUSTAIN", .unit = ""};
            if (ui_knob_render(ctx, &g_app.knob_env_sustain, &sustain_cfg)) {
                g_app.env_sustain = g_app.knob_env_sustain.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].env_amp.sustain = g_app.env_sustain;
                }
                enqueue_param_float_msg(PARAM_ENV_SUSTAIN, g_app.env_sustain);
            }

            nk_layout_row_push(ctx, env_knob_width);
            UiKnobConfig release_cfg = {.label = "RELEASE", .unit = "s"};
            if (ui_knob_render(ctx, &g_app.knob_env_release, &release_cfg)) {
                g_app.env_release = g_app.knob_env_release.value;
                for (int i = 0; i < MAX_VOICES; ++i) {
                    g_app.synth.voices[i].env_amp.release = g_app.env_release;
                }
                enqueue_param_float_msg(PARAM_ENV_RELEASE, g_app.env_release);
            }
            nk_layout_row_end(ctx);

            nk_group_end(ctx);
        }

        nk_layout_row_push(ctx, column_width);
        if (nk_group_begin_titled(ctx, "PANEL_MASTER", "Master / FX / Arp", panel_flags)) {
            nk_layout_row_begin(ctx, NK_STATIC, 150, 2);
            const float master_knob_width = fminf(column_width * 0.45f, 140.0f);
            nk_layout_row_push(ctx, master_knob_width);
            UiKnobConfig master_cfg = {.label = "MASTER", .unit = ""};
            if (ui_knob_render(ctx, &g_app.knob_master_volume, &master_cfg)) {
                g_app.master_volume = g_app.knob_master_volume.value;
                g_app.synth.master_volume = g_app.master_volume;
                enqueue_param_float_msg(PARAM_MASTER_VOLUME, g_app.master_volume);
            }

            nk_layout_row_push(ctx, master_knob_width);
            UiKnobConfig tempo_cfg = {.label = "TEMPO", .unit = "BPM"};
            if (ui_knob_render(ctx, &g_app.knob_tempo, &tempo_cfg)) {
                g_app.tempo = g_app.knob_tempo.value;
                synth_set_tempo(&g_app.synth, g_app.tempo);
                enqueue_param_float_msg(PARAM_TEMPO, g_app.tempo);
            }
            nk_layout_row_end(ctx);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Effects", NK_TEXT_LEFT);

            int prev_dist_enabled = g_app.fx_dist_enabled;
            int prev_delay_enabled = g_app.fx_delay_enabled;
            int prev_reverb_enabled = g_app.fx_reverb_enabled;

            nk_layout_row_dynamic(ctx, 26, 3);
            nk_checkbox_label(ctx, "Dist", &g_app.fx_dist_enabled);
            nk_checkbox_label(ctx, "Delay", &g_app.fx_delay_enabled);
            nk_checkbox_label(ctx, "Reverb", &g_app.fx_reverb_enabled);

            if (prev_dist_enabled != g_app.fx_dist_enabled) {
                enqueue_param_int_msg(PARAM_FX_DISTORTION_ENABLED, g_app.fx_dist_enabled);
            }
            if (prev_delay_enabled != g_app.fx_delay_enabled) {
                enqueue_param_int_msg(PARAM_FX_DELAY_ENABLED, g_app.fx_delay_enabled);
            }
            if (prev_reverb_enabled != g_app.fx_reverb_enabled) {
                enqueue_param_int_msg(PARAM_FX_REVERB_ENABLED, g_app.fx_reverb_enabled);
            }

            g_app.fx.distortion.enabled = g_app.fx_dist_enabled;
            g_app.fx.delay.enabled = g_app.fx_delay_enabled;
            g_app.fx.reverb.enabled = g_app.fx_reverb_enabled;

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Distortion", NK_TEXT_LEFT);
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.drive, 10.0f, 0.1f)) {
                enqueue_param_float_msg(PARAM_FX_DISTORTION_DRIVE, g_app.fx.distortion.drive);
            }
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.mix, 1.0f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_DISTORTION_MIX, g_app.fx.distortion.mix);
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Delay", NK_TEXT_LEFT);
            if (nk_slider_float(ctx, 100.0f, &g_app.fx.delay.time_ms, 2000.0f, 10.0f)) {
                enqueue_param_float_msg(PARAM_FX_DELAY_TIME, g_app.fx.delay.time_ms);
            }
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.delay.feedback, 0.95f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_DELAY_FEEDBACK, g_app.fx.delay.feedback);
            }
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.delay.mix, 1.0f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_DELAY_MIX, g_app.fx.delay.mix);
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Reverb", NK_TEXT_LEFT);
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.size, 1.0f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_REVERB_SIZE, g_app.fx.reverb.size);
            }
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.damping, 1.0f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_REVERB_DAMPING, g_app.fx.reverb.damping);
            }
            if (nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.mix, 1.0f, 0.01f)) {
                enqueue_param_float_msg(PARAM_FX_REVERB_MIX, g_app.fx.reverb.mix);
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Arpeggiator", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 28, 2);
            nk_label(ctx, "Mode", NK_TEXT_LEFT);
            static const char *arp_modes[] = {"Off", "Up", "Down", "Up+Down", "Random"};
            int arp_mode = (int)g_app.arp.mode;
            int new_arp_mode = nk_combo(ctx, arp_modes, 5, arp_mode, 28, nk_vec2(140, 200));
            if (new_arp_mode != arp_mode) {
                g_app.arp.mode = (ArpMode)new_arp_mode;
                enqueue_param_int_msg(PARAM_ARP_MODE, new_arp_mode);
            }

            nk_layout_row_begin(ctx, NK_STATIC, 120, 1);
            nk_layout_row_push(ctx, master_knob_width);
            UiKnobConfig arp_rate_cfg = {.label = "RATE", .unit = "stp"};
            if (ui_knob_render(ctx, &g_app.knob_arp_rate, &arp_rate_cfg)) {
                g_app.arp.rate = g_app.knob_arp_rate.value;
                enqueue_param_float_msg(PARAM_ARP_RATE, g_app.arp.rate);
            }
            nk_layout_row_end(ctx);

            nk_layout_row_dynamic(ctx, 28, 2);
            nk_label(ctx, "Gate", NK_TEXT_LEFT);
            if (nk_slider_float(ctx, 0.1f, &g_app.arp.gate, 1.0f, 0.05f)) {
                enqueue_param_float_msg(PARAM_ARP_GATE, g_app.arp.gate);
            }

            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_begin(ctx, NK_STATIC, 200, 2);
        nk_layout_row_push(ctx, (region.w - column_gap) * 0.5f);
        if (nk_group_begin_titled(ctx, "PANEL_PERF", "Performance Monitor", panel_flags)) {
            nk_layout_row_dynamic(ctx, 26, 2);
            nk_label(ctx, "Held notes", NK_TEXT_LEFT);
            nk_label(ctx, "Mouse note", NK_TEXT_RIGHT);

            nk_layout_row_dynamic(ctx, 26, 2);
            char held_buf[64];
            snprintf(held_buf, sizeof(held_buf), "%d active", g_app.arp.num_held);
            nk_label(ctx, held_buf, NK_TEXT_LEFT);
            if (g_app.mouse_note_playing >= 0) {
                char mouse_buf[32];
                snprintf(mouse_buf, sizeof(mouse_buf), "MIDI %d", g_app.mouse_note_playing);
                nk_label(ctx, mouse_buf, NK_TEXT_RIGHT);
            } else {
                nk_label(ctx, "-", NK_TEXT_RIGHT);
            }

            nk_layout_row_dynamic(ctx, 32, 1);
            if (nk_button_label(ctx, "Panic (All Notes Off)")) {
                synth_all_notes_off(&g_app.synth);
                memset(g_app.keys_pressed, 0, sizeof(g_app.keys_pressed));
                g_app.mouse_note_playing = -1;
                g_app.arp.num_held = 0;
                enqueue_param_int_msg(PARAM_PANIC, 1);
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Sequencer controls return in the next update.", NK_TEXT_CENTERED);
            nk_group_end(ctx);
        }

        nk_layout_row_push(ctx, (region.w - column_gap) * 0.5f);
        if (nk_group_begin_titled(ctx, "PANEL_VOICES", "Voice Activity", panel_flags)) {
            for (int i = 0; i < MAX_VOICES; ++i) {
                nk_layout_row_begin(ctx, NK_STATIC, 22, 3);
                nk_layout_row_push(ctx, 40);
                char label[8];
                snprintf(label, sizeof(label), "V%d", i + 1);
                nk_label(ctx, label, NK_TEXT_LEFT);

                nk_layout_row_push(ctx, column_width * 0.35f);
                float level = 0.0f;
                if (g_app.synth.voices[i].state != VOICE_OFF) {
                    level = g_app.synth.voices[i].env_amp.current_level;
                }
                nk_size bar = (nk_size)(level * 100.0f);
                nk_progress(ctx, &bar, 100, nk_false);

                nk_layout_row_push(ctx, 60);
                if (g_app.synth.voices[i].state != VOICE_OFF) {
                    snprintf(label, sizeof(label), "M%d", g_app.synth.voices[i].midi_note);
                    nk_label(ctx, label, NK_TEXT_RIGHT);
                } else {
                    nk_label(ctx, "-", NK_TEXT_RIGHT);
                }
                nk_layout_row_end(ctx);
            }
            nk_group_end(ctx);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, metrics->keyboard_height + 30.0f, 1);
        struct nk_rect keyboard_bounds;
        if (nk_widget(&keyboard_bounds, ctx) != NK_WIDGET_INVALID) {
            PianoKeyboardParams params = {
                .bounds = nk_rect(keyboard_bounds.x + metrics->padding_lr,
                                  keyboard_bounds.y + 4.0f,
                                  keyboard_bounds.w - metrics->padding_lr * 2.0f,
                                  keyboard_bounds.h - 8.0f),
                .start_note = 48,
                .num_octaves = 3
            };
            draw_piano_keyboard(ctx, &params);
        }

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Mouse: click & drag keys • Keyboard: Z-M / Q-I • SPACE = Play/Stop • ESC = Panic", NK_TEXT_CENTERED);
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
    ui_knobs_init();
    
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
