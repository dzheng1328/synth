/**
 * Synthesizer Engine Implementation
 * Core DSP components
 */

#include "synth_engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * M_PI)

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float midi_to_freq(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float cents_to_ratio(float cents) {
    return powf(2.0f, cents / 1200.0f);
}

float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

float linear_to_db(float linear) {
    return 20.0f * log10f(linear);
}

float soft_clip(float x) {
    // Soft clipping using tanh
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return tanhf(x * 1.5f) / tanhf(1.5f);
}

float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Fast random float between 0.0 and 1.0
static uint32_t rng_state = 123456789;
float fast_rand(void) {
    rng_state = rng_state * 1664525 + 1013904223;
    return (float)(rng_state >> 8) / 16777216.0f;
}

// ============================================================================
// OSCILLATOR IMPLEMENTATION
// ============================================================================

void osc_init(Oscillator* osc, float sample_rate) {
    (void)sample_rate;
    memset(osc, 0, sizeof(Oscillator));
    osc->waveform = WAVE_SAW;
    osc->phase = 0.0f;
    osc->frequency = 440.0f;
    osc->amplitude = 1.0f;
    osc->pulse_width = 0.5f;
    osc->unison_voices = 1;
    osc->detune_cents = 0.0f;
    osc->unison_spread = 0.5f;
    osc->wavetable_size = WAVETABLE_SIZE;
}

void osc_set_waveform(Oscillator* osc, WaveformType type) {
    osc->waveform = type;
}

void osc_set_frequency(Oscillator* osc, float freq) {
    osc->frequency = freq;
}

// Generate basic waveform sample (internal)
static float osc_generate_basic(Oscillator* osc, float phase, float pw) {
    float output = 0.0f;
    
    switch (osc->waveform) {
        case WAVE_SINE:
            output = sinf(phase * TWO_PI);
            break;
            
        case WAVE_SAW:
            output = (phase * 2.0f) - 1.0f;
            break;
            
        case WAVE_SQUARE:
            output = (phase < pw) ? 1.0f : -1.0f;
            break;
            
        case WAVE_TRIANGLE:
            if (phase < 0.5f) {
                output = (phase * 4.0f) - 1.0f;
            } else {
                output = 3.0f - (phase * 4.0f);
            }
            break;
            
        case WAVE_NOISE:
            output = (fast_rand() * 2.0f) - 1.0f;
            break;
            
        case WAVE_WAVETABLE:
            if (osc->wavetable && osc->wavetable_size > 0) {
                float pos = phase * osc->wavetable_size;
                int index = (int)pos;
                float frac = pos - index;
                int next_index = (index + 1) % osc->wavetable_size;
                output = lerp(osc->wavetable[index], 
                            osc->wavetable[next_index], frac);
            } else {
                output = sinf(phase * TWO_PI); // Fallback to sine
            }
            break;
            
        default:
            output = 0.0f;
    }
    
    return output;
}

float osc_process(Oscillator* osc, float sample_rate, float fm_input) {
    float output = 0.0f;
    float base_freq = osc->frequency;
    
    // Apply FM modulation
    if (osc->fm_amount > 0.0f) {
        base_freq *= 1.0f + (fm_input * osc->fm_amount);
    }
    
    // Apply drift (analog feel)
    if (osc->drift_amount > 0.0f) {
        float drift_lfo = sinf(osc->drift_phase * TWO_PI);
        base_freq *= 1.0f + (drift_lfo * osc->drift_amount * 0.01f); // ±1% max
        osc->drift_phase += osc->drift_rate / sample_rate;
        if (osc->drift_phase >= 1.0f) osc->drift_phase -= 1.0f;
    }
    
    // Unison processing
    if (osc->unison_voices > 1) {
        float unison_output = 0.0f;
        
        for (int i = 0; i < osc->unison_voices; i++) {
            // Calculate detune for this voice
            float detune_offset = 0.0f;
            if (i > 0) {
                float spread = ((float)i / (osc->unison_voices - 1)) - 0.5f; // -0.5 to 0.5
                detune_offset = spread * osc->detune_cents * osc->unison_spread;
            }
            
            float voice_freq = base_freq * cents_to_ratio(detune_offset);
            float detune_ratio = voice_freq / fmaxf(base_freq, 0.0001f);
            
            // Calculate phase for this unison voice
            float voice_phase = osc->phase * detune_ratio + (i * osc->phase_offset);
            voice_phase -= floorf(voice_phase);
            
            // Generate sample
            float pw = osc->pulse_width;
            unison_output += osc_generate_basic(osc, voice_phase, pw);
        }
        
        // Average the unison voices
        output = unison_output / osc->unison_voices;
    } else {
        // Single voice
        output = osc_generate_basic(osc, osc->phase, osc->pulse_width);
    }
    
    // Hard sync (sync to sync_phase)
    if (osc->hard_sync) {
        if (osc->sync_phase >= 1.0f) {
            osc->phase = 0.0f;
            osc->sync_phase = 0.0f;
        }
    }
    
    // Advance phase
    float phase_increment = base_freq / sample_rate;
    osc->phase += phase_increment;
    osc->sync_phase += phase_increment;
    
    // Wrap phase
    if (osc->phase >= 1.0f) osc->phase -= 1.0f;
    if (osc->sync_phase >= 1.0f) osc->sync_phase -= 1.0f;
    
    // Apply amplitude
    output *= osc->amplitude;
    
    // Ring modulation (multiply with FM input)
    if (osc->rm_amount > 0.0f) {
        output = lerp(output, output * fm_input, osc->rm_amount);
    }
    
    return output;
}

// ============================================================================
// FILTER IMPLEMENTATION
// ============================================================================

void filter_init(Filter* filter, float sample_rate) {
    memset(filter, 0, sizeof(Filter));
    filter->mode = FILTER_LP;
    filter->cutoff = 1000.0f;
    filter->resonance = 0.0f;
    filter->keytrack = 0.0f;
    filter->env_amount = 0.0f;
    filter->cutoff_actual = filter->cutoff;
    filter->resonance_actual = filter->resonance;
    filter_update_coefficients(filter, sample_rate, filter->cutoff, filter->resonance);
}

void filter_set_mode(Filter* filter, FilterMode mode) {
    filter->mode = mode;
}

void filter_update_coefficients(Filter* filter, float sample_rate, float cutoff, float resonance) {
    filter->cutoff_actual = clamp(cutoff, 20.0f, sample_rate * 0.45f);
    filter->resonance_actual = clamp(resonance, 0.0f, 0.99f);

    float freq = filter->cutoff_actual;
    filter->f = 2.0f * sinf(M_PI * freq / sample_rate);
    filter->q = clamp(1.0f - filter->resonance_actual, 0.1f, 1.0f);
}

float filter_process(Filter* filter, float input) {
    // State variable filter (Chamberlin/Hal Chamberlin)
    filter->low += filter->f * filter->band;
    filter->high = input - filter->low - (filter->q * filter->band);
    filter->band += filter->f * filter->high;
    filter->notch = filter->high + filter->low;
    
    // Select output based on mode
    float output = 0.0f;
    switch (filter->mode) {
        case FILTER_LP:
            output = filter->low;
            break;
        case FILTER_HP:
            output = filter->high;
            break;
        case FILTER_BP:
            output = filter->band;
            break;
        case FILTER_NOTCH:
            output = filter->notch;
            break;
        case FILTER_ALLPASS:
            output = filter->low - filter->high;
            break;
        default:
            output = input;
    }
    
    return output;
}

// ============================================================================
// ENVELOPE IMPLEMENTATION
// ============================================================================

void envelope_init(Envelope* env) {
    memset(env, 0, sizeof(Envelope));
    env->attack = 0.01f;
    env->decay = 0.1f;
    env->sustain = 0.7f;
    env->release = 0.3f;
    env->velocity_sensitivity = 0.5f;
    env->state = ENV_OFF;
    env->current_level = 0.0f;
}

void envelope_trigger(Envelope* env, float velocity) {
    env->velocity = velocity;
    env->state = ENV_ATTACK;
    env->target_level = 1.0f;
    
    // Apply velocity sensitivity
    float vel_mod = lerp(1.0f, velocity, env->velocity_sensitivity);
    env->target_level *= vel_mod;
    
    // Retrigger resets level to 0
    if (env->retrigger) {
        env->current_level = 0.0f;
    }
}

void envelope_release(Envelope* env) {
    env->state = ENV_RELEASE;
    env->target_level = 0.0f;
}

float envelope_process(Envelope* env, float sample_rate) {
    if (env->state == ENV_OFF) {
        return 0.0f;
    }
    
    float rate = 0.0f;
    
    switch (env->state) {
        case ENV_ATTACK:
            if (env->attack > 0.0f) {
                rate = 1.0f / (env->attack * sample_rate);
                env->current_level += rate;
                if (env->current_level >= env->target_level) {
                    env->current_level = env->target_level;
                    env->state = ENV_DECAY;
                }
            } else {
                env->current_level = env->target_level;
                env->state = ENV_DECAY;
            }
            break;
            
        case ENV_DECAY:
            if (env->decay > 0.0f) {
                rate = (env->target_level - env->sustain) / (env->decay * sample_rate);
                env->current_level -= rate;
                if (env->current_level <= env->sustain) {
                    env->current_level = env->sustain;
                    env->state = ENV_SUSTAIN;
                }
            } else {
                env->current_level = env->sustain;
                env->state = ENV_SUSTAIN;
            }
            break;
            
        case ENV_SUSTAIN:
            env->current_level = env->sustain;
            
            // Loop back to attack if looping enabled
            if (env->loop) {
                env->state = ENV_ATTACK;
                if (env->retrigger) {
                    env->current_level = 0.0f;
                }
            }
            break;
            
        case ENV_RELEASE:
            if (env->release > 0.0f) {
                rate = env->current_level / (env->release * sample_rate);
                env->current_level -= rate;
                if (env->current_level <= 0.0f) {
                    env->current_level = 0.0f;
                    env->state = ENV_OFF;
                }
            } else {
                env->current_level = 0.0f;
                env->state = ENV_OFF;
            }
            break;
            
        default:
            break;
    }
    
    return clamp(env->current_level, 0.0f, 1.0f);
}

bool envelope_is_active(Envelope* env) {
    return env->state != ENV_OFF;
}

// ============================================================================
// LFO IMPLEMENTATION
// ============================================================================

void lfo_init(LFO* lfo) {
    memset(lfo, 0, sizeof(LFO));
    lfo->waveform = WAVE_SINE;
    lfo->rate = 2.0f; // 2 Hz
    lfo->amount = 0.5f;
    lfo->phase = 0.0f;
    lfo->tempo_division = 1.0f; // Quarter note
    lfo->bipolar = true;
    lfo->fade_level = 1.0f;
}

void lfo_trigger(LFO* lfo) {
    if (lfo->key_sync) {
        lfo->phase = 0.0f;
    }
    lfo->fade_level = 0.0f;
}

float lfo_process(LFO* lfo, float sample_rate, float tempo) {
    // Calculate effective rate
    float rate = lfo->rate;
    if (lfo->tempo_sync && tempo > 0.0f) {
        // Convert BPM to Hz for given tempo division
        // Quarter note at 120 BPM = 2 Hz
        rate = (tempo / 60.0f) * lfo->tempo_division;
    }
    
    // Update fade-in
    if (lfo->fade_level < 1.0f && lfo->fade_time > 0.0f) {
        lfo->fade_level += 1.0f / (lfo->fade_time * sample_rate);
        if (lfo->fade_level > 1.0f) lfo->fade_level = 1.0f;
    }
    
    // Generate waveform (reuse oscillator logic)
    float output = 0.0f;
    switch (lfo->waveform) {
        case WAVE_SINE:
            output = sinf(lfo->phase * TWO_PI);
            break;
        case WAVE_TRIANGLE:
            output = fabsf((lfo->phase * 4.0f) - 2.0f) - 1.0f;
            break;
        case WAVE_SAW:
            output = (lfo->phase * 2.0f) - 1.0f;
            break;
        case WAVE_SQUARE:
            output = (lfo->phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVE_NOISE:
            output = (fast_rand() * 2.0f) - 1.0f;
            break;
        default:
            output = 0.0f;
    }
    
    // Convert to unipolar if needed
    if (!lfo->bipolar) {
        output = (output + 1.0f) * 0.5f; // 0.0 to 1.0
    }
    
    // Apply amount and fade
    output *= lfo->amount * lfo->fade_level;
    
    // Advance phase
    lfo->phase += rate / sample_rate;
    if (lfo->phase >= 1.0f) lfo->phase -= 1.0f;
    
    return output;
}

// ============================================================================
// MODULATION MATRIX IMPLEMENTATION
// ============================================================================

void mod_matrix_init(ModulationMatrix* matrix) {
    memset(matrix, 0, sizeof(ModulationMatrix));
}

void mod_matrix_add_slot(ModulationMatrix* matrix, ModSource source, 
                         ModDestination dest, float amount) {
    if (matrix->num_slots < MAX_MOD_SLOTS) {
        ModSlot* slot = &matrix->slots[matrix->num_slots];
        slot->source = source;
        slot->destination = dest;
        slot->amount = amount;
        slot->enabled = true;
        matrix->num_slots++;
    }
}

void mod_matrix_update_sources(ModulationMatrix* matrix, SynthEngine* synth) {
    if (!matrix || !synth) {
        return;
    }

    for (int i = 0; i < MOD_SOURCE_COUNT; i++) {
        matrix->source_values[i] = 0.0f;
    }

    for (int i = 0; i < MAX_LFO && (MOD_SOURCE_LFO1 + i) < MOD_SOURCE_COUNT; i++) {
        matrix->source_values[MOD_SOURCE_LFO1 + i] =
            lfo_process(&synth->lfos[i], synth->sample_rate, synth->tempo);
    }

    float velocity_sum = 0.0f;
    float keytrack_sum = 0.0f;
    int active = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice* voice = &synth->voices[i];
        if (voice_is_active(voice)) {
            velocity_sum += voice->velocity;
            float normalized_note = (float)(voice->midi_note - 60) / 36.0f;
            keytrack_sum += normalized_note;
            active++;
        }
    }

    if (active > 0) {
        matrix->source_values[MOD_SOURCE_VELOCITY] = clamp(velocity_sum / active, 0.0f, 1.0f);
        matrix->source_values[MOD_SOURCE_KEYTRACK] = clamp(keytrack_sum / active, -1.0f, 1.0f);
    }

    matrix->source_values[MOD_SOURCE_RANDOM] = (fast_rand() * 2.0f) - 1.0f;
}

float mod_matrix_get_value(ModulationMatrix* matrix, ModDestination dest) {
    float total = 0.0f;
    
    for (int i = 0; i < matrix->num_slots; i++) {
        ModSlot* slot = &matrix->slots[i];
        if (slot->enabled && slot->destination == dest) {
            float source_val = matrix->source_values[slot->source];
            total += source_val * slot->amount;
        }
    }
    
    return clamp(total, -1.0f, 1.0f);
}

// ============================================================================
// VOICE IMPLEMENTATION
// ============================================================================

void voice_init(Voice* voice, float sample_rate) {
    memset(voice, 0, sizeof(Voice));
    voice->state = VOICE_OFF;
    
    osc_init(&voice->osc1, sample_rate);
    osc_init(&voice->osc2, sample_rate);
    filter_init(&voice->filter, sample_rate);
    envelope_init(&voice->env_amp);
    envelope_init(&voice->env_filter);
    envelope_init(&voice->env_pitch);
    
    voice->pan = 0.0f; // Center
    voice->random_value = fast_rand();
}

void voice_note_on(Voice* voice, int midi_note, float velocity, uint64_t time) {
    voice->state = VOICE_ATTACK;
    voice->midi_note = midi_note;
    voice->frequency = midi_to_freq(midi_note);
    voice->velocity = velocity;
    voice->note_on_time = time;
    voice->target_pitch = voice->frequency;
    
    // Trigger envelopes
    envelope_trigger(&voice->env_amp, velocity);
    envelope_trigger(&voice->env_filter, velocity);
    envelope_trigger(&voice->env_pitch, velocity);
    
    // Reset oscillator phases if phase_reset is enabled
    if (voice->osc1.phase_reset) {
        voice->osc1.phase = 0.0f;
    }
    if (voice->osc2.phase_reset) {
        voice->osc2.phase = 0.0f;
    }
    
    // Set initial pitch for glide
    if (voice->glide_rate <= 0.0f) {
        voice->current_pitch = voice->target_pitch;
    }
}

void voice_note_off(Voice* voice, uint64_t time) {
    voice->state = VOICE_RELEASE;
    voice->note_off_time = time;
    
    // Release envelopes
    envelope_release(&voice->env_amp);
    envelope_release(&voice->env_filter);
    envelope_release(&voice->env_pitch);
}

void voice_process(Voice* voice, float* left, float* right, float sample_rate) {
    if (voice->state == VOICE_OFF) {
        *left = 0.0f;
        *right = 0.0f;
        return;
    }
    
    // Process envelopes
    float env_amp = envelope_process(&voice->env_amp, sample_rate);
    float env_filter = envelope_process(&voice->env_filter, sample_rate);
    float env_pitch = envelope_process(&voice->env_pitch, sample_rate);
    
    // Check if voice is done
    if (!envelope_is_active(&voice->env_amp)) {
        voice->state = VOICE_OFF;
        *left = 0.0f;
        *right = 0.0f;
        return;
    }
    
    // Handle glide/portamento
    if (voice->glide_rate > 0.0f && voice->current_pitch != voice->target_pitch) {
        float glide_speed = 12.0f / (voice->glide_rate * sample_rate); // Semitones per sample
        
        if (voice->current_pitch < voice->target_pitch) {
            voice->current_pitch *= powf(2.0f, glide_speed / 12.0f);
            if (voice->current_pitch >= voice->target_pitch) {
                voice->current_pitch = voice->target_pitch;
            }
        } else {
            voice->current_pitch *= powf(2.0f, -glide_speed / 12.0f);
            if (voice->current_pitch <= voice->target_pitch) {
                voice->current_pitch = voice->target_pitch;
            }
        }
    } else {
        voice->current_pitch = voice->target_pitch;
    }
    
    // Set oscillator frequencies with pitch envelope
    float pitch_mod = 1.0f + (env_pitch * 0.1f); // ±10% max from envelope
    voice->osc1.frequency = voice->current_pitch * pitch_mod;
    voice->osc2.frequency = voice->current_pitch * pitch_mod;
    
    // Process oscillators (osc1 can modulate osc2)
    float osc1_out = osc_process(&voice->osc1, sample_rate, 0.0f);
    float osc2_out = osc_process(&voice->osc2, sample_rate, osc1_out);
    
    // Mix oscillators
    float mixed = (osc1_out + osc2_out) * 0.5f;
    
    // Apply filter with envelope modulation
    float filter_cutoff = voice->filter.cutoff;
    filter_cutoff *= 1.0f + (env_filter * voice->filter.env_amount * 10.0f); // Up to 10x modulation
    filter_cutoff = clamp(filter_cutoff, 20.0f, sample_rate * 0.45f);
    float filter_resonance = clamp(voice->filter.resonance, 0.0f, 0.99f);

    if (fabsf(filter_cutoff - voice->filter.cutoff_actual) > 1.0f ||
        fabsf(filter_resonance - voice->filter.resonance_actual) > 0.001f) {
        filter_update_coefficients(&voice->filter, sample_rate, filter_cutoff, filter_resonance);
    }
    float filtered = filter_process(&voice->filter, mixed);
    
    // Apply amplitude envelope
    float output = filtered * env_amp * voice->velocity;
    
    // Apply panning (constant power)
    float pan_angle = (voice->pan + 1.0f) * 0.25f * M_PI; // -1..1 -> 0..PI/2
    *left = output * cosf(pan_angle);
    *right = output * sinf(pan_angle);
}

bool voice_is_active(Voice* voice) {
    return voice->state != VOICE_OFF;
}

// ============================================================================
// SYNTH ENGINE IMPLEMENTATION
// ============================================================================

void synth_init(SynthEngine* synth, float sample_rate) {
    memset(synth, 0, sizeof(SynthEngine));
    
    synth->sample_rate = sample_rate;
    synth->tempo = 120.0f;
    synth->master_volume = 0.7f;
    synth->master_tune = 0.0f;
    synth->pitch_bend_range = 2; // ±2 semitones
    
    synth->mono_mode = false;
    synth->legato_mode = false;
    synth->glide_time = 0.0f;

    synth->filter_cutoff = 8000.0f;
    synth->filter_resonance = 0.3f;
    synth->filter_mode = FILTER_LP;
    synth->filter_env_amount = 0.0f;

    synth->env_attack = 0.01f;
    synth->env_decay = 0.1f;
    synth->env_sustain = 0.7f;
    synth->env_release = 0.3f;
    
    synth->limiter_threshold = 0.95f;
    synth->limiter_release = 0.1f;
    synth->limiter_gain = 1.0f;
    
    // Initialize voices
    for (int i = 0; i < MAX_VOICES; i++) {
        voice_init(&synth->voices[i], sample_rate);
        synth->voices[i].filter.cutoff = synth->filter_cutoff;
        synth->voices[i].filter.resonance = synth->filter_resonance;
        synth->voices[i].filter.mode = synth->filter_mode;
        synth->voices[i].filter.env_amount = synth->filter_env_amount;

        synth->voices[i].env_amp.attack = synth->env_attack;
        synth->voices[i].env_amp.decay = synth->env_decay;
        synth->voices[i].env_amp.sustain = synth->env_sustain;
        synth->voices[i].env_amp.release = synth->env_release;
    }
    
    // Initialize LFOs
    for (int i = 0; i < MAX_LFO; i++) {
        lfo_init(&synth->lfos[i]);
    }
    
    // Initialize modulation matrix
    mod_matrix_init(&synth->mod_matrix);
    
    // Seed random
    rng_state = (uint32_t)time(NULL);
}

void synth_set_tempo(SynthEngine* synth, float bpm) {
    synth->tempo = clamp(bpm, 20.0f, 300.0f);
}

// Find a free voice or steal the oldest
static Voice* synth_allocate_voice(SynthEngine* synth) {
    // First, look for a free voice
    for (int i = 0; i < MAX_VOICES; i++) {
        if (synth->voices[i].state == VOICE_OFF) {
            return &synth->voices[i];
        }
    }
    
    // No free voices, find the oldest playing voice
    Voice* oldest = &synth->voices[0];
    uint64_t oldest_time = synth->voices[0].note_on_time;
    
    for (int i = 1; i < MAX_VOICES; i++) {
        if (synth->voices[i].note_on_time < oldest_time) {
            oldest = &synth->voices[i];
            oldest_time = synth->voices[i].note_on_time;
        }
    }
    
    return oldest;
}

void synth_note_on(SynthEngine* synth, int note, float velocity) {
    // Mono mode handling
    if (synth->mono_mode) {
        // In mono mode, use first voice only
        Voice* voice = &synth->voices[0];
        
        if (synth->legato_mode && voice_is_active(voice)) {
            // Legato: change pitch without retriggering envelopes
            voice->midi_note = note;
            voice->target_pitch = midi_to_freq(note);
        } else {
            // Normal mono: retrigger
            voice_note_on(voice, note, velocity, synth->sample_counter);
        }
        
        voice->glide_rate = synth->glide_time;
    } else {
        // Polyphonic mode: allocate a voice
        Voice* voice = synth_allocate_voice(synth);
        voice_note_on(voice, note, velocity, synth->sample_counter);
        voice->glide_rate = synth->glide_time;
    }
}

void synth_note_off(SynthEngine* synth, int note) {
    // Find and release all voices playing this note
    for (int i = 0; i < MAX_VOICES; i++) {
        if (synth->voices[i].midi_note == note && 
            synth->voices[i].state != VOICE_OFF &&
            synth->voices[i].state != VOICE_RELEASE) {
            voice_note_off(&synth->voices[i], synth->sample_counter);
        }
    }
}

void synth_all_notes_off(SynthEngine* synth) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voice_is_active(&synth->voices[i])) {
            voice_note_off(&synth->voices[i], synth->sample_counter);
        }
    }
}

