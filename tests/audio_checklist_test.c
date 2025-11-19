#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synth_engine.h"

#define SAMPLE_RATE 44100
#define SHORT_FRAMES 4096
#define LONG_FRAMES (SAMPLE_RATE * 3)

typedef struct {
    float rms;
    float peak;
} BufferStats;

typedef struct {
    const char* name;
    bool passed;
    char detail[256];
} TestResult;

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

typedef struct {
    bool enabled;
    float drive;
    float mix;
} TestDistortion;

static void fx_distortion_process_buffer(TestDistortion* fx, float* buffer, int frames) {
    if (!fx->enabled) {
        return;
    }
    for (int i = 0; i < frames; i++) {
        float left = buffer[i * 2];
        float right = buffer[i * 2 + 1];
        float left_wet = tanhf(left * fx->drive);
        float right_wet = tanhf(right * fx->drive);
        buffer[i * 2] = lerpf(left, left_wet, fx->mix);
        buffer[i * 2 + 1] = lerpf(right, right_wet, fx->mix);
    }
}

typedef struct {
    bool enabled;
    float* buffer_l;
    float* buffer_r;
    int size;
    int write_pos;
    int delay_samples;
    float feedback;
    float mix;
} TestDelay;

static void fx_delay_init(TestDelay* fx, float max_seconds) {
    memset(fx, 0, sizeof(TestDelay));
    fx->size = (int)(SAMPLE_RATE * max_seconds);
    if (fx->size < 2) {
        fx->size = 2;
    }
    fx->buffer_l = (float*)calloc(fx->size, sizeof(float));
    fx->buffer_r = (float*)calloc(fx->size, sizeof(float));
}

static void fx_delay_free(TestDelay* fx) {
    free(fx->buffer_l);
    free(fx->buffer_r);
    fx->buffer_l = fx->buffer_r = NULL;
}

static void fx_delay_set(TestDelay* fx, float time_seconds, float feedback, float mix) {
    fx->delay_samples = (int)(time_seconds * SAMPLE_RATE);
    if (fx->delay_samples >= fx->size) {
        fx->delay_samples = fx->size - 1;
    }
    if (fx->delay_samples < 1) {
        fx->delay_samples = 1;
    }
    fx->feedback = feedback;
    fx->mix = mix;
}

static void fx_delay_process_buffer(TestDelay* fx, float* buffer, int frames) {
    if (!fx->enabled || !fx->buffer_l || !fx->buffer_r) {
        return;
    }
    for (int i = 0; i < frames; i++) {
        float left = buffer[i * 2];
        float right = buffer[i * 2 + 1];

        int read_pos = fx->write_pos - fx->delay_samples;
        if (read_pos < 0) {
            read_pos += fx->size;
        }

        float delayed_l = fx->buffer_l[read_pos];
        float delayed_r = fx->buffer_r[read_pos];

        fx->buffer_l[fx->write_pos] = left + delayed_l * fx->feedback;
        fx->buffer_r[fx->write_pos] = right + delayed_r * fx->feedback;

        buffer[i * 2] = lerpf(left, delayed_l, fx->mix);
        buffer[i * 2 + 1] = lerpf(right, delayed_r, fx->mix);

        fx->write_pos = (fx->write_pos + 1) % fx->size;
    }
}

typedef struct {
    bool enabled;
    float* buffer;
    int size;
    int write_pos;
    int tap_offset;
    float damping;
    float mix;
} TestReverb;

static void fx_reverb_init(TestReverb* fx, float max_seconds) {
    memset(fx, 0, sizeof(TestReverb));
    fx->size = (int)(SAMPLE_RATE * max_seconds);
    if (fx->size < 2) {
        fx->size = 2;
    }
    fx->buffer = (float*)calloc(fx->size, sizeof(float));
}

static void fx_reverb_free(TestReverb* fx) {
    free(fx->buffer);
    fx->buffer = NULL;
}

