/**
 * Minimal C Synthesizer - Phase 1
 * 
 * Goal: Play a 440 Hz sine wave (A4 note)
 * 
 * This is the simplest possible audio program:
 * 1. Initialize audio device
 * 2. Generate sine wave samples in callback
 * 3. Play through speakers
 * 
 * Compile: gcc main.c -o synth -lm -lpthread
 * Run: ./synth
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

// We'll download miniaudio.h separately, but for now we'll describe the structure
// For now, let's create a minimal example that will work

// ============================================================================
// PHASE 1: MINIMAL SINE WAVE GENERATOR
// ============================================================================

// Global audio state (will be improved in Phase 2)
typedef struct {
    float phase;          // Current phase of oscillator (0.0 to 1.0)
    float frequency;      // Frequency in Hz
    float sample_rate;    // Sample rate (e.g., 44100)
    float amplitude;      // Volume (0.0 to 1.0)
    bool playing;         // Is sound playing?
} SimpleOscillator;

// Initialize oscillator
void osc_init(SimpleOscillator* osc, float sample_rate) {
    osc->phase = 0.0f;
    osc->frequency = 440.0f;  // A4 note
    osc->sample_rate = sample_rate;
    osc->amplitude = 0.3f;    // 30% volume (safe starting point)
    osc->playing = true;
}

// Generate one sample
float osc_process(SimpleOscillator* osc) {
    if (!osc->playing) {
        return 0.0f;
    }
    
    // Generate sine wave: sin(2π * phase)
    float output = sinf(osc->phase * 2.0f * M_PI) * osc->amplitude;
    
    // Advance phase
    float phase_increment = osc->frequency / osc->sample_rate;
    osc->phase += phase_increment;
    
    // Wrap phase to [0, 1)
    if (osc->phase >= 1.0f) {
        osc->phase -= 1.0f;
    }
    
    return output;
}

// Set frequency (for testing different notes)
void osc_set_frequency(SimpleOscillator* osc, float freq) {
    osc->frequency = freq;
}

// Convert MIDI note to frequency
float midi_to_freq(int midi_note) {
    // A4 (440 Hz) = MIDI note 69
    // Frequency = 440 * 2^((note - 69) / 12)
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

// ============================================================================
// PHASE 1: PLACEHOLDER AUDIO (miniaudio will replace this)
// ============================================================================

/**
 * This is a placeholder. In the next step, you'll:
 * 1. Download miniaudio.h from: https://github.com/mackron/miniaudio
 * 2. Place it in this directory
 * 3. Include it: #include "miniaudio.h"
 * 4. Use the real audio callback below
 * 
 * For now, this demonstrates the CONCEPTS:
 */

void audio_callback_example(void* user_data, float* output, int num_frames) {
    SimpleOscillator* osc = (SimpleOscillator*)user_data;
    
    // CRITICAL: This runs on the audio thread!
    // - No malloc/free
    // - No printf (in real code)
    // - No file I/O
    // - Just fast math
    
    for (int i = 0; i < num_frames; i++) {
        // Generate one sample
        float sample = osc_process(osc);
        
        // Mono output (could be stereo: left and right channels)
        output[i] = sample;
    }
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================

int main(int argc, char* argv[]) {
    printf("==============================================\n");
    printf("  Minimal C Synthesizer - Phase 1\n");
    printf("==============================================\n");
    printf("\n");
    printf("Goal: Play a 440 Hz sine wave (A4 note)\n");
    printf("\n");
    
    // Audio configuration
    const float SAMPLE_RATE = 44100.0f;
    const int BUFFER_SIZE = 256;  // Samples per callback
    
    // Calculate latency
    float latency_ms = (BUFFER_SIZE / SAMPLE_RATE) * 1000.0f;
    printf("Sample Rate: %.0f Hz\n", SAMPLE_RATE);
    printf("Buffer Size: %d samples\n", BUFFER_SIZE);
    printf("Latency: %.2f ms\n", latency_ms);
    printf("\n");
    
    // Create oscillator
    SimpleOscillator osc;
    osc_init(&osc, SAMPLE_RATE);
    
    printf("Playing A4 (440 Hz) sine wave...\n");
    printf("\n");
    
    // ========================================================================
    // NEXT STEPS (You'll implement these):
    // ========================================================================
    
    printf("TODO - NEXT STEPS:\n");
    printf("\n");
    printf("1. Download miniaudio.h:\n");
    printf("   wget https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h\n");
    printf("\n");
    printf("2. Add this code after including miniaudio.h:\n");
    printf("\n");
    printf("   ma_device_config config = ma_device_config_init(ma_device_type_playback);\n");
    printf("   config.playback.format = ma_format_f32;\n");
    printf("   config.playback.channels = 1;\n");
    printf("   config.sampleRate = 44100;\n");
    printf("   config.dataCallback = audio_callback;\n");
    printf("   config.pUserData = &osc;\n");
    printf("\n");
    printf("   ma_device device;\n");
    printf("   ma_device_init(NULL, &config, &device);\n");
    printf("   ma_device_start(&device);\n");
    printf("\n");
    printf("   // Play for 5 seconds\n");
    printf("   sleep(5);\n");
    printf("\n");
    printf("   ma_device_uninit(&device);\n");
    printf("\n");
    printf("3. Compile:\n");
    printf("   gcc main.c -o synth -lm -lpthread\n");
    printf("\n");
    printf("4. Run:\n");
    printf("   ./synth\n");
    printf("\n");
    
    // Test the oscillator (no audio output yet)
    printf("Testing oscillator (generating samples)...\n");
    printf("\n");
    printf("First 10 samples:\n");
    for (int i = 0; i < 10; i++) {
        float sample = osc_process(&osc);
        printf("  Sample %d: %.6f\n", i, sample);
    }
    printf("\n");
    
    // Test different frequencies
    printf("Testing different notes:\n");
    printf("\n");
    
    int test_notes[] = {60, 64, 67, 72};  // C4, E4, G4, C5
    const char* note_names[] = {"C4", "E4", "G4", "C5"};
    
    for (int i = 0; i < 4; i++) {
        float freq = midi_to_freq(test_notes[i]);
        printf("  MIDI %d (%s) = %.2f Hz\n", test_notes[i], note_names[i], freq);
    }
    printf("\n");
    
    printf("==============================================\n");
    printf("  Phase 1 Complete!\n");
    printf("==============================================\n");
    printf("\n");
    printf("What you learned:\n");
    printf("  ✓ Phase accumulation for sine generation\n");
    printf("  ✓ Sample rate and buffer size concepts\n");
    printf("  ✓ MIDI note to frequency conversion\n");
    printf("  ✓ Audio callback structure\n");
    printf("\n");
    printf("Next: Download miniaudio.h and add real audio output!\n");
    printf("\n");
    
    return 0;
}
