/**
 * Minimal C Synthesizer - WITH REAL AUDIO!
 * 
 * This version actually plays sound through your speakers!
 * 
 * Compile: gcc main.c -o synth -lm -lpthread
 * Run: ./synth
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

// ============================================================================
// SIMPLE OSCILLATOR
// ============================================================================

typedef struct {
    float phase;          // Current phase (0.0 to 1.0)
    float frequency;      // Frequency in Hz
    float sample_rate;    // Sample rate
    float amplitude;      // Volume (0.0 to 1.0)
    bool playing;         // Is sound playing?
} SimpleOscillator;

void osc_init(SimpleOscillator* osc, float sample_rate) {
    osc->phase = 0.0f;
    osc->frequency = 440.0f;  // A4 note
    osc->sample_rate = sample_rate;
    osc->amplitude = 0.3f;    // 30% volume
    osc->playing = true;
}

float osc_process(SimpleOscillator* osc) {
    if (!osc->playing) {
        return 0.0f;
    }
    
    // Generate sine wave
    float output = sinf(osc->phase * 2.0f * M_PI) * osc->amplitude;
    
    // Advance phase
    osc->phase += osc->frequency / osc->sample_rate;
    
    // Wrap phase
    if (osc->phase >= 1.0f) {
        osc->phase -= 1.0f;
    }
    
    return output;
}

void osc_set_frequency(SimpleOscillator* osc, float freq) {
    osc->frequency = freq;
}

float midi_to_freq(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

// ============================================================================
// AUDIO CALLBACK - THE HEART OF REAL-TIME AUDIO
// ============================================================================

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    SimpleOscillator* osc = (SimpleOscillator*)device->pUserData;
    float* out = (float*)output;
    
    (void)input;  // Unused
    
    // CRITICAL: This function is called from the audio thread!
    // - No malloc/free
    // - No printf (commented out for demo, but avoid in production)
    // - No file I/O
    // - Just fast math
    
    for (ma_uint32 i = 0; i < frameCount; i++) {
        out[i] = osc_process(osc);
    }
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================

int main(int argc, char* argv[]) {
    printf("\n");
    printf("==================================================\n");
    printf("  🎵 C Synthesizer - REAL AUDIO VERSION! 🎵\n");
    printf("==================================================\n");
    printf("\n");
    
    // Create oscillator
    SimpleOscillator osc;
    osc_init(&osc, 44100.0f);
    
    printf("Initializing audio device...\n");
    
    // Configure audio device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;   // 32-bit float
    config.playback.channels = 1;              // Mono
    config.sampleRate = 44100;                 // CD quality
    config.dataCallback = audio_callback;      // Our callback function
    config.pUserData = &osc;                   // Pass oscillator to callback
    
    // Initialize device
    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("❌ Failed to initialize audio device!\n");
        return 1;
    }
    
    printf("✅ Audio device initialized!\n");
    printf("\n");
    printf("Sample Rate: %d Hz\n", device.sampleRate);
    printf("Buffer Size: %d samples\n", device.playback.internalPeriodSizeInFrames);
    printf("Channels: %d (mono)\n", device.playback.channels);
    printf("Latency: ~%.2f ms\n", 
           (float)device.playback.internalPeriodSizeInFrames / device.sampleRate * 1000.0f);
    printf("\n");
    
    // Start playback
    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("❌ Failed to start audio device!\n");
        ma_device_uninit(&device);
        return 1;
    }
    
    printf("==================================================\n");
    printf("  🎹 NOW PLAYING!\n");
    printf("==================================================\n");
    printf("\n");
    printf("Playing A4 (440 Hz) sine wave...\n");
    printf("\n");
    printf("Press Enter to try different notes...\n");
    getchar();
    
    // Demo: Play different notes
    printf("\n");
    printf("🎵 Playing C major scale (C4-C5)...\n");
    printf("\n");
    
    int scale[] = {60, 62, 64, 65, 67, 69, 71, 72};  // C major
    const char* notes[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
    
    for (int i = 0; i < 8; i++) {
        float freq = midi_to_freq(scale[i]);
        printf("  Playing %s (MIDI %d) = %.2f Hz\n", notes[i], scale[i], freq);
        
        osc_set_frequency(&osc, freq);
        
        // Play for 500ms
        ma_sleep(500);
    }
    
    printf("\n");
    printf("🎵 Playing a chord (C-E-G)...\n");
    printf("(Note: This oscillator only plays one note at a time)\n");
    printf("(Polyphony coming in Phase 3!)\n");
    printf("\n");
    
    // Quickly alternate between chord notes (fake polyphony)
    for (int repeat = 0; repeat < 20; repeat++) {
        osc_set_frequency(&osc, midi_to_freq(60));  // C
        ma_sleep(30);
        osc_set_frequency(&osc, midi_to_freq(64));  // E
        ma_sleep(30);
        osc_set_frequency(&osc, midi_to_freq(67));  // G
        ma_sleep(30);
    }
    
    printf("\n");
    printf("==================================================\n");
    printf("  ✅ DEMO COMPLETE!\n");
    printf("==================================================\n");
    printf("\n");
    printf("What you just heard:\n");
    printf("  ✓ Real-time audio synthesis in C\n");
    printf("  ✓ MIDI note to frequency conversion\n");
    printf("  ✓ Pure sine wave oscillator\n");
    printf("  ✓ Low-latency audio callback\n");
    printf("\n");
    printf("Compare this to your web synth:\n");
    printf("  - Same concepts (oscillator, frequency, sample rate)\n");
    printf("  - Same math (phase accumulation, MIDI conversion)\n");
    printf("  - Lower latency (~5ms vs ~10-20ms)\n");
    printf("  - More control (you wrote the oscillator!)\n");
    printf("\n");
    printf("Next steps:\n");
    printf("  1. Try different waveforms (saw, square, triangle)\n");
    printf("  2. Add ADSR envelope\n");
    printf("  3. Implement 8-voice polyphony\n");
    printf("  4. Add filters and effects\n");
    printf("\n");
    printf("Press Enter to stop...\n");
    getchar();
    
    // Cleanup
    printf("\n");
    printf("Stopping audio...\n");
    ma_device_uninit(&device);
    printf("✅ Audio device closed.\n");
    printf("\n");
    printf("🎉 You just wrote a real-time audio synthesizer in C!\n");
    printf("\n");
    
    return 0;
}