static void fx_reverb_set(TestReverb* fx, float size_factor, float damping, float mix) {
    if (size_factor < 0.1f) size_factor = 0.1f;
    if (size_factor > 0.95f) size_factor = 0.95f;
    fx->tap_offset = (int)(size_factor * (float)fx->size);
    if (fx->tap_offset < 1) {
        fx->tap_offset = 1;
    }
    fx->damping = damping;
    fx->mix = mix;
}

static void fx_reverb_process_buffer(TestReverb* fx, float* buffer, int frames) {
    if (!fx->enabled || !fx->buffer) {
        return;
    }
    for (int i = 0; i < frames; i++) {
        float input = 0.5f * (buffer[i * 2] + buffer[i * 2 + 1]);
        fx->buffer[fx->write_pos] = input + fx->buffer[fx->write_pos] * fx->damping;

        int read_pos = fx->write_pos - fx->tap_offset;
        if (read_pos < 0) {
            read_pos += fx->size;
        }

        float reverb_out = fx->buffer[read_pos];
        buffer[i * 2] = lerpf(buffer[i * 2], reverb_out, fx->mix);
        buffer[i * 2 + 1] = lerpf(buffer[i * 2 + 1], reverb_out, fx->mix);

        fx->write_pos = (fx->write_pos + 1) % fx->size;
    }
}

static BufferStats compute_stats(const float* buffer, int frames) {
    BufferStats stats = {0};
    double sum_sq = 0.0;
    float peak = 0.0f;
    int total_samples = frames * 2;
    for (int i = 0; i < frames; i++) {
        float left = buffer[i * 2];
        float right = buffer[i * 2 + 1];
        sum_sq += (double)left * (double)left;
        sum_sq += (double)right * (double)right;
        float frame_peak = fmaxf(fabsf(left), fabsf(right));
        if (frame_peak > peak) {
            peak = frame_peak;
        }
    }
    if (total_samples > 0) {
        stats.rms = (float)sqrt(sum_sq / total_samples);
    }
    stats.peak = peak;
    return stats;
}

static void set_all_waveforms(SynthEngine* synth, WaveformType wave) {
    for (int i = 0; i < MAX_VOICES; i++) {
        synth->voices[i].osc1.waveform = wave;
        synth->voices[i].osc2.waveform = wave;
    }
}

static void set_unison(SynthEngine* synth, int voices, float detune) {
    for (int i = 0; i < MAX_VOICES; i++) {
        synth->voices[i].osc1.unison_voices = voices;
        synth->voices[i].osc1.detune_cents = detune;
        synth->voices[i].osc2.unison_voices = voices;
        synth->voices[i].osc2.detune_cents = detune;
    }
}

static void set_filter(SynthEngine* synth, float cutoff, float resonance) {
    for (int i = 0; i < MAX_VOICES; i++) {
        synth->voices[i].filter.cutoff = cutoff;
        synth->voices[i].filter.resonance = resonance;
        synth->voices[i].filter.low = 0.0f;
        synth->voices[i].filter.high = 0.0f;
        synth->voices[i].filter.band = 0.0f;
        synth->voices[i].filter.notch = 0.0f;
    }
}

static void set_amp_env(SynthEngine* synth, float attack, float decay, float sustain, float release) {
    for (int i = 0; i < MAX_VOICES; i++) {
        synth->voices[i].env_amp.attack = attack;
        synth->voices[i].env_amp.decay = decay;
        synth->voices[i].env_amp.sustain = sustain;
        synth->voices[i].env_amp.release = release;
    }
}

static int frames_from_seconds(float seconds) {
    return (int)(seconds * SAMPLE_RATE);
}

