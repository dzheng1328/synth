/**
 * PROFESSIONAL SYNTHESIZER - Complete Implementation
 * 
 * A full-featured software synthesizer with:
 * - Computer keyboard input (QWERTY â†’ MIDI)
 * - Professional DSP engine (8-voice polyphony)
 * - Effects rack (7 effects)
 * - Arpeggiator & step sequencer
 * - Preset management
 * - Real-time GUI
 */

#define GL_SILENCE_DEPRECATION

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include <glad/gl.h>
#include <GLFW/glfw3.h>

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
#include "sample_io.h"
#include "preset.h"
#include "third_party/cjson/cJSON.h"

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// Forward declarations for note helpers used by UI components
void play_note(int midi_note, const char* label);
void stop_note(int midi_note, const char* label);

#if defined(_WIN32)
static void sleep_milliseconds(unsigned int ms) {
    Sleep(ms);
}
#else
static void sleep_milliseconds(unsigned int ms) {
    usleep(ms * 1000U);
}
#endif

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

#define UI_MACRO_COUNT 4
#define UI_MOD_SLOT_COUNT 4

typedef struct {
    int source;
    int destination;
    float amount;
    int enabled;
} ModMatrixSlot;

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

#define MAX_VOICE_TRACKS 4
#define VOICE_TRACK_MAX_SECONDS 60

#define MAX_PRESET_SNIPPET_PRESETS 32
#define MAX_SNIPPETS_PER_PRESET 3
#define PRESET_SNIPPET_MAX_SECONDS 30

typedef struct {
    SampleBuffer buffer;
    uint32_t record_capacity_frames;
    uint32_t recorded_frames;
    uint32_t playback_pos;
    int recording;
    int playing;
    float volume;
    char last_saved_path[260];
} VoiceTrack;

typedef struct {
    VoiceTrack tracks[MAX_VOICE_TRACKS];
    char recordings_dir[260];
} VoiceLayerRack;

typedef struct {
    SampleBuffer buffer;
    uint32_t record_capacity_frames;
    uint32_t recorded_frames;
    uint32_t playback_pos;
    int recording;
    int playing;
    float captured_tempo;
    char relative_path[260];
} PresetSnippet;

typedef struct {
    char preset_name[64];
    PresetSnippet snippets[MAX_SNIPPETS_PER_PRESET];
    int in_use;
    float average_tempo;
} PresetSnippetEntry;

typedef struct {
    PresetSnippetEntry entries[MAX_PRESET_SNIPPET_PRESETS];
    PresetSnippet* active_recording;
    PresetSnippet* active_playback;
    int active_entry_index;
    int active_snippet_index;
    int play_all_active;
    int play_all_entry_index;
    int play_all_snippet_index;
    int pending_save_ready;
    int pending_save_entry_index;
    int pending_save_snippet_index;
    char base_dir[260];
    char manifest_path[260];
} PresetSnippetLibrary;

typedef struct {
    SynthEngine synth;
    EffectsRack fx;
    Arpeggiator arp;
    Sequencer sequencer;
    VoiceLayerRack voice_layers;
    PresetSnippetLibrary preset_snippets;
    
    ma_device audio_device;
    ma_mutex audio_mutex;
    int capture_channels;
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
    
    // Layout & navigation state
    int active_scene;
    int scenes_linked;
    int ui_section;
    int patch_category;
    float scene_blend;
    char patch_name[64];
    char browser_search[64];
    int browser_selection;
    float macro_values[UI_MACRO_COUNT];
    ModMatrixSlot mod_slots[UI_MOD_SLOT_COUNT];
    
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
    UiKnobState knob_macro[UI_MACRO_COUNT];

    double last_frame_time;
} AppState;

AppState g_app = {0};

static void audio_state_lock(void) {
    ma_mutex_lock(&g_app.audio_mutex);
}

static void audio_state_unlock(void) {
    ma_mutex_unlock(&g_app.audio_mutex);
}

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

static void ensure_directory_exists(const char* path) {
    if (!path || !path[0]) {
        return;
    }
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void voice_layers_init(VoiceLayerRack* rack) {
    if (!rack) {
        return;
    }
    memset(rack, 0, sizeof(VoiceLayerRack));
    for (int i = 0; i < MAX_VOICE_TRACKS; ++i) {
        sample_buffer_init(&rack->tracks[i].buffer);
        rack->tracks[i].volume = 1.0f;
    }
    snprintf(rack->recordings_dir, sizeof(rack->recordings_dir), "recordings");
}

static void voice_track_finalize_recording(VoiceTrack* track) {
    if (!track || !track->recording) {
        return;
    }
    track->recording = 0;
    track->buffer.frame_count = track->recorded_frames;
    if (track->buffer.frame_count == 0) {
        sample_buffer_free(&track->buffer);
    }
    track->playback_pos = 0;
}

static void voice_track_begin_recording(int index) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return;
    }
    audio_state_lock();
    for (int i = 0; i < MAX_VOICE_TRACKS; ++i) {
        voice_track_finalize_recording(&g_app.voice_layers.tracks[i]);
    }

    VoiceTrack* track = &g_app.voice_layers.tracks[index];
    sample_buffer_free(&track->buffer);

    uint32_t sample_rate = (uint32_t)(g_app.synth.sample_rate > 0.0f ? g_app.synth.sample_rate : 44100.0f);
    uint32_t capacity = sample_rate * VOICE_TRACK_MAX_SECONDS;
    size_t samples = (size_t)capacity * 2;
    track->buffer.data = (float*)malloc(samples * sizeof(float));
    if (!track->buffer.data) {
        track->recording = 0;
        audio_state_unlock();
        fprintf(stderr, "âŒ Unable to allocate recording buffer for track %d\n", index + 1);
        return;
    }
    track->buffer.channels = 2;
    track->buffer.sample_rate = sample_rate;
    track->buffer.frame_count = 0;
    track->record_capacity_frames = capacity;
    track->recorded_frames = 0;
    track->playback_pos = 0;
    track->playing = 0;
    track->recording = 1;
    track->last_saved_path[0] = '\0';
    audio_state_unlock();
}

static void voice_track_stop_recording(int index) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return;
    }
    audio_state_lock();
    voice_track_finalize_recording(&g_app.voice_layers.tracks[index]);
    audio_state_unlock();
}

static void voice_track_clear(int index) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return;
    }
    audio_state_lock();
    VoiceTrack* track = &g_app.voice_layers.tracks[index];
    sample_buffer_free(&track->buffer);
    track->record_capacity_frames = 0;
    track->recorded_frames = 0;
    track->playback_pos = 0;
    track->recording = 0;
    track->playing = 0;
    track->last_saved_path[0] = '\0';
    audio_state_unlock();
}

static void voice_track_toggle_play(int index) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return;
    }
    audio_state_lock();
    VoiceTrack* track = &g_app.voice_layers.tracks[index];
    if (!track->buffer.data || track->buffer.frame_count == 0) {
        track->playing = 0;
        audio_state_unlock();
        return;
    }
    track->playing = !track->playing;
    if (track->playing) {
        track->playback_pos = 0;
    }
    audio_state_unlock();
}

static void voice_track_set_volume(int index, float volume) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return;
    }
    audio_state_lock();
    g_app.voice_layers.tracks[index].volume = fmaxf(0.0f, fminf(volume, 2.0f));
    audio_state_unlock();
}

static bool voice_track_save_to_disk(int index) {
    if (index < 0 || index >= MAX_VOICE_TRACKS) {
        return false;
    }

    float* copy = NULL;
    uint32_t frame_count = 0;
    uint32_t channels = 0;
    uint32_t sample_rate = 0;

    audio_state_lock();
    VoiceTrack* track = &g_app.voice_layers.tracks[index];
    if (track->buffer.data && track->buffer.frame_count > 0) {
        frame_count = track->buffer.frame_count;
        channels = track->buffer.channels;
        sample_rate = track->buffer.sample_rate;
        size_t samples = (size_t)frame_count * channels;
        copy = (float*)malloc(samples * sizeof(float));
        if (copy) {
            memcpy(copy, track->buffer.data, samples * sizeof(float));
        }
    }
    audio_state_unlock();

    if (!copy) {
        return false;
    }

    ensure_directory_exists(g_app.voice_layers.recordings_dir);

    char filepath[260];
    snprintf(filepath, sizeof(filepath), "%s/voice_track_%d.wav",
             g_app.voice_layers.recordings_dir, index + 1);
    bool ok = sample_buffer_write_wav(filepath, copy, frame_count, channels, sample_rate);
    free(copy);

    if (ok) {
        audio_state_lock();
        VoiceTrack* track = &g_app.voice_layers.tracks[index];
        strncpy(track->last_saved_path, filepath, sizeof(track->last_saved_path) - 1);
        track->last_saved_path[sizeof(track->last_saved_path) - 1] = '\0';
        audio_state_unlock();
        printf("ðŸ’¾ Saved voice track %d to %s\n", index + 1, filepath);
    } else {
        fprintf(stderr, "âŒ Failed to save voice track %d to %s\n", index + 1, filepath);
    }

    return ok;
}

