/**
 * Professional Synthesizer Test Program
 * 
 * Demonstrates:
 * - Multiple oscillator waveforms
 * - Filters with resonance
 * - ADSR envelopes
 * - LFO modulation
 * - Polyphony
 * - Real-time audio output
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "synth_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Global synth instance
SynthEngine g_synth;

// Audio callback for miniaudio
void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    float* out = (float*)output;
    (void)input;
    
    synth_process(&g_synth, out, frameCount);
}

// Helper to print a nice header
void print_header(const char* text) {
    printf("\n");
    printf("========================================\n");
    printf("  %s\n", text);
    printf("========================================\n");
    printf("\n");
}

// Demo 1: Different oscillator waveforms
void demo_waveforms() {
    print_header("DEMO 1: Oscillator Waveforms");
    
    const char* wave_names[] = {"SINE", "SAW", "SQUARE", "TRIANGLE", "NOISE"};
    WaveformType waveforms[] = {WAVE_SINE, WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_NOISE};
    
    for (int i = 0; i < 5; i++) {
        printf("Playing %s wave (A4 = 440 Hz)...\n", wave_names[i]);
        
        // Set oscillator waveforms for all voices
        for (int v = 0; v < MAX_VOICES; v++) {
            g_synth.voices[v].osc1.waveform = waveforms[i];
            g_synth.voices[v].osc1.amplitude = 0.3f;
        }
        
        synth_note_on(&g_synth, 69, 0.8f); // A4
        usleep(800000); // 800ms
        synth_note_off(&g_synth, 69);
        usleep(200000); // 200ms gap
    }
}

// Demo 2: Filter sweep
void demo_filter() {
    print_header("DEMO 2: Filter Sweep");
    
    printf("Low-pass filter sweep (200 Hz -> 5000 Hz)...\n");
    
    // Setup sawtooth wave for clear filter effect
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SAW;
        g_synth.voices[v].osc1.amplitude = 0.3f;
        g_synth.voices[v].filter.mode = FILTER_LP;
        g_synth.voices[v].filter.resonance = 0.7f; // High resonance for effect
    }
    
    synth_note_on(&g_synth, 60, 0.8f); // C4
    
    // Sweep cutoff from 200 Hz to 5000 Hz over 3 seconds
    for (int i = 0; i < 30; i++) {
        float t = i / 29.0f;
        float cutoff = 200.0f + (5000.0f - 200.0f) * t;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            g_synth.voices[v].filter.cutoff = cutoff;
        }
        
        usleep(100000); // 100ms
    }
    
    synth_note_off(&g_synth, 60);
    usleep(300000);
}

// Demo 3: ADSR Envelope
void demo_envelope() {
    print_header("DEMO 3: ADSR Envelopes");
    
    // Reset to sine wave
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SINE;
        g_synth.voices[v].osc1.amplitude = 0.4f;
    }
    
    printf("Short pluck (fast attack, short release)...\n");
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].env_amp.attack = 0.001f;
        g_synth.voices[v].env_amp.decay = 0.1f;
        g_synth.voices[v].env_amp.sustain = 0.3f;
        g_synth.voices[v].env_amp.release = 0.1f;
    }
    synth_note_on(&g_synth, 64, 0.8f); // E4
    usleep(100000);
    synth_note_off(&g_synth, 64);
    usleep(500000);
    
    printf("Pad sound (slow attack, long release)...\n");
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].env_amp.attack = 0.5f;
        g_synth.voices[v].env_amp.decay = 0.3f;
        g_synth.voices[v].env_amp.sustain = 0.7f;
        g_synth.voices[v].env_amp.release = 1.0f;
    }
    synth_note_on(&g_synth, 67, 0.8f); // G4
    usleep(1500000);
    synth_note_off(&g_synth, 67);
    usleep(1500000);
}

// Demo 4: Polyphony
void demo_polyphony() {
    print_header("DEMO 4: Polyphony");
    
    // Setup nice sound
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SAW;
        g_synth.voices[v].osc1.amplitude = 0.25f;
        g_synth.voices[v].filter.mode = FILTER_LP;
        g_synth.voices[v].filter.cutoff = 2000.0f;
        g_synth.voices[v].filter.resonance = 0.3f;
        g_synth.voices[v].env_amp.attack = 0.01f;
        g_synth.voices[v].env_amp.decay = 0.2f;
        g_synth.voices[v].env_amp.sustain = 0.6f;
        g_synth.voices[v].env_amp.release = 0.3f;
    }
    
    printf("Playing C major chord (C-E-G)...\n");
    synth_note_on(&g_synth, 60, 0.8f); // C4
    usleep(100000);
    synth_note_on(&g_synth, 64, 0.8f); // E4
    usleep(100000);
    synth_note_on(&g_synth, 67, 0.8f); // G4
    usleep(1500000);
    
    synth_note_off(&g_synth, 60);
    synth_note_off(&g_synth, 64);
    synth_note_off(&g_synth, 67);
    usleep(500000);
    
    printf("Playing arpeggio...\n");
    int notes[] = {60, 64, 67, 72, 67, 64}; // C E G C G E
    for (int i = 0; i < 6; i++) {
        synth_note_on(&g_synth, notes[i], 0.8f);
        usleep(200000);
        synth_note_off(&g_synth, notes[i]);
        usleep(50000);
    }
    usleep(500000);
}

// Demo 5: LFO Modulation
void demo_lfo() {
    print_header("DEMO 5: LFO Modulation");
    
    // Setup LFO to modulate oscillator pitch (vibrato)
    g_synth.lfos[0].waveform = WAVE_SINE;
    g_synth.lfos[0].rate = 5.0f; // 5 Hz vibrato
    g_synth.lfos[0].amount = 0.02f; // Subtle
    g_synth.lfos[0].bipolar = true;
    
    // Note: Full LFO routing would require modulation matrix setup
    // For now, we'll demonstrate the LFO is working
    
    printf("Playing note with vibrato (LFO initialized)...\n");
    printf("(Full LFO routing requires modulation matrix setup)\n");
    
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SINE;
        g_synth.voices[v].osc1.amplitude = 0.4f;
    }
    
    synth_note_on(&g_synth, 69, 0.8f); // A4
    usleep(2000000); // 2 seconds
    synth_note_off(&g_synth, 69);
    usleep(500000);
}

// Demo 6: Unison & Detune
void demo_unison() {
    print_header("DEMO 6: Unison & Detune");
    
    printf("Single oscillator...\n");
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SAW;
        g_synth.voices[v].osc1.amplitude = 0.3f;
        g_synth.voices[v].osc1.unison_voices = 1;
    }
    synth_note_on(&g_synth, 60, 0.8f);
    usleep(1000000);
    synth_note_off(&g_synth, 60);
    usleep(300000);
    
    printf("5-voice unison with detune (super saw!)...\n");
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.unison_voices = 5;
        g_synth.voices[v].osc1.detune_cents = 15.0f; // ±15 cents
        g_synth.voices[v].osc1.unison_spread = 1.0f;
    }
    synth_note_on(&g_synth, 60, 0.8f);
    usleep(1500000);
    synth_note_off(&g_synth, 60);
    usleep(500000);
}

// Demo 7: PWM (Pulse Width Modulation)
void demo_pwm() {
    print_header("DEMO 7: Pulse Width Modulation");
    
    printf("Sweeping pulse width from 10%% to 90%%...\n");
    
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SQUARE;
        g_synth.voices[v].osc1.amplitude = 0.3f;
    }
    
    synth_note_on(&g_synth, 60, 0.8f);
    
    // Sweep pulse width
    for (int i = 0; i < 30; i++) {
        float pw = 0.1f + (0.8f * i / 29.0f);
        for (int v = 0; v < MAX_VOICES; v++) {
            g_synth.voices[v].osc1.pulse_width = pw;
        }
        usleep(100000);
    }
    
    synth_note_off(&g_synth, 60);
    usleep(300000);
}

int main() {
    printf("\n");
    printf("==================================================\n");
    printf("  🎹 PROFESSIONAL SYNTHESIZER ENGINE TEST 🎹\n");
    printf("==================================================\n");
    printf("\n");
    printf("This demo showcases:\n");
    printf("  • Multiple oscillator waveforms\n");
    printf("  • State-variable filters\n");
    printf("  • ADSR envelopes\n");
    printf("  • 8-voice polyphony\n");
    printf("  • Unison & detune\n");
    printf("  • PWM (pulse width modulation)\n");
    printf("  • LFO system\n");
    printf("\n");
    
    // Initialize synth engine
    synth_init(&g_synth, 44100.0f);
    
    // Setup default envelope for all voices
    for (int i = 0; i < MAX_VOICES; i++) {
        g_synth.voices[i].env_amp.attack = 0.01f;
        g_synth.voices[i].env_amp.decay = 0.1f;
        g_synth.voices[i].env_amp.sustain = 0.7f;
        g_synth.voices[i].env_amp.release = 0.3f;
    }
    
    // Initialize audio device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;  // Stereo
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    config.pUserData = &g_synth;
    
    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("❌ Failed to initialize audio device\n");
        return 1;
    }
    
    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("❌ Failed to start audio device\n");
        ma_device_uninit(&device);
        return 1;
    }
    
    printf("✅ Audio engine running!\n");
    printf("Sample Rate: %d Hz\n", device.sampleRate);
    printf("Buffer Size: %d frames\n", device.playback.internalPeriodSizeInFrames);
    printf("\n");
    printf("Press Enter to start demos...\n");
    getchar();
    
    // Run demos
    demo_waveforms();
    demo_filter();
    demo_envelope();
    demo_polyphony();
    demo_lfo();
    demo_unison();
    demo_pwm();
    
    // Final chord
    print_header("FINALE");
    printf("Final chord with everything combined...\n");
    
    for (int v = 0; v < MAX_VOICES; v++) {
        g_synth.voices[v].osc1.waveform = WAVE_SAW;
        g_synth.voices[v].osc1.amplitude = 0.2f;
        g_synth.voices[v].osc1.unison_voices = 3;
        g_synth.voices[v].osc1.detune_cents = 10.0f;
        g_synth.voices[v].filter.mode = FILTER_LP;
        g_synth.voices[v].filter.cutoff = 2000.0f;
        g_synth.voices[v].filter.resonance = 0.5f;
        g_synth.voices[v].env_amp.attack = 0.2f;
        g_synth.voices[v].env_amp.release = 1.5f;
    }
    
    synth_note_on(&g_synth, 48, 0.8f); // C3
    usleep(100000);
    synth_note_on(&g_synth, 52, 0.8f); // E3
    usleep(100000);
    synth_note_on(&g_synth, 55, 0.8f); // G3
    usleep(100000);
    synth_note_on(&g_synth, 60, 0.8f); // C4
    usleep(2000000);
    
    synth_all_notes_off(&g_synth);
    usleep(2000000);
    
    printf("\n");
    printf("==================================================\n");
    printf("  ✅ ALL DEMOS COMPLETE!\n");
    printf("==================================================\n");
    printf("\n");
    printf("You now have:\n");
    printf("  ✓ Professional-grade oscillators\n");
    printf("  ✓ State-variable filters\n");
    printf("  ✓ ADSR envelopes\n");
    printf("  ✓ 8-voice polyphony\n");
    printf("  ✓ Voice allocation & stealing\n");
    printf("  ✓ Unison & detune\n");
    printf("  ✓ PWM\n");
    printf("  ✓ LFO system\n");
    printf("  ✓ Soft limiting\n");
    printf("\n");
    printf("Next steps:\n");
    printf("  • Add full modulation matrix routing\n");
    printf("  • Implement effects (delay, reverb)\n");
    printf("  • Add wavetable synthesis\n");
    printf("  • Create preset system\n");
    printf("  • Build MIDI input handler\n");
    printf("\n");
    printf("Press Enter to exit...\n");
    getchar();
    
    // Cleanup
    ma_device_uninit(&device);
    
    printf("\n🎉 Thanks for testing the synth engine!\n\n");
    
    return 0;
}