static void render_note_buffer(WaveformType wave, float seconds, float master_volume, float* buffer) {
    int frames = frames_from_seconds(seconds);
    SynthEngine synth;
    synth_init(&synth, (float)SAMPLE_RATE);
    synth.master_volume = master_volume;
    set_all_waveforms(&synth, wave);

    synth_note_on(&synth, 60, 1.0f);
    synth_process(&synth, buffer, frames);
    synth_note_off(&synth, 60);
}

static BufferStats render_note(WaveformType wave, float seconds, float master_volume) {
    int frames = frames_from_seconds(seconds);
    float* buffer = (float*)calloc(frames * 2, sizeof(float));
    render_note_buffer(wave, seconds, master_volume, buffer);
    BufferStats stats = compute_stats(buffer, frames);
    free(buffer);
    return stats;
}

static float average_abs_difference(const float* a, const float* b, int frames) {
    int samples = frames * 2;
    double sum = 0.0;
    for (int i = 0; i < samples; i++) {
        sum += fabsf(a[i] - b[i]);
    }
    return (float)(sum / samples);
}

static float segment_rms_seconds(const float* buffer, int total_frames, float start_seconds, float duration_seconds) {
    if (duration_seconds <= 0.0f) {
        return 0.0f;
    }
    int start_frame = frames_from_seconds(start_seconds);
    int segment_frames = frames_from_seconds(duration_seconds);
    if (start_frame >= total_frames) {
        return 0.0f;
    }
    if (start_frame + segment_frames > total_frames) {
        segment_frames = total_frames - start_frame;
    }
    if (segment_frames <= 0) {
        return 0.0f;
    }
    const float* segment = buffer + start_frame * 2;
    BufferStats stats = compute_stats(segment, segment_frames);
    return stats.rms;
}

static void render_note_with_release(float* buffer,
                                     int total_frames,
                                     int sustain_frames,
                                     WaveformType wave,
                                     float attack,
                                     float decay,
                                     float sustain,
                                     float release) {
    SynthEngine synth;
    synth_init(&synth, (float)SAMPLE_RATE);
    synth.master_volume = 0.8f;
    set_all_waveforms(&synth, wave);
    set_amp_env(&synth, attack, decay, sustain, release);

    memset(buffer, 0, sizeof(float) * total_frames * 2);

    synth_note_on(&synth, 60, 1.0f);
    if (sustain_frames > total_frames) {
        sustain_frames = total_frames;
    }
    if (sustain_frames > 0) {
        synth_process(&synth, buffer, sustain_frames);
    }

    synth_note_off(&synth, 60);

    if (sustain_frames < total_frames) {
        synth_process(&synth,
                      buffer + sustain_frames * 2,
                      total_frames - sustain_frames);
    }
}

static TestResult test_single_note(void) {
    TestResult result = {.name = "Single note (sine)"};
    BufferStats stats = render_note(WAVE_SINE, 0.2f, 0.8f);
    bool pass = stats.peak > 0.01f && stats.rms > 0.001f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "peak=%.3f rms=%.3f", stats.peak, stats.rms);
    return result;
}

static TestResult test_waveform_energy(void) {
    TestResult result = {.name = "Saw vs sine energy"};
    int frames = frames_from_seconds(0.2f);
    float* sine_buf = (float*)calloc(frames * 2, sizeof(float));
    float* saw_buf = (float*)calloc(frames * 2, sizeof(float));

    render_note_buffer(WAVE_SINE, 0.2f, 0.8f, sine_buf);
    render_note_buffer(WAVE_SAW, 0.2f, 0.8f, saw_buf);
    BufferStats sine_stats = compute_stats(sine_buf, frames);
    BufferStats saw_stats = compute_stats(saw_buf, frames);
    float diff = average_abs_difference(sine_buf, saw_buf, frames);

    free(sine_buf);
    free(saw_buf);

    bool pass = diff > 0.01f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "diff=%.3f saw_rms=%.3f sine_rms=%.3f", diff, saw_stats.rms, sine_stats.rms);
    return result;
}