// ============================================================================
// APPLICATION STATE
// ============================================================================

static void enqueue_param_float_msg(ParamId id, float value) {
    ParamMsg msg = {0};
    msg.id = (uint32_t)id;
    msg.type = PARAM_FLOAT;
    msg.value.f = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "âš ï¸ Param queue full (float id %u)\n", msg.id);
    }
}

static void enqueue_param_int_msg(ParamId id, int value) {
    ParamMsg msg = {0};
    msg.id = (uint32_t)id;
    msg.type = PARAM_INT;
    msg.value.i = value;
    if (!param_queue_enqueue(&msg)) {
        fprintf(stderr, "âš ï¸ Param queue full (int id %u)\n", msg.id);
    }
}

static void app_apply_tempo(float bpm) {
    if (bpm <= 0.0f) {
        return;
    }
    g_app.tempo = bpm;
    synth_set_tempo(&g_app.synth, bpm);
    g_app.knob_tempo.value = bpm;
    enqueue_param_float_msg(PARAM_TEMPO, bpm);
}

static int strings_equal_ci(const char* a, const char* b) {
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        int lhs = tolower((unsigned char)*a);
        int rhs = tolower((unsigned char)*b);
        if (lhs != rhs) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static void sanitize_name_for_path(const char* src, char* dst, size_t dst_size) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || !src[0]) {
        snprintf(dst, dst_size, "preset");
        return;
    }
    size_t written = 0;
    for (size_t i = 0; src[i] && written + 1 < dst_size; ++i) {
        char c = src[i];
        if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) {
            c = '_';
        }
        dst[written++] = c;
    }
    if (written == 0) {
        dst[written++] = 'p';
    }
    dst[written] = '\0';
}

static PresetSnippetEntry* preset_snippet_entry_for_name(PresetSnippetLibrary* lib,
                                                         const char* preset_name,
                                                         int create_if_missing,
                                                         int* out_index) {
    if (out_index) {
        *out_index = -1;
    }
    if (!lib || !preset_name || !preset_name[0]) {
        return NULL;
    }
    for (int i = 0; i < MAX_PRESET_SNIPPET_PRESETS; ++i) {
        PresetSnippetEntry* entry = &lib->entries[i];
        if (!entry->in_use) {
            continue;
        }
        if (strings_equal_ci(entry->preset_name, preset_name)) {
            if (out_index) {
                *out_index = i;
            }
            return entry;
        }
    }
    if (!create_if_missing) {
        return NULL;
    }
    for (int i = 0; i < MAX_PRESET_SNIPPET_PRESETS; ++i) {
        PresetSnippetEntry* entry = &lib->entries[i];
        if (entry->in_use) {
            continue;
        }
        memset(entry, 0, sizeof(*entry));
        entry->in_use = 1;
        snprintf(entry->preset_name, sizeof(entry->preset_name), "%s", preset_name);
        for (int s = 0; s < MAX_SNIPPETS_PER_PRESET; ++s) {
            sample_buffer_init(&entry->snippets[s].buffer);
        }
        entry->average_tempo = 0.0f;
        if (out_index) {
            *out_index = i;
        }
        return entry;
    }
    fprintf(stderr, "âš ï¸ No free preset snippet slots available for '%s'\n", preset_name);
    return NULL;
}

static int preset_snippet_has_audio(const PresetSnippet* snippet) {
    return snippet && snippet->buffer.data && snippet->buffer.frame_count > 0;
}

static void preset_snippet_play_all_advance_locked(PresetSnippetLibrary* lib);

static void preset_snippet_update_average(PresetSnippetEntry* entry) {
    if (!entry) {
        return;
    }
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < MAX_SNIPPETS_PER_PRESET; ++i) {
        const PresetSnippet* snippet = &entry->snippets[i];
        if (preset_snippet_has_audio(snippet) && snippet->captured_tempo > 0.0f) {
            sum += snippet->captured_tempo;
            ++count;
        }
    }
    entry->average_tempo = count > 0 ? (sum / (float)count) : 0.0f;
}

static bool preset_snippet_write_to_disk(PresetSnippetLibrary* lib,
                                         PresetSnippetEntry* entry,
                                         PresetSnippet* snippet,
                                         const float* interleaved,
                                         uint32_t frame_count,
                                         uint32_t channels,
                                         uint32_t sample_rate) {
    if (!lib || !entry || !snippet || !interleaved || frame_count == 0 || channels == 0) {
        return false;
    }
    ensure_directory_exists(lib->base_dir);
    char preset_folder[96];
    sanitize_name_for_path(entry->preset_name, preset_folder, sizeof(preset_folder));

    char preset_dir[260];
    snprintf(preset_dir, sizeof(preset_dir), "%s/%s", lib->base_dir, preset_folder);
    ensure_directory_exists(preset_dir);

    time_t now = time(NULL);
    struct tm tm_info;
#if defined(_WIN32)
    localtime_s(&tm_info, &now);
#else
    localtime_r(&now, &tm_info);
#endif
    char filename[128];
    snprintf(filename,
             sizeof(filename),
             "%s_%04d%02d%02d_%02d%02d%02d.wav",
             preset_folder,
             tm_info.tm_year + 1900,
             tm_info.tm_mon + 1,
             tm_info.tm_mday,
             tm_info.tm_hour,
             tm_info.tm_min,
             tm_info.tm_sec);

    char full_path[520];
    snprintf(full_path, sizeof(full_path), "%s/%s", preset_dir, filename);

    bool ok = sample_buffer_write_wav(full_path, interleaved, frame_count, channels, sample_rate);
    if (ok) {
        snprintf(snippet->relative_path, sizeof(snippet->relative_path), "%s/%s", preset_folder, filename);
        printf("ðŸ’¾ Preset snippet saved to %s\n", full_path);
    }
    return ok;
}

static void preset_snippet_library_save_manifest(PresetSnippetLibrary* lib) {
    if (!lib) {
        return;
    }
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return;
    }
    cJSON* entries = cJSON_AddArrayToObject(root, "entries");
    if (!entries) {
        cJSON_Delete(root);
        return;
    }
    for (int i = 0; i < MAX_PRESET_SNIPPET_PRESETS; ++i) {
        PresetSnippetEntry* entry = &lib->entries[i];
        if (!entry->in_use) {
            continue;
        }
        cJSON* entry_json = cJSON_CreateObject();
        if (!entry_json) {
            continue;
        }
        cJSON_AddStringToObject(entry_json, "name", entry->preset_name);
        cJSON* snippet_array = cJSON_AddArrayToObject(entry_json, "snippets");
        if (!snippet_array) {
            cJSON_Delete(entry_json);
            continue;
        }
        int written = 0;
        for (int s = 0; s < MAX_SNIPPETS_PER_PRESET; ++s) {
            PresetSnippet* snippet = &entry->snippets[s];
            if (!preset_snippet_has_audio(snippet) || snippet->relative_path[0] == '\0') {
                continue;
            }
            cJSON* sn = cJSON_CreateObject();
            if (!sn) {
                continue;
            }
            cJSON_AddStringToObject(sn, "file", snippet->relative_path);
            cJSON_AddNumberToObject(sn, "tempo", snippet->captured_tempo);
            cJSON_AddItemToArray(snippet_array, sn);
            ++written;
        }
        if (written == 0) {
            cJSON_Delete(entry_json);
        } else {
            cJSON_AddItemToArray(entries, entry_json);
        }
    }

    char* serialized = cJSON_PrintBuffered(root, 1024, 1);
    cJSON_Delete(root);
    if (!serialized) {
        return;
    }
    size_t len = strlen(serialized);
    preset_write_text_file(lib->manifest_path, serialized, len);
    cJSON_free(serialized);
}

static void preset_snippet_library_load_manifest(PresetSnippetLibrary* lib) {
    if (!lib) {
        return;
    }
    long size = 0;
    char* data = preset_read_text_file(lib->manifest_path, &size);
    if (!data) {
        return;
    }
    cJSON* root = cJSON_ParseWithLength(data, (size_t)size);
    free(data);
    if (!root) {
        return;
    }
    const cJSON* entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (cJSON_IsArray(entries)) {
        cJSON* entry_json = NULL;
        cJSON_ArrayForEach(entry_json, entries) {
            const cJSON* name = cJSON_GetObjectItemCaseSensitive(entry_json, "name");
            if (!cJSON_IsString(name) || !name->valuestring) {
                continue;
            }
            int entry_index = -1;
            PresetSnippetEntry* entry = preset_snippet_entry_for_name(lib, name->valuestring, 1, &entry_index);
            if (!entry) {
                continue;
            }
            const cJSON* snippets = cJSON_GetObjectItemCaseSensitive(entry_json, "snippets");
            if (!cJSON_IsArray(snippets)) {
                continue;
            }
            int slot = 0;
            cJSON* snippet_json = NULL;
            cJSON_ArrayForEach(snippet_json, snippets) {
                if (slot >= MAX_SNIPPETS_PER_PRESET) {
                    break;
                }
                const cJSON* file = cJSON_GetObjectItemCaseSensitive(snippet_json, "file");
                const cJSON* tempo = cJSON_GetObjectItemCaseSensitive(snippet_json, "tempo");
                if (!cJSON_IsString(file) || !file->valuestring) {
                    continue;
                }
                char full_path[520];
                snprintf(full_path, sizeof(full_path), "%s/%s", lib->base_dir, file->valuestring);
                PresetSnippet* snippet = &entry->snippets[slot];
                if (sample_buffer_load_wav(&snippet->buffer, full_path)) {
                    snippet->record_capacity_frames = snippet->buffer.frame_count;
                    snippet->recorded_frames = snippet->buffer.frame_count;
                    snippet->playback_pos = 0;
                    snippet->recording = 0;
                    snippet->playing = 0;
                    snippet->captured_tempo = cJSON_IsNumber(tempo) ? (float)tempo->valuedouble : 120.0f;
                    snprintf(snippet->relative_path, sizeof(snippet->relative_path), "%s", file->valuestring);
                    ++slot;
                }
            }
            preset_snippet_update_average(entry);
        }
    }
    cJSON_Delete(root);
}

