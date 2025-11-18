// Minimal test to check if audio is working
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "synth_engine.h"
#include <stdio.h>
#include <unistd.h>

SynthEngine synth;

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    float* out = (float*)output;
    (void)input; (void)device;
    
    float buffer[2];
    for (ma_uint32 i = 0; i < frameCount; i++) {
        synth_process(&synth, buffer, 1);
        out[i * 2 + 0] = buffer[0];
        out[i * 2 + 1] = buffer[1];
    }
}

int main() {
    printf("🎵 Minimal Synth Audio Test\n");
    printf("═══════════════════════════\n\n");
    
    // Init synth
    synth_init(&synth, 44100.0f);
    synth.master_volume = 0.5f;
    
    // Init audio
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    
    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("❌ Failed to initialize audio\n");
        return 1;
    }
    
    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("❌ Failed to start audio\n");
        return 1;
    }
    
    printf("✅ Audio started\n\n");
    
    // Play C major scale
    int notes[] = {60, 62, 64, 65, 67, 69, 71, 72};  // C D E F G A B C
    const char* labels[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
    
    for (int i = 0; i < 8; i++) {
        printf("Playing %s (MIDI %d)...\n", labels[i], notes[i]);
        synth_note_on(&synth, notes[i], 0.8f);
        sleep(1);
        synth_note_off(&synth, notes[i]);
    }
    
    printf("\n✅ Test complete!\n");
    
    ma_device_uninit(&device);
    return 0;
}