static TestResult test_unison(void) {
    TestResult result = {.name = "Unison thickness"};
    int frames = SHORT_FRAMES;
    float* mono_buf = (float*)calloc(frames * 2, sizeof(float));
    float* unison_buf = (float*)calloc(frames * 2, sizeof(float));

    SynthEngine synth;
    synth_init(&synth, (float)SAMPLE_RATE);
    synth.master_volume = 0.8f;
    set_all_waveforms(&synth, WAVE_SAW);
    set_unison(&synth, 1, 0.0f);
    synth_note_on(&synth, 60, 1.0f);
    synth_process(&synth, mono_buf, frames);

    set_unison(&synth, 5, 15.0f);
    synth_note_off(&synth, 60);
    synth_note_on(&synth, 60, 1.0f);
    synth_process(&synth, unison_buf, frames);
    synth_note_off(&synth, 60);

    BufferStats mono_stats = compute_stats(mono_buf, frames);
    BufferStats unison_stats = compute_stats(unison_buf, frames);
    float diff = average_abs_difference(mono_buf, unison_buf, frames);

    free(mono_buf);
    free(unison_buf);

    bool pass = diff > 0.005f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "diff=%.3f mono_peak=%.3f unison_peak=%.3f", diff, mono_stats.peak, unison_stats.peak);
    return result;
}

static TestResult test_filter_sweep(void) {
    TestResult result = {.name = "Filter cutoff sweep"};
    int frames = SHORT_FRAMES;
    float* buffer = (float*)calloc(frames * 2, sizeof(float));

    SynthEngine bright_synth;
    synth_init(&bright_synth, (float)SAMPLE_RATE);
    bright_synth.master_volume = 0.8f;
    set_all_waveforms(&bright_synth, WAVE_SAW);
    set_filter(&bright_synth, 8000.0f, 0.1f);
    synth_note_on(&bright_synth, 60, 1.0f);
    synth_process(&bright_synth, buffer, frames);
    BufferStats bright = compute_stats(buffer, frames);

    SynthEngine dark_synth;
    synth_init(&dark_synth, (float)SAMPLE_RATE);
    dark_synth.master_volume = 0.8f;
    set_all_waveforms(&dark_synth, WAVE_SAW);
    set_filter(&dark_synth, 200.0f, 0.1f);
    memset(buffer, 0, sizeof(float) * frames * 2);
    synth_note_on(&dark_synth, 60, 1.0f);
    synth_process(&dark_synth, buffer, frames);
    BufferStats dark = compute_stats(buffer, frames);

    free(buffer);

    bool pass = dark.rms < bright.rms * 0.6f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "bright_rms=%.3f dark_rms=%.3f", bright.rms, dark.rms);
    return result;
}

static TestResult test_adsr_behavior(void) {
    TestResult result = {.name = "ADSR attack/release"};
    SynthEngine synth;
    synth_init(&synth, (float)SAMPLE_RATE);
    synth.master_volume = 0.8f;
    set_all_waveforms(&synth, WAVE_SINE);
    set_amp_env(&synth, 0.5f, 0.1f, 0.8f, 2.0f);

    int attack_frames = SAMPLE_RATE; // 1 second buffer
    float* attack_buf = (float*)calloc(attack_frames * 2, sizeof(float));
    synth_note_on(&synth, 60, 1.0f);
    synth_process(&synth, attack_buf, attack_frames);
    BufferStats attack_stats = compute_stats(attack_buf, attack_frames);

    float attack_threshold = attack_stats.peak * 0.6f;
    float attack_time = -1.0f;
    for (int i = 0; i < attack_frames; i++) {
        float mag = fmaxf(fabsf(attack_buf[i * 2]), fabsf(attack_buf[i * 2 + 1]));
        if (mag >= attack_threshold) {
            attack_time = (float)i / SAMPLE_RATE;
            break;
        }
    }

    int release_frames = SAMPLE_RATE * 3;
    float* release_buf = (float*)calloc(release_frames * 2, sizeof(float));
    synth_note_off(&synth, 60);
    synth_process(&synth, release_buf, release_frames);

    float release_threshold = attack_stats.peak * 0.1f;
    float release_time = -1.0f;
    for (int i = release_frames - 1; i >= 0; i--) {
        float mag = fmaxf(fabsf(release_buf[i * 2]), fabsf(release_buf[i * 2 + 1]));
        if (mag >= release_threshold) {
            release_time = (float)i / SAMPLE_RATE;
            break;
        }
    }

    free(attack_buf);
    free(release_buf);

    bool attack_pass = attack_time >= 0.2f && attack_time <= 0.7f;
    bool release_pass = release_time >= 2.0f;
    result.passed = attack_pass && release_pass;
    snprintf(result.detail, sizeof(result.detail), "attack_time=%.2fs release_tail=%.2fs", attack_time, release_time);
    return result;
}