void synth_pitch_bend(SynthEngine* synth, float amount) {
    // amount is -1.0 to 1.0
    float bend_semitones = amount * synth->pitch_bend_range;
    float bend_ratio = cents_to_ratio(bend_semitones * 100.0f);
    
    // Apply to all active voices
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voice_is_active(&synth->voices[i])) {
            synth->voices[i].pitch_bend = amount;
            synth->voices[i].target_pitch = midi_to_freq(synth->voices[i].midi_note) * bend_ratio;
        }
    }
}

bool synth_engine_apply_param(SynthEngine* synth, const ParamMsg* msg) {
    if (!synth || !msg) {
        return false;
    }

    ParamId id = (ParamId)msg->id;
    switch (id) {
        case PARAM_MASTER_VOLUME: {
            float volume = clamp(param_msg_get_float(msg), 0.0f, 1.0f);
            synth->master_volume = volume;
            return true;
        }
        case PARAM_TEMPO: {
            float bpm = param_msg_get_float(msg);
            synth_set_tempo(synth, bpm);
            return true;
        }
        case PARAM_FILTER_CUTOFF: {
            float cutoff = clamp(param_msg_get_float(msg), 20.0f, 20000.0f);
            synth->filter_cutoff = cutoff;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].filter.cutoff = cutoff;
            }
            return true;
        }
        case PARAM_FILTER_RESONANCE: {
            float resonance = clamp(param_msg_get_float(msg), 0.0f, 1.0f);
            synth->filter_resonance = resonance;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].filter.resonance = resonance;
            }
            return true;
        }
        case PARAM_FILTER_MODE: {
            int mode = param_msg_get_int(msg);
            if (mode < FILTER_LP) mode = FILTER_LP;
            if (mode >= FILTER_COUNT) mode = FILTER_COUNT - 1;
            synth->filter_mode = (FilterMode)mode;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].filter.mode = synth->filter_mode;
            }
            return true;
        }
        case PARAM_FILTER_ENV_AMOUNT: {
            float amount = clamp(param_msg_get_float(msg), -1.0f, 1.0f);
            synth->filter_env_amount = amount;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].filter.env_amount = amount;
            }
            return true;
        }
        case PARAM_ENV_AMP_ATTACK: {
            float attack = clamp(param_msg_get_float(msg), 0.001f, 2.0f);
            synth->env_attack = attack;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].env_amp.attack = attack;
            }
            return true;
        }
        case PARAM_ENV_AMP_DECAY: {
            float decay = clamp(param_msg_get_float(msg), 0.001f, 2.0f);
            synth->env_decay = decay;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].env_amp.decay = decay;
            }
            return true;
        }
        case PARAM_ENV_AMP_SUSTAIN: {
            float sustain = clamp(param_msg_get_float(msg), 0.0f, 1.0f);
            synth->env_sustain = sustain;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].env_amp.sustain = sustain;
            }
            return true;
        }
        case PARAM_ENV_AMP_RELEASE: {
            float release = clamp(param_msg_get_float(msg), 0.001f, 5.0f);
            synth->env_release = release;
            for (int i = 0; i < MAX_VOICES; ++i) {
                synth->voices[i].env_amp.release = release;
            }
            return true;
        }
        case PARAM_PANIC:
            synth_all_notes_off(synth);
            return true;
        default:
            return false;
    }
}