static void preset_snippet_library_init(PresetSnippetLibrary* lib) {
    if (!lib) {
        return;
    }
    memset(lib, 0, sizeof(*lib));
    snprintf(lib->base_dir, sizeof(lib->base_dir), "preset_snippets");
    ensure_directory_exists(lib->base_dir);
    snprintf(lib->manifest_path, sizeof(lib->manifest_path), "%s/library.json", lib->base_dir);
    for (int i = 0; i < MAX_PRESET_SNIPPET_PRESETS; ++i) {
        lib->entries[i].in_use = 0;
        for (int s = 0; s < MAX_SNIPPETS_PER_PRESET; ++s) {
            sample_buffer_init(&lib->entries[i].snippets[s].buffer);
        }
    }
    lib->active_entry_index = -1;
    lib->active_snippet_index = -1;
    lib->play_all_entry_index = -1;
    lib->play_all_snippet_index = -1;
    lib->pending_save_entry_index = -1;
    lib->pending_save_snippet_index = -1;
    preset_snippet_library_load_manifest(lib);
}

static void preset_snippet_schedule_save_locked(PresetSnippetLibrary* lib,
                                                int entry_index,
                                                int snippet_index) {
    if (!lib) {
        return;
    }
    lib->pending_save_ready = 1;
    lib->pending_save_entry_index = entry_index;
    lib->pending_save_snippet_index = snippet_index;
}

static void preset_snippet_finalize_recording_locked(PresetSnippetLibrary* lib,
                                                     int entry_index,
                                                     int snippet_index) {
    if (!lib || entry_index < 0 || snippet_index < 0) {
        return;
    }
    PresetSnippetEntry* entry = &lib->entries[entry_index];
    PresetSnippet* snippet = &entry->snippets[snippet_index];
    snippet->recording = 0;
    snippet->buffer.frame_count = snippet->recorded_frames;
    lib->active_recording = NULL;
    lib->active_entry_index = entry_index;
    lib->active_snippet_index = snippet_index;
    if (snippet->recorded_frames == 0) {
        return;
    }
    preset_snippet_schedule_save_locked(lib, entry_index, snippet_index);
}

static void preset_snippet_flush_pending_save(PresetSnippetLibrary* lib) {
    if (!lib || !lib->pending_save_ready) {
        return;
    }
    const int entry_index = lib->pending_save_entry_index;
    const int snippet_index = lib->pending_save_snippet_index;
    if (entry_index < 0 || snippet_index < 0 || entry_index >= MAX_PRESET_SNIPPET_PRESETS) {
        lib->pending_save_ready = 0;
        return;
    }
    PresetSnippetEntry* entry = &lib->entries[entry_index];
    if (!entry->in_use) {
        lib->pending_save_ready = 0;
        return;
    }
    PresetSnippet* snippet = &entry->snippets[snippet_index];
    if (!preset_snippet_has_audio(snippet)) {
        lib->pending_save_ready = 0;
        return;
    }

    float* copy = NULL;
    uint32_t frame_count = 0;
    uint32_t channels = 0;
    uint32_t sample_rate = 0;

    audio_state_lock();
    size_t sample_count = (size_t)snippet->buffer.frame_count * snippet->buffer.channels;
    if (sample_count > 0 && snippet->buffer.data) {
        copy = (float*)malloc(sample_count * sizeof(float));
        if (copy) {
            memcpy(copy, snippet->buffer.data, sample_count * sizeof(float));
            frame_count = snippet->buffer.frame_count;
            channels = snippet->buffer.channels;
            sample_rate = snippet->buffer.sample_rate;
        }
    }
    lib->pending_save_ready = 0;
    audio_state_unlock();

    if (!copy) {
        return;
    }

    if (preset_snippet_write_to_disk(lib, entry, snippet, copy, frame_count, channels, sample_rate)) {
        preset_snippet_update_average(entry);
        preset_snippet_library_save_manifest(lib);
    }
    free(copy);
}

static void preset_snippet_process_frame(float* left, float* right) {
    PresetSnippetLibrary* lib = &g_app.preset_snippets;
    if (!lib) {
        return;
    }
    if (lib->active_recording && lib->active_recording->recording) {
        PresetSnippet* rec = lib->active_recording;
        if (rec->recorded_frames < rec->record_capacity_frames && rec->buffer.data) {
            size_t idx = (size_t)rec->recorded_frames * rec->buffer.channels;
            rec->buffer.data[idx] = *left;
            if (rec->buffer.channels > 1) {
                rec->buffer.data[idx + 1] = *right;
            }
            rec->recorded_frames++;
            if (rec->recorded_frames >= rec->record_capacity_frames) {
                preset_snippet_finalize_recording_locked(lib,
                                                        lib->active_entry_index,
                                                        lib->active_snippet_index);
            }
        }
    }

    float playback_l = 0.0f;
    float playback_r = 0.0f;
    if (lib->active_playback && lib->active_playback->playing) {
        PresetSnippet* snippet = lib->active_playback;
        if (snippet->playback_pos < snippet->buffer.frame_count) {
            size_t idx = (size_t)snippet->playback_pos * snippet->buffer.channels;
            playback_l = snippet->buffer.data[idx];
            playback_r = (snippet->buffer.channels > 1) ? snippet->buffer.data[idx + 1] : playback_l;
            snippet->playback_pos++;
        } else {
            snippet->playing = 0;
        }

        if (!snippet->playing || snippet->playback_pos >= snippet->buffer.frame_count) {
            snippet->playing = 0;
            snippet->playback_pos = 0;
            lib->active_playback = NULL;
            if (lib->play_all_active) {
                lib->play_all_entry_index = lib->active_entry_index;
                lib->play_all_snippet_index = lib->active_snippet_index;
                preset_snippet_play_all_advance_locked(lib);
            } else {
                lib->play_all_entry_index = -1;
                lib->play_all_snippet_index = -1;
            }
        }
    }

    *left += playback_l;
    *right += playback_r;
}

static void preset_snippet_tick(void) {
    preset_snippet_flush_pending_save(&g_app.preset_snippets);
}

static void preset_snippet_stop_playback_locked(PresetSnippetLibrary* lib, int preserve_play_all) {
    if (!lib || !lib->active_playback) {
        return;
    }
    lib->active_playback->playing = 0;
    lib->active_playback->playback_pos = 0;
    lib->active_playback = NULL;
    if (!preserve_play_all) {
        lib->play_all_active = 0;
        lib->play_all_entry_index = -1;
        lib->play_all_snippet_index = -1;
    }
}

static void preset_snippet_start_playback_locked(PresetSnippetLibrary* lib,
                                                 int entry_index,
                                                 int snippet_index) {
    if (!lib || entry_index < 0 || snippet_index < 0) {
        return;
    }
    PresetSnippetEntry* entry = &lib->entries[entry_index];
    if (!entry->in_use) {
        return;
    }
    PresetSnippet* snippet = &entry->snippets[snippet_index];
    if (!preset_snippet_has_audio(snippet)) {
        return;
    }
    preset_snippet_stop_playback_locked(lib, lib->play_all_active);
    snippet->playing = 1;
    snippet->playback_pos = 0;
    lib->active_playback = snippet;
    lib->active_entry_index = entry_index;
    lib->active_snippet_index = snippet_index;
    if (entry->average_tempo > 0.0f) {
        app_apply_tempo(entry->average_tempo);
    }
}