static TestResult test_polyphony(void) {
    TestResult result = {.name = "8-voice polyphony"};
    SynthEngine synth;
    synth_init(&synth, (float)SAMPLE_RATE);
    synth.master_volume = 0.8f;
    set_all_waveforms(&synth, WAVE_SAW);

    int notes[8] = {60, 64, 67, 71, 72, 76, 79, 83};
    for (int i = 0; i < 8; i++) {
        synth_note_on(&synth, notes[i], 0.8f);
    }

    float* buffer = (float*)calloc(SHORT_FRAMES * 2, sizeof(float));
    synth_process(&synth, buffer, SHORT_FRAMES);
    BufferStats stats = compute_stats(buffer, SHORT_FRAMES);
    free(buffer);

    int active = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (synth.voices[i].state != VOICE_OFF) {
            active++;
        }
    }

    bool pass = active >= 8 && stats.rms > 0.05f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "active=%d rms=%.3f", active, stats.rms);
    return result;
}

static TestResult test_master_volume(void) {
    TestResult result = {.name = "Master volume scaling"};
    BufferStats unity = render_note(WAVE_SINE, 0.2f, 1.0f);
    BufferStats half = render_note(WAVE_SINE, 0.2f, 0.5f);
    float ratio = (unity.rms > 0.0f) ? (half.rms / unity.rms) : 0.0f;
    bool pass = ratio > 0.4f && ratio < 0.6f;
    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail), "unity_rms=%.3f half_rms=%.3f ratio=%.2f", unity.rms, half.rms, ratio);
    return result;
}

static TestResult test_distortion_effect(void) {
    TestResult result = {.name = "Distortion saturation"};
    int frames = frames_from_seconds(0.4f);
    float* dry = (float*)calloc(frames * 2, sizeof(float));
    float* wet = (float*)calloc(frames * 2, sizeof(float));
    render_note_buffer(WAVE_SAW, 0.4f, 0.9f, dry);
    memcpy(wet, dry, sizeof(float) * frames * 2);

    TestDistortion fx = {
        .enabled = true,
        .drive = 6.0f,
        .mix = 0.8f
    };
    fx_distortion_process_buffer(&fx, wet, frames);

    BufferStats dry_stats = compute_stats(dry, frames);
    BufferStats wet_stats = compute_stats(wet, frames);
    float diff = average_abs_difference(dry, wet, frames);
    float crest_dry = dry_stats.peak / fmaxf(dry_stats.rms, 1e-4f);
    float crest_wet = wet_stats.peak / fmaxf(wet_stats.rms, 1e-4f);
    bool pass = diff > 0.01f && crest_wet < crest_dry * 0.9f;

    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail),
             "diff=%.3f crest_wet=%.2f crest_dry=%.2f",
             diff, crest_wet, crest_dry);

    free(dry);
    free(wet);
    return result;
}