void synth_process(SynthEngine* synth, float* output, int num_frames) {
    // Process audio buffer
    for (int frame = 0; frame < num_frames; frame++) {
        float left = 0.0f;
        float right = 0.0f;
        
        mod_matrix_update_sources(&synth->mod_matrix, synth);
        
        // Process each voice
        int active_voices = 0;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voice_is_active(&synth->voices[i])) {
                float voice_left, voice_right;
                voice_process(&synth->voices[i], &voice_left, &voice_right, synth->sample_rate);
                
                left += voice_left;
                right += voice_right;
                active_voices++;
            }
        }
        
        // Mix down
        if (active_voices > 0) {
            float scale = 1.0f / sqrtf((float)active_voices); // Energy-preserving mix
            left *= scale;
            right *= scale;
        }
        
        // Apply master volume
        left *= synth->master_volume;
        right *= synth->master_volume;
        
        // Simple limiter
        float peak = fmaxf(fabsf(left), fabsf(right));
        if (peak > synth->limiter_threshold) {
            float target_gain = synth->limiter_threshold / peak;
            synth->limiter_gain = fminf(synth->limiter_gain, target_gain);
        } else {
            // Release
            float release_coeff = 1.0f - expf(-1.0f / (synth->limiter_release * synth->sample_rate));
            synth->limiter_gain += (1.0f - synth->limiter_gain) * release_coeff;
        }
        
        left *= synth->limiter_gain;
        right *= synth->limiter_gain;
        
        // Soft clip to prevent digital clipping
        left = soft_clip(left);
        right = soft_clip(right);
        
        // Write output (interleaved stereo)
        output[frame * 2 + 0] = left;
        output[frame * 2 + 1] = right;
        
        synth->sample_counter++;
    }
}