static void preset_snippet_play_all_advance_locked(PresetSnippetLibrary* lib) {
    if (!lib) {
        return;
    }
    int start_entry = lib->play_all_entry_index;
    int start_slot = lib->play_all_snippet_index;
    if (start_entry < 0) {
        start_entry = 0;
        start_slot = -1;
    }
    int entry = start_entry;
    int slot = start_slot;
    const int max_iters = MAX_PRESET_SNIPPET_PRESETS * MAX_SNIPPETS_PER_PRESET;
    for (int iter = 0; iter < max_iters; ++iter) {
        ++slot;
        if (slot >= MAX_SNIPPETS_PER_PRESET) {
            slot = 0;
            ++entry;
            if (entry >= MAX_PRESET_SNIPPET_PRESETS) {
                entry = 0;
            }
        }
        if (entry == start_entry && slot == start_slot) {
            break;
        }
        PresetSnippetEntry* candidate = &lib->entries[entry];
        if (!candidate->in_use) {
            continue;
        }
        if (!preset_snippet_has_audio(&candidate->snippets[slot])) {
            continue;
        }
        preset_snippet_start_playback_locked(lib, entry, slot);
        lib->play_all_entry_index = entry;
        lib->play_all_snippet_index = slot;
        return;
    }
    lib->play_all_active = 0;
    lib->play_all_entry_index = -1;
    lib->play_all_snippet_index = -1;
    lib->active_playback = NULL;
}

static void preset_snippet_play_all_start(void) {
    PresetSnippetLibrary* lib = &g_app.preset_snippets;
    preset_snippet_flush_pending_save(lib);
    audio_state_lock();
    lib->play_all_active = 1;
    lib->play_all_entry_index = -1;
    lib->play_all_snippet_index = -1;
    preset_snippet_play_all_advance_locked(lib);
    audio_state_unlock();
}

static void preset_snippet_play_all_stop(void) {
    audio_state_lock();
    preset_snippet_stop_playback_locked(&g_app.preset_snippets, 0);
    audio_state_unlock();
}

static void preset_snippet_begin_recording_slot(const char* preset_name, int slot_index) {
    if (slot_index < 0 || slot_index >= MAX_SNIPPETS_PER_PRESET) {
        return;
    }
    PresetSnippetLibrary* lib = &g_app.preset_snippets;
    preset_snippet_flush_pending_save(lib);
    audio_state_lock();
    preset_snippet_stop_playback_locked(lib, 0);
    int entry_index = -1;
    PresetSnippetEntry* entry = preset_snippet_entry_for_name(lib, preset_name, 1, &entry_index);
    if (!entry) {
        audio_state_unlock();
        return;
    }
    PresetSnippet* snippet = &entry->snippets[slot_index];
    sample_buffer_free(&snippet->buffer);
    snippet->relative_path[0] = '\0';
    snippet->buffer.channels = 2;
    snippet->buffer.sample_rate = (uint32_t)(g_app.synth.sample_rate > 0.0f ? g_app.synth.sample_rate : 44100.0f);
    snippet->record_capacity_frames = snippet->buffer.sample_rate * PRESET_SNIPPET_MAX_SECONDS;
    size_t samples = (size_t)snippet->record_capacity_frames * snippet->buffer.channels;
    snippet->buffer.data = (float*)malloc(samples * sizeof(float));
    if (!snippet->buffer.data) {
        audio_state_unlock();
        fprintf(stderr, "âŒ Unable to allocate preset snippet buffer\n");
        return;
    }
    snippet->buffer.frame_count = 0;
    snippet->recorded_frames = 0;
    snippet->playback_pos = 0;
    snippet->recording = 1;
    snippet->playing = 0;
    snippet->captured_tempo = g_app.tempo;
    lib->active_recording = snippet;
    lib->active_entry_index = entry_index;
    lib->active_snippet_index = slot_index;
    audio_state_unlock();
}

static void preset_snippet_stop_recording_slot(const char* preset_name, int slot_index) {
    if (slot_index < 0 || slot_index >= MAX_SNIPPETS_PER_PRESET) {
        return;
    }
    PresetSnippetLibrary* lib = &g_app.preset_snippets;
    audio_state_lock();
    int entry_index = -1;
    PresetSnippetEntry* entry = preset_snippet_entry_for_name(lib, preset_name, 0, &entry_index);
    if (!entry) {
        audio_state_unlock();
        return;
    }
    PresetSnippet* snippet = &entry->snippets[slot_index];
    if (!snippet->recording) {
        audio_state_unlock();
        return;
    }
    preset_snippet_finalize_recording_locked(lib, entry_index, slot_index);
    audio_state_unlock();
    preset_snippet_flush_pending_save(lib);
}

static void preset_snippet_play_slot(const char* preset_name, int slot_index) {
    PresetSnippetLibrary* lib = &g_app.preset_snippets;
    preset_snippet_flush_pending_save(lib);
    audio_state_lock();
    lib->play_all_active = 0;
    lib->play_all_entry_index = -1;
    lib->play_all_snippet_index = -1;
    int entry_index = -1;
    PresetSnippetEntry* entry = preset_snippet_entry_for_name(lib, preset_name, 0, &entry_index);
    if (!entry) {
        audio_state_unlock();
        return;
    }
    preset_snippet_start_playback_locked(lib, entry_index, slot_index);
    audio_state_unlock();
}

static void ui_knobs_init(void) {
    const float filter_cutoff_max = fminf(g_app.synth.sample_rate * 0.45f, 20000.0f);
    const float filter_cutoff_default = fminf(g_app.filter_cutoff, filter_cutoff_max);
    ui_knob_state_init(&g_app.knob_master_volume, 0.0f, 1.0f, g_app.master_volume);
    ui_knob_state_init(&g_app.knob_tempo, 60.0f, 300.0f, g_app.tempo);
    ui_knob_state_init(&g_app.knob_filter_cutoff, 20.0f, filter_cutoff_max, filter_cutoff_default);
    ui_knob_state_init(&g_app.knob_filter_resonance, 0.0f, 1.0f, g_app.filter_resonance);
    ui_knob_state_init(&g_app.knob_filter_env, -1.0f, 1.0f, g_app.filter_env);
    ui_knob_state_init(&g_app.knob_env_attack, 0.001f, 2.0f, g_app.env_attack);
    ui_knob_state_init(&g_app.knob_env_decay, 0.001f, 2.0f, g_app.env_decay);
    ui_knob_state_init(&g_app.knob_env_sustain, 0.0f, 1.0f, g_app.env_sustain);
    ui_knob_state_init(&g_app.knob_env_release, 0.001f, 5.0f, g_app.env_release);
    ui_knob_state_init(&g_app.knob_osc1_detune, -50.0f, 50.0f, g_app.osc1_detune);
    ui_knob_state_init(&g_app.knob_osc1_pwm, 0.0f, 1.0f, g_app.osc1_pwm);
    ui_knob_state_init(&g_app.knob_arp_rate, 1.0f, 16.0f, g_app.arp.rate > 0.0f ? g_app.arp.rate : 2.0f);
    for (int i = 0; i < UI_MACRO_COUNT; ++i) {
        ui_knob_state_init(&g_app.knob_macro[i], 0.0f, 1.0f, g_app.macro_values[i]);
    }
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

static bool ui_str_icontains(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) {
        return true;
    }
    if (!haystack) {
        return false;
    }
    for (const char* h = haystack; *h; ++h) {
        size_t i = 0;
        while (needle[i] && h[i]) {
            const int lhs = tolower((unsigned char)h[i]);
            const int rhs = tolower((unsigned char)needle[i]);
            if (lhs != rhs) {
                break;
            }
            ++i;
        }
        if (!needle[i]) {
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
    switch (change->id) {
        case PARAM_FX_DISTORTION_ENABLED:
            g_app.fx.distortion.enabled = param_msg_get_bool(change);
            g_app.fx_dist_enabled = g_app.fx.distortion.enabled;
            return;
        case PARAM_FX_DISTORTION_DRIVE:
            g_app.fx.distortion.drive = param_msg_get_float(change);
            return;
        case PARAM_FX_DISTORTION_MIX:
            g_app.fx.distortion.mix = param_msg_get_float(change);
            return;
        case PARAM_FX_DELAY_ENABLED:
            g_app.fx.delay.enabled = param_msg_get_bool(change);
            g_app.fx_delay_enabled = g_app.fx.delay.enabled;
            return;
        case PARAM_FX_DELAY_TIME:
            g_app.fx.delay.time_ms = param_msg_get_float(change);
            return;
        case PARAM_FX_DELAY_FEEDBACK:
            g_app.fx.delay.feedback = param_msg_get_float(change);
            return;
        case PARAM_FX_DELAY_MIX:
            g_app.fx.delay.mix = param_msg_get_float(change);
            return;
        case PARAM_FX_REVERB_ENABLED:
            g_app.fx.reverb.enabled = param_msg_get_bool(change);
            g_app.fx_reverb_enabled = g_app.fx.reverb.enabled;
            return;
        case PARAM_FX_REVERB_SIZE:
            g_app.fx.reverb.size = param_msg_get_float(change);
            return;
        case PARAM_FX_REVERB_DAMPING:
            g_app.fx.reverb.damping = param_msg_get_float(change);
            return;
        case PARAM_FX_REVERB_MIX:
            g_app.fx.reverb.mix = param_msg_get_float(change);
            return;
        default:
            synth_engine_apply_param(&g_app.synth, change);
            break;
    }
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
    const float* in = (const float*)input;
    
    double sample_duration = 1.0 / g_app.synth.sample_rate;

    param_queue_drain(apply_param_change, NULL);
    midi_queue_drain(handle_midi_event, NULL);

    audio_state_lock();

    const int capture_channels = device->capture.channels > 0 ? device->capture.channels : g_app.capture_channels;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        float mic_l = 0.0f;
        float mic_r = 0.0f;
        if (in && capture_channels > 0) {
            mic_l = in[i * capture_channels];
            mic_r = (capture_channels > 1) ? in[i * capture_channels + 1] : mic_l;
        }

        arp_process(&g_app.arp, &g_app.synth, g_app.current_time, g_app.tempo);
        
        float left = 0.0f, right = 0.0f;
        float temp_buffer[2];
        synth_process(&g_app.synth, temp_buffer, 1);
        left = temp_buffer[0];
        right = temp_buffer[1];
        
        fx_distortion_process(&g_app.fx.distortion, &left, &right);
        fx_delay_process(&g_app.fx.delay, &left, &right, g_app.synth.sample_rate);
        fx_reverb_process(&g_app.fx.reverb, &left, &right);

        float voice_mix_l = 0.0f;
        float voice_mix_r = 0.0f;

        for (int t = 0; t < MAX_VOICE_TRACKS; ++t) {
            VoiceTrack* track = &g_app.voice_layers.tracks[t];

            if (track->recording && track->buffer.data && track->recorded_frames < track->record_capacity_frames) {
                size_t idx = (size_t)track->recorded_frames * track->buffer.channels;
                track->buffer.data[idx] = mic_l;
                if (track->buffer.channels > 1) {
                    track->buffer.data[idx + 1] = mic_r;
                }
                track->recorded_frames++;
                if (track->recorded_frames >= track->record_capacity_frames) {
                    track->recording = 0;
                    track->buffer.frame_count = track->recorded_frames;
                }
            }

            if (track->playing && track->buffer.data && track->buffer.frame_count > 0) {
                if (track->playback_pos >= track->buffer.frame_count) {
                    track->playback_pos = 0;
                }
                size_t idx = (size_t)track->playback_pos * track->buffer.channels;
                float src_l = track->buffer.data[idx];
                float src_r = (track->buffer.channels > 1) ? track->buffer.data[idx + 1] : src_l;
                voice_mix_l += src_l * track->volume;
                voice_mix_r += src_r * track->volume;
                track->playback_pos++;
                if (track->playback_pos >= track->buffer.frame_count) {
                    track->playback_pos = 0;
                }
            }
        }

        float output_l = left + voice_mix_l;
        float output_r = right + voice_mix_r;
        preset_snippet_process_frame(&output_l, &output_r);

        out[i * 2 + 0] = output_l;
        out[i * 2 + 1] = output_r;

        g_app.current_time += sample_duration;
    }

    audio_state_unlock();
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
    printf("ðŸŽµ QUEUE NOTE ON: %s (MIDI %d)\n", label, midi_note);
    fflush(stdout);
}

void stop_note(int midi_note, const char* label) {
    if (midi_note < 0 || midi_note > 127) {
        return;
    }
    midi_queue_send_note_off((uint8_t)midi_note);
    printf("ðŸ”‡ QUEUE NOTE OFF: %s (MIDI %d)\n", label, midi_note);
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
            printf("ðŸš¨ PANIC - All notes off\n");
        } else if (key == GLFW_KEY_SPACE) {
            g_app.playing = !g_app.playing;
            printf("%s %s\n", g_app.playing ? "â–¶" : "â¸", g_app.playing ? "Playing" : "Stopped");
        }
    }
}