static TestResult test_delay_effect(void) {
    TestResult result = {.name = "Delay echo tail"};
    float total_seconds = 1.5f;
    int frames = frames_from_seconds(total_seconds);
    int sustain_frames = frames_from_seconds(0.25f);
    float* dry = (float*)calloc(frames * 2, sizeof(float));
    render_note_with_release(dry,
                             frames,
                             sustain_frames,
                             WAVE_SAW,
                             0.01f,
                             0.05f,
                             0.0f,
                             0.05f);
    float* wet = (float*)calloc(frames * 2, sizeof(float));
    memcpy(wet, dry, sizeof(float) * frames * 2);

    TestDelay fx;
    fx_delay_init(&fx, 2.0f);
    fx.enabled = true;
    fx_delay_set(&fx, 0.35f, 0.45f, 0.6f);
    fx_delay_process_buffer(&fx, wet, frames);

    float dry_tail = segment_rms_seconds(dry, frames, 0.9f, 0.4f);
    float wet_tail = segment_rms_seconds(wet, frames, 0.9f, 0.4f);
    bool pass = wet_tail > fmaxf(0.01f, dry_tail * 2.5f);

    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail),
             "tail_wet=%.4f tail_dry=%.4f",
             wet_tail, dry_tail);

    fx_delay_free(&fx);
    free(dry);
    free(wet);
    return result;
}

static TestResult test_reverb_effect(void) {
    TestResult result = {.name = "Reverb ambience tail"};
    float total_seconds = 1.6f;
    int frames = frames_from_seconds(total_seconds);
    int sustain_frames = frames_from_seconds(0.3f);
    float* dry = (float*)calloc(frames * 2, sizeof(float));
    render_note_with_release(dry,
                             frames,
                             sustain_frames,
                             WAVE_TRIANGLE,
                             0.02f,
                             0.1f,
                             0.0f,
                             0.08f);
    float* wet = (float*)calloc(frames * 2, sizeof(float));
    memcpy(wet, dry, sizeof(float) * frames * 2);

    TestReverb fx;
    fx_reverb_init(&fx, 2.0f);
    fx.enabled = true;
        fx_reverb_set(&fx, 0.45f, 0.55f, 0.65f);
    fx_reverb_process_buffer(&fx, wet, frames);

    float dry_tail = segment_rms_seconds(dry, frames, 0.75f, 0.6f);
    float wet_tail = segment_rms_seconds(wet, frames, 0.75f, 0.6f);
    bool pass = wet_tail > fmaxf(0.006f, dry_tail * 2.0f);

    result.passed = pass;
    snprintf(result.detail, sizeof(result.detail),
             "tail_wet=%.4f tail_dry=%.4f",
             wet_tail, dry_tail);

    fx_reverb_free(&fx);
    free(dry);
    free(wet);
    return result;
}

int main(void) {
    TestResult results[] = {
        test_single_note(),
        test_waveform_energy(),
        test_unison(),
        test_filter_sweep(),
        test_adsr_behavior(),
        test_polyphony(),
        test_master_volume(),
        test_delay_effect(),
        test_reverb_effect(),
        test_distortion_effect()
    };

    const int total = (int)(sizeof(results) / sizeof(results[0]));
    int passed = 0;
    printf("AUDIO ENGINE CHECKLIST\n");
    printf("=======================\n");
    for (int i = 0; i < total; i++) {
        printf("[%s] %s - %s\n",
               results[i].passed ? "PASS" : "FAIL",
               results[i].name,
               results[i].detail);
        if (results[i].passed) {
            passed++;
        }
    }

    printf("\nSummary: %d/%d tests passed\n", passed, total);
    if (passed != total) {
        printf("Some items failed thresholds. Review details above.\n");
    } else {
        printf("All headless checklist tests passed.\n");
    }

    printf("\nNote: FX tests model the production rack algorithms offline so they can run headlessly in CI.\n");
    return (passed == total) ? 0 : 1;
}