void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// ============================================================================
// GUI
// ============================================================================

void draw_gui(struct nk_context *ctx, float width, float height) {
    preset_snippet_tick();
    const UiMetrics* metrics = ui_style_metrics();
    const UiPalette* palette = ui_style_palette();
    const char* window_name = "Professional Synthesizer";
    if (width < metrics->min_layout_width) {
        width = metrics->min_layout_width;
    }
    if (height < 700.0f) {
        height = 700.0f;
    }
    const nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

    struct nk_rect desired_bounds = nk_rect(0, 0, width, height);
    struct nk_window* existing_window = nk_window_find(ctx, window_name);
    static struct nk_rect applied_bounds = {0, 0, 0, 0};
    if (existing_window && (applied_bounds.x != desired_bounds.x ||
                            applied_bounds.y != desired_bounds.y ||
                            applied_bounds.w != desired_bounds.w ||
                            applied_bounds.h != desired_bounds.h)) {
        nk_window_set_bounds(ctx, window_name, desired_bounds);
        applied_bounds = desired_bounds;
    } else if (!existing_window) {
        applied_bounds = desired_bounds;
    }

    if (nk_begin(ctx, window_name, desired_bounds, window_flags)) {
    struct nk_rect region = nk_window_get_content_region(ctx);
    const float column_gap = metrics->column_gap * 1.5f;
    float nav_width = fminf(320.0f, region.w * 0.34f);
        float content_width = region.w - nav_width - column_gap;

        if (content_width < 520.0f) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Increase the window width to view the Surge-inspired control surface.", NK_TEXT_CENTERED);
            nk_end(ctx);
            return;
        }

    float column_width = (content_width - 2.0f * column_gap) / 3.0f;
    const nk_flags compact_panel_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;
    const nk_flags scroll_panel_flags = NK_WINDOW_BORDER;

        static const char* kPatchCategories[] = {"Init", "Bass", "Leads", "Keys", "Pads", "Sequences"};
        const int patch_category_count = (int)(sizeof(kPatchCategories) / sizeof(kPatchCategories[0]));

        nk_layout_row_begin(ctx, NK_STATIC, 36, 2);
        nk_layout_row_push(ctx, region.w * 0.58f);
        nk_edit_string_zero_terminated(ctx,
                                       NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT,
                                       g_app.patch_name,
                                       sizeof(g_app.patch_name),
                                       nk_filter_default);
        nk_layout_row_push(ctx, region.w * 0.32f);
        g_app.patch_category = nk_combo(ctx,
                                        kPatchCategories,
                                        patch_category_count,
                                        g_app.patch_category,
                                        28,
                                        nk_vec2(220, 220));
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 32, 4);
        nk_layout_row_push(ctx, region.w * 0.22f);
        if (nk_button_label(ctx, "Init Patch")) {
            strncpy(g_app.patch_name, "Init Patch", sizeof(g_app.patch_name));
            g_app.patch_name[sizeof(g_app.patch_name) - 1] = '\0';
        }
        nk_layout_row_push(ctx, region.w * 0.22f);
        nk_button_label(ctx, "Save Snapshot");
        nk_layout_row_push(ctx, region.w * 0.22f);
        nk_button_label(ctx, "Duplicate to Scene B");
        nk_layout_row_push(ctx, region.w * 0.22f);
        nk_button_label(ctx, "Randomize (placeholder)");
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 42, 3);
        nk_layout_row_push(ctx, region.w * 0.30f);
        if (nk_button_label(ctx, g_app.playing ? "â¹ Stop Transport" : "â–¶ Start Transport")) {
            g_app.playing = !g_app.playing;
        }
        nk_layout_row_push(ctx, region.w * 0.38f);
        nk_size voice_meter = (nk_size)g_app.synth.num_active_voices;
        nk_progress(ctx, &voice_meter, MAX_VOICES, nk_false);
        nk_layout_row_push(ctx, region.w * 0.24f);
        int previous_arp_enabled = g_app.arp_enabled;
        nk_checkbox_label(ctx, "Arp Enabled", &g_app.arp_enabled);
        if (previous_arp_enabled != g_app.arp_enabled) {
            g_app.arp.enabled = g_app.arp_enabled;
            enqueue_param_int_msg(PARAM_ARP_ENABLED, g_app.arp_enabled);
        } else {
            g_app.arp.enabled = g_app.arp_enabled != 0;
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24, 3);
        nk_layout_row_push(ctx, region.w * 0.30f);
        char voice_info[64];
        snprintf(voice_info, sizeof(voice_info), "%d / %d voices", g_app.synth.num_active_voices, MAX_VOICES);
        nk_label(ctx, voice_info, NK_TEXT_LEFT);
        nk_layout_row_push(ctx, region.w * 0.32f);
        char tempo_info[64];
        snprintf(tempo_info, sizeof(tempo_info), "Tempo %.1f BPM", g_app.tempo);
        nk_label(ctx, tempo_info, NK_TEXT_CENTERED);
        nk_layout_row_push(ctx, region.w * 0.24f);
        int minutes = (int)(g_app.current_time / 60.0);
        int seconds = (int)fmod(g_app.current_time, 60.0);
        char time_info[32];
        snprintf(time_info, sizeof(time_info), "%s %02d:%02d", g_app.playing ? "â–¶" : "â¸", minutes, seconds);
        nk_label(ctx, time_info, NK_TEXT_RIGHT);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);

    nk_layout_row_begin(ctx, NK_STATIC, 620, 2);
        nk_layout_row_push(ctx, nav_width);
    if (nk_group_begin_titled(ctx, "PANEL_NAV", "Scenes & Browser", scroll_panel_flags)) {
            static const char* nav_sections[] = {"Engine", "Mod Matrix", "FX Suite", "Performance"};
            const int section_count = (int)(sizeof(nav_sections) / sizeof(nav_sections[0]));
            static const char* kBrowserPatches[] = {
                "Init Bassline",
                "Glass Keys",
                "Neon Arp",
                "Dub Pad",
                "FM Pluck",
                "Surge Classic"
            };
            const int browser_patch_count = (int)(sizeof(kBrowserPatches) / sizeof(kBrowserPatches[0]));

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Scenes", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 28, 2);
            if (nk_option_label(ctx, "Scene A", g_app.active_scene == 0)) {
                g_app.active_scene = 0;
            }
            if (nk_option_label(ctx, "Scene B", g_app.active_scene == 1)) {
                g_app.active_scene = 1;
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Blend", NK_TEXT_LEFT);
            nk_slider_float(ctx, 0.0f, &g_app.scene_blend, 1.0f, 0.01f);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_checkbox_label(ctx, "Link modulation", &g_app.scenes_linked);

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Patch Browser", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 28, 1);
            nk_edit_string_zero_terminated(ctx,
                                           NK_EDIT_FIELD,
                                           g_app.browser_search,
                                           sizeof(g_app.browser_search),
                                           nk_filter_ascii);

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx,
                     g_app.browser_search[0] ? "Filtering results below (prototype)"
                                             : "Type to quickly jump between curated presets.",
                     NK_TEXT_LEFT);

            const int chips_per_row = 3;
            for (int start = 0; start < patch_category_count; start += chips_per_row) {
                nk_layout_row_dynamic(ctx, 26, chips_per_row);
                for (int j = 0; j < chips_per_row && (start + j) < patch_category_count; ++j) {
                    const int idx = start + j;
                    nk_bool chip_active = (nk_bool)(g_app.patch_category == idx);
                    if (nk_selectable_label(ctx, kPatchCategories[idx], NK_TEXT_CENTERED, &chip_active)) {
                        g_app.patch_category = idx;
                    }
                }
            }

            nk_layout_row_dynamic(ctx, 110, 1);
            if (nk_group_begin(ctx, "NAV_BROWSER_LIST", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
                int visible = 0;
                for (int i = 0; i < browser_patch_count; ++i) {
                    if (!ui_str_icontains(kBrowserPatches[i], g_app.browser_search)) {
                        continue;
                    }
                    ++visible;
                    nk_bool selected = (nk_bool)(g_app.browser_selection == i);
                    if (nk_selectable_label(ctx, kBrowserPatches[i], NK_TEXT_LEFT, &selected)) {
                        g_app.browser_selection = i;
                        strncpy(g_app.patch_name, kBrowserPatches[i], sizeof(g_app.patch_name));
                        g_app.patch_name[sizeof(g_app.patch_name) - 1] = '\0';
                    }
                }
                if (!visible) {
                    nk_label(ctx, "No matches â€” try a different tag or search.", NK_TEXT_CENTERED);
                }
                nk_group_end(ctx);
            }

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Sections", NK_TEXT_LEFT);
            for (int i = 0; i < section_count; ++i) {
                nk_bool active = (nk_bool)(g_app.ui_section == i);
                if (nk_selectable_label(ctx, nav_sections[i], NK_TEXT_LEFT, &active)) {
                    g_app.ui_section = i;
                }
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Favorites", NK_TEXT_LEFT);
            if (nk_button_label(ctx, "Init Bassline")) {
                strncpy(g_app.patch_name, "Init Bassline", sizeof(g_app.patch_name));
                g_app.patch_name[sizeof(g_app.patch_name) - 1] = '\0';
            }
            if (nk_button_label(ctx, "Glass Keys")) {
                strncpy(g_app.patch_name, "Glass Keys", sizeof(g_app.patch_name));
                g_app.patch_name[sizeof(g_app.patch_name) - 1] = '\0';
            }

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, 260, 1);
            if (nk_group_begin_titled(ctx,
                                      "PANEL_PRESET_SNIPPETS",
                                      "Preset Snippets",
                                      NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
                PresetSnippetLibrary* lib = &g_app.preset_snippets;
                const char* preset_name = g_app.patch_name[0] ? g_app.patch_name : "Untitled";
                int entry_index = -1;
                PresetSnippetEntry* entry = preset_snippet_entry_for_name(lib, preset_name, 1, &entry_index);

                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, preset_name, NK_TEXT_LEFT);

                char tempo_buf[64];
                if (entry && entry->average_tempo > 0.0f) {
                    snprintf(tempo_buf, sizeof(tempo_buf), "Average tempo: %.1f BPM", entry->average_tempo);
                } else {
                    snprintf(tempo_buf, sizeof(tempo_buf), "Average tempo: n/a");
                }
                nk_layout_row_dynamic(ctx, 18, 1);
                nk_label(ctx, tempo_buf, NK_TEXT_LEFT);

                if (!entry) {
                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, "All preset snippet slots are currently in use.", NK_TEXT_LEFT);
                }

                for (int slot = 0; slot < MAX_SNIPPETS_PER_PRESET && entry; ++slot) {
                    PresetSnippet* snippet = entry ? &entry->snippets[slot] : NULL;
                    char status[128];
                    if (snippet && snippet->recording) {
                        float seconds = snippet->recorded_frames > 0 && snippet->buffer.sample_rate > 0
                                            ? (float)snippet->recorded_frames / (float)snippet->buffer.sample_rate
                                            : 0.0f;
                        snprintf(status, sizeof(status), "Recordingâ€¦ (%.1fs)", seconds);
                    } else if (snippet && preset_snippet_has_audio(snippet)) {
                        float seconds = (snippet->buffer.sample_rate > 0)
                                            ? (float)snippet->buffer.frame_count / (float)snippet->buffer.sample_rate
                                            : 0.0f;
                        snprintf(status,
                                 sizeof(status),
                                 "%.1fs @ %.1f BPM",
                                 seconds,
                                 snippet->captured_tempo);
                    } else {
                        snprintf(status, sizeof(status), "Empty slot");
                    }

                    nk_layout_row_dynamic(ctx, 20, 2);
                    char slot_label[32];
                    snprintf(slot_label, sizeof(slot_label), "Snippet %d", slot + 1);
                    nk_label(ctx, slot_label, NK_TEXT_LEFT);
                    nk_label(ctx, status, NK_TEXT_RIGHT);

                    nk_layout_row_begin(ctx, NK_STATIC, 26, 3);
                    nk_layout_row_push(ctx, 70);
                    if (snippet && snippet->recording) {
                        if (nk_button_label(ctx, "Stop")) {
                            preset_snippet_stop_recording_slot(preset_name, slot);
                        }
                    } else {
                        if (nk_button_label(ctx, "Record")) {
                            preset_snippet_begin_recording_slot(preset_name, slot);
                        }
                    }

                    nk_layout_row_push(ctx, 70);
                    nk_bool can_play = (nk_bool)(snippet && preset_snippet_has_audio(snippet));
                    if (!can_play) {
                        struct nk_style_button disabled = ctx->style.button;
                        struct nk_color inactive = palette->panel_inset;
                        inactive.a = (nk_byte)((int)inactive.a * 0.7f);
                        disabled.normal = disabled.hover = disabled.active = nk_style_item_color(inactive);
                        struct nk_color muted = palette->border;
                        muted.a = (nk_byte)((int)muted.a * 0.7f);
                        disabled.text_normal = disabled.text_hover = disabled.text_active = muted;
                        nk_button_label_styled(ctx, &disabled, "Play");
                    } else {
                        if (nk_button_label(ctx, "Play")) {
                            preset_snippet_play_slot(preset_name, slot);
                        }
                    }

                    nk_layout_row_push(ctx, 90);
                    if (can_play && snippet->relative_path[0]) {
                        nk_label(ctx, snippet->relative_path, NK_TEXT_LEFT);
                    } else {
                        nk_label(ctx, "", NK_TEXT_LEFT);
                    }
                    nk_layout_row_end(ctx);

                    nk_layout_row_dynamic(ctx, 4, 1);
                    nk_spacing(ctx, 1);
                }

                nk_layout_row_dynamic(ctx, 30, 2);
                if (nk_button_label(ctx, "Play All (Tempo Match)")) {
                    preset_snippet_play_all_start();
                }
                if (nk_button_label(ctx, "Stop Playback")) {
                    preset_snippet_play_all_stop();
                }

                nk_layout_row_dynamic(ctx, 18, 1);
                if (lib->play_all_active) {
                    nk_label(ctx, "Playing recorded preset showcaseâ€¦", NK_TEXT_LEFT);
                } else {
                    nk_label(ctx, "Play a snippet to audition captured presets.", NK_TEXT_LEFT);
                }

                nk_group_end(ctx);
            }

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Utilities", NK_TEXT_LEFT);
            nk_button_symbol_label(ctx, NK_SYMBOL_TRIANGLE_RIGHT, "Copy Scene Aâ†’B", NK_TEXT_LEFT);
            nk_button_symbol_label(ctx, NK_SYMBOL_TRIANGLE_RIGHT, "Copy Scene Bâ†’A", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 36, 1);
            nk_label_wrap(ctx, "Navigation now mirrors Surge's browser column with tags, search, and curated patch lists.");
            nk_group_end(ctx);
        }

        nk_layout_row_push(ctx, content_width);
        if (nk_group_begin(ctx, "PANEL_MAIN", scroll_panel_flags)) {
            nk_layout_row_begin(ctx, NK_STATIC, 360, 3);

            nk_layout_row_push(ctx, column_width);
            if (nk_group_begin_titled(ctx, "PANEL_OSC", "Oscillator Deck", compact_panel_flags)) {
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
                UiKnobConfig detune_cfg = {.label = "DETUNE", .unit = "Â¢", .snap_increment = 1.0f};
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
            if (nk_group_begin_titled(ctx, "PANEL_FILTER", "Filter & Envelope", compact_panel_flags)) {
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
            if (nk_group_begin_titled(ctx, "PANEL_MASTER", "Master / FX / Arp", compact_panel_flags)) {
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

                nk_layout_row_dynamic(ctx, 32, 1);
                nk_label(ctx, "Effects", NK_TEXT_LEFT);

                int prev_dist_enabled = g_app.fx_dist_enabled;
                int prev_delay_enabled = g_app.fx_delay_enabled;
                int prev_reverb_enabled = g_app.fx_reverb_enabled;

                nk_layout_row_dynamic(ctx, 32, 3);
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

                nk_layout_row_dynamic(ctx, 28, 1);
                nk_label(ctx, "Distortion", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.drive, 10.0f, 0.1f)) {
                    enqueue_param_float_msg(PARAM_FX_DISTORTION_DRIVE, g_app.fx.distortion.drive);
                }
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.distortion.mix, 1.0f, 0.01f)) {
                    enqueue_param_float_msg(PARAM_FX_DISTORTION_MIX, g_app.fx.distortion.mix);
                }

                nk_layout_row_dynamic(ctx, 28, 1);
                nk_label(ctx, "Delay", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 100.0f, &g_app.fx.delay.time_ms, 2000.0f, 10.0f)) {
                    enqueue_param_float_msg(PARAM_FX_DELAY_TIME, g_app.fx.delay.time_ms);
                }
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.delay.feedback, 0.95f, 0.01f)) {
                    enqueue_param_float_msg(PARAM_FX_DELAY_FEEDBACK, g_app.fx.delay.feedback);
                }
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.delay.mix, 1.0f, 0.01f)) {
                    enqueue_param_float_msg(PARAM_FX_DELAY_MIX, g_app.fx.delay.mix);
                }

                nk_layout_row_dynamic(ctx, 28, 1);
                nk_label(ctx, "Reverb", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.size, 1.0f, 0.01f)) {
                    enqueue_param_float_msg(PARAM_FX_REVERB_SIZE, g_app.fx.reverb.size);
                }
                nk_layout_row_dynamic(ctx, 34, 1);
                if (nk_slider_float(ctx, 0.0f, &g_app.fx.reverb.damping, 1.0f, 0.01f)) {
                    enqueue_param_float_msg(PARAM_FX_REVERB_DAMPING, g_app.fx.reverb.damping);
                }
                nk_layout_row_dynamic(ctx, 34, 1);
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

            nk_layout_row_dynamic(ctx, 300, 1);
            if (nk_group_begin_titled(ctx, "PANEL_MODMATRIX", "Mod Matrix Router", scroll_panel_flags)) {
                static const char* kModSources[] = {"Env Amp", "Env Filter", "LFO Drift", "Velocity", "Keytrack", "Macro 1", "Macro 2"};
                static const char* kModDestinations[] = {"Osc Pitch", "Osc Fine", "Filter Cutoff", "Filter Res", "Drive", "FX Send", "Pan"};
                const int source_count = (int)(sizeof(kModSources) / sizeof(kModSources[0]));
                const int dest_count = (int)(sizeof(kModDestinations) / sizeof(kModDestinations[0]));

                nk_layout_row_begin(ctx, NK_STATIC, 30, 5);
                nk_layout_row_push(ctx, 90);
                nk_label(ctx, "Slot", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 180);
                nk_label(ctx, "Source", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 180);
                nk_label(ctx, "Destination", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 200);
                nk_label(ctx, "Amount", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 150);
                nk_label(ctx, "Visual", NK_TEXT_LEFT);
                nk_layout_row_end(ctx);

                for (int i = 0; i < UI_MOD_SLOT_COUNT; ++i) {
                    nk_layout_row_begin(ctx, NK_STATIC, 44, 5);
                    nk_layout_row_push(ctx, 90);
                    char slot_label[32];
                    snprintf(slot_label, sizeof(slot_label), "Slot %d", i + 1);
                    nk_checkbox_label(ctx, slot_label, &g_app.mod_slots[i].enabled);

                    nk_layout_row_push(ctx, 180);
                    if (g_app.mod_slots[i].source >= source_count) {
                        g_app.mod_slots[i].source = 0;
                    }
                    g_app.mod_slots[i].source = nk_combo(ctx,
                                                         kModSources,
                                                         source_count,
                                                         g_app.mod_slots[i].source,
                                                         28,
                                                         nk_vec2(160, 200));

                    nk_layout_row_push(ctx, 180);
                    if (g_app.mod_slots[i].destination >= dest_count) {
                        g_app.mod_slots[i].destination = 0;
                    }
                    g_app.mod_slots[i].destination = nk_combo(ctx,
                                                              kModDestinations,
                                                              dest_count,
                                                              g_app.mod_slots[i].destination,
                                                              28,
                                                              nk_vec2(160, 200));

                    nk_layout_row_push(ctx, 200);
                    nk_slider_float(ctx, -1.0f, &g_app.mod_slots[i].amount, 1.0f, 0.01f);

                    nk_layout_row_push(ctx, 150);
                    struct nk_rect meter_bounds;
                    enum nk_widget_layout_states meter_state = nk_widget(&meter_bounds, ctx);
                    if (meter_state != NK_WIDGET_INVALID) {
                        struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
                        ui_draw_mod_meter(canvas, meter_bounds, g_app.mod_slots[i].amount);
                    }
                    nk_layout_row_end(ctx);
                }

                nk_layout_row_dynamic(ctx, 28, 1);
                nk_label_wrap(ctx, "Assign macro knobs or envelopes to destinations just like Surge's modulation matrix. Routing is visual only for now.");

                nk_group_end(ctx);
            }

            nk_layout_row_begin(ctx, NK_STATIC, 220, 2);
            nk_layout_row_push(ctx, (content_width - column_gap) * 0.5f);
            if (nk_group_begin_titled(ctx, "PANEL_MACROS", "Macro Console", compact_panel_flags)) {
                static const char* macro_labels[UI_MACRO_COUNT] = {"MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4"};
                static const char* macro_tags[UI_MACRO_COUNT] = {"Brightness Sweep", "Bass Morph", "Texture", "Chaos Send"};
                static const char* macro_targets[UI_MACRO_COUNT] = {"â†’ Filter Cutoff", "â†’ Osc PWM", "â†’ FX Width", "â†’ Delay Mix"};

                nk_layout_row_dynamic(ctx, 26, 1);
                nk_label(ctx, "Macro pages mirror Surge's console â€” each card exposes target hints and live values.", NK_TEXT_LEFT);

                const float macro_panel_width = (content_width - column_gap) * 0.5f;
                const float macro_card_width = fminf(macro_panel_width * 0.48f, 320.0f);

                for (int row = 0; row < UI_MACRO_COUNT; row += 2) {
                    nk_layout_row_begin(ctx, NK_STATIC, 190, 2);
                    for (int col = 0; col < 2; ++col) {
                        const int idx = row + col;
                        nk_layout_row_push(ctx, macro_card_width);
                        if (idx >= UI_MACRO_COUNT) {
                            nk_spacing(ctx, 1);
                            continue;
                        }
                        char card_id[32];
                        snprintf(card_id, sizeof(card_id), "MACRO_CARD_%d", idx);
                        if (nk_group_begin(ctx, card_id, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
                            nk_layout_row_dynamic(ctx, 24, 1);
                            nk_label(ctx, macro_labels[idx], NK_TEXT_LEFT);

                            nk_layout_row_begin(ctx, NK_STATIC, 110, 2);
                            nk_layout_row_push(ctx, macro_card_width * 0.55f);
                            UiKnobConfig macro_cfg = {
                                .label = "",
                                .unit = "",
                                .diameter_override = 80.0f
                            };
                            if (ui_knob_render(ctx, &g_app.knob_macro[idx], &macro_cfg)) {
                                g_app.macro_values[idx] = g_app.knob_macro[idx].value;
                            }
                            nk_layout_row_push(ctx, macro_card_width * 0.35f);
                            char macro_value[32];
                            snprintf(macro_value, sizeof(macro_value), "%3.0f %%", g_app.macro_values[idx] * 100.0f);
                            nk_label(ctx, macro_value, NK_TEXT_LEFT);
                            nk_layout_row_end(ctx);

                            nk_layout_row_dynamic(ctx, 22, 1);
                            nk_size macro_meter = (nk_size)(g_app.macro_values[idx] * 100.0f);
                            nk_progress(ctx, &macro_meter, 100, nk_false);

                            nk_layout_row_dynamic(ctx, 22, 1);
                            nk_label(ctx, macro_targets[idx], NK_TEXT_LEFT);

                            nk_layout_row_dynamic(ctx, 24, 2);
                            nk_label_colored(ctx, macro_tags[idx], NK_TEXT_LEFT, palette->accent);
                            if (nk_button_symbol_label(ctx, NK_SYMBOL_PLUS, "Assign", NK_TEXT_RIGHT)) {
                                // Placeholder: future routing dialog
                            }

                            nk_group_end(ctx);
                        }
                    }
                    nk_layout_row_end(ctx);
                }

                nk_group_end(ctx);
            }

            nk_layout_row_push(ctx, (content_width - column_gap) * 0.5f);
            if (nk_group_begin_titled(ctx, "PANEL_VOICES", "Voice Activity", compact_panel_flags)) {
                const float voice_bar_width = fminf((content_width - column_gap) * 0.3f, 280.0f);
                for (int i = 0; i < MAX_VOICES; ++i) {
                    nk_layout_row_begin(ctx, NK_STATIC, 22, 3);
                    nk_layout_row_push(ctx, 40);
                    char label[8];
                    snprintf(label, sizeof(label), "V%d", i + 1);
                    nk_label(ctx, label, NK_TEXT_LEFT);

                    nk_layout_row_push(ctx, voice_bar_width);
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

            nk_layout_row_dynamic(ctx, 240, 1);
            if (nk_group_begin_titled(ctx, "PANEL_VOICE_LAYERS", "Voice Layers", compact_panel_flags)) {
                for (int i = 0; i < MAX_VOICE_TRACKS; ++i) {
                    const VoiceTrack* track = &g_app.voice_layers.tracks[i];

                    nk_layout_row_dynamic(ctx, 24, 2);
                    char title_buf[32];
                    snprintf(title_buf, sizeof(title_buf), "Track %d", i + 1);
                    nk_label(ctx, title_buf, NK_TEXT_LEFT);

                    char status_buf[96];
                    if (track->recording) {
                        snprintf(status_buf, sizeof(status_buf), "Recordingâ€¦ (%u/%u frames)",
                                 track->recorded_frames, track->record_capacity_frames);
                    } else if (track->playing) {
                        snprintf(status_buf, sizeof(status_buf), "Playing (%.1fs)",
                                 (track->buffer.sample_rate > 0)
                                     ? (track->buffer.frame_count / (float)track->buffer.sample_rate)
                                     : 0.0f);
                    } else if (track->buffer.data && track->buffer.frame_count > 0) {
                        float seconds = (track->buffer.sample_rate > 0)
                                            ? (track->buffer.frame_count / (float)track->buffer.sample_rate)
                                            : 0.0f;
                        snprintf(status_buf, sizeof(status_buf), "Ready (%.1fs)", seconds);
                    } else {
                        snprintf(status_buf, sizeof(status_buf), "Empty");
                    }
                    nk_label(ctx, status_buf, NK_TEXT_RIGHT);

                    nk_layout_row_begin(ctx, NK_STATIC, 28, 4);
                    nk_layout_row_push(ctx, 80);
                    if (track->recording) {
                        if (nk_button_label(ctx, "Stop")) {
                            voice_track_stop_recording(i);
                        }
                    } else {
                        if (nk_button_label(ctx, "Record")) {
                            voice_track_begin_recording(i);
                        }
                    }

                    nk_layout_row_push(ctx, 80);
                    const char* play_label = track->playing ? "Stop Play" : "Play";
                    if (nk_button_label(ctx, play_label)) {
                        voice_track_toggle_play(i);
                    }

                    nk_layout_row_push(ctx, 70);
                    if (nk_button_label(ctx, "Save")) {
                        voice_track_save_to_disk(i);
                    }

                    nk_layout_row_push(ctx, 70);
                    if (nk_button_label(ctx, "Clear")) {
                        voice_track_clear(i);
                    }
                    nk_layout_row_end(ctx);

                    nk_layout_row_dynamic(ctx, 20, 2);
                    nk_label(ctx, "Volume", NK_TEXT_LEFT);
                    float volume = track->volume;
                    if (nk_slider_float(ctx, 0.0f, &volume, 2.0f, 0.01f)) {
                        voice_track_set_volume(i, volume);
                    }

                    if (track->last_saved_path[0]) {
                        nk_layout_row_dynamic(ctx, 18, 1);
                        nk_label(ctx, track->last_saved_path, NK_TEXT_LEFT);
                    }

                    if (i < MAX_VOICE_TRACKS - 1) {
                        nk_layout_row_dynamic(ctx, 4, 1);
                        nk_spacing(ctx, 1);
                    }
                }
                nk_group_end(ctx);
            }

            nk_layout_row_dynamic(ctx, 200, 1);
            if (nk_group_begin_titled(ctx, "PANEL_PERF", "Performance Monitor", compact_panel_flags)) {
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
        nk_label(ctx, "Mouse: click & drag keys â€¢ Keyboard: Z-M / Q-I â€¢ SPACE = Play/Stop â€¢ ESC = Panic", NK_TEXT_CENTERED);
    }
    nk_end(ctx);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n");
    printf("     ðŸŽ¹ PROFESSIONAL SYNTHESIZER ðŸŽ¹\n");
    printf("â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n\n");

    voice_layers_init(&g_app.voice_layers);
    preset_snippet_library_init(&g_app.preset_snippets);
    ma_mutex_init(&g_app.audio_mutex);

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

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize OpenGL context via GLAD\n");
        glfwDestroyWindow(g_app.window);
        glfwTerminate();
        return 1;
    }

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
    snprintf(g_app.patch_name, sizeof(g_app.patch_name), "Init Patch");
    g_app.patch_category = 0;
    g_app.ui_section = 0;
    g_app.active_scene = 0;
    g_app.scenes_linked = 1;
    g_app.scene_blend = 0.5f;
    for (int i = 0; i < UI_MACRO_COUNT; ++i) {
        g_app.macro_values[i] = 0.5f;
    }
    for (int i = 0; i < UI_MOD_SLOT_COUNT; ++i) {
        g_app.mod_slots[i].source = 0;
        g_app.mod_slots[i].destination = 0;
        g_app.mod_slots[i].amount = 0.0f;
        g_app.mod_slots[i].enabled = (i == 0);
    }
    
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
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    config.pUserData = &g_app;
    
    if (ma_device_init(NULL, &config, &g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "âŒ Failed to initialize audio\n");
        return 1;
    }
    g_app.capture_channels = g_app.audio_device.capture.channels;
    
    if (ma_device_start(&g_app.audio_device) != MA_SUCCESS) {
        fprintf(stderr, "âŒ Failed to start audio\n");
        return 1;
    }
    
    printf("âœ… Audio: 44100 Hz, stereo, %.1f ms latency\n", 
           (float)config.periodSizeInFrames / 44100.0f * 1000.0f);
    printf("âœ… GUI: Ready\n");
    printf("âœ… Controls: QWERTY keyboard â†’ Notes\n\n");
    
    // Play test tone to verify audio
    printf("ðŸ”Š Playing test tone (C4)...\n");
    synth_note_on(&g_app.synth, 60, 1.0f);  // C4 (middle C)
    sleep_milliseconds(1000);
    synth_note_off(&g_app.synth, 60);
    printf("âœ… Audio test complete!\n\n");
    
    printf("Ready to play! ðŸŽµ\n\n");
    
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
                printf("ðŸ”‡ Mouse released - stopping note %d\n", g_app.mouse_note_playing);
                g_app.mouse_note_playing = -1;
            }
        }
        g_app.mouse_was_down = (mouse_state == GLFW_PRESS);
        
    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(g_app.window, &fb_width, &fb_height);

    nk_glfw3_new_frame(&g_app.glfw);
        
    draw_gui(g_app.nk_ctx, (float)fb_width, (float)fb_height);
        
    glViewport(0, 0, fb_width, fb_height);
    glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(g_app.bg.r, g_app.bg.g, g_app.bg.b, g_app.bg.a);
        
        nk_glfw3_render(&g_app.glfw, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(g_app.window);
    }
    
    // Cleanup
    midi_input_stop();
    ma_device_uninit(&g_app.audio_device);
    ma_mutex_uninit(&g_app.audio_mutex);
    nk_glfw3_shutdown(&g_app.glfw);
    glfwTerminate();
    
    printf("\nðŸŽ‰ Goodbye!\n");
    return 0;
}

