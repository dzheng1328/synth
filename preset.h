#ifndef PRESET_H
#define PRESET_H

#include <stdbool.h>
#include <stddef.h>

#include "third_party/cjson/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRESET_SCHEMA_VERSION 1

typedef struct {
    char name[64];
    char author[64];
    char category[32];
    char description[128];
} PresetMetadata;

typedef struct {
    bool enabled;
    float drive;
    float mix;
} PresetDistortionSettings;

typedef struct {
    bool enabled;
    float rate;
    float depth;
    float mix;
} PresetChorusSettings;

typedef struct {
    bool enabled;
    float time;
    float feedback;
    float mix;
} PresetDelaySettings;

typedef struct {
    bool enabled;
    float size;
    float damping;
    float mix;
} PresetReverbSettings;

typedef struct {
    bool enabled;
    float threshold;
    float ratio;
} PresetCompressorSettings;

typedef struct {
    bool enabled;
    float rate_multiplier;
    int mode;
} PresetArpSettings;

typedef struct {
    PresetMetadata meta;
    int version;
    float tempo;
    float master_volume;
    float filter_cutoff;
    float filter_resonance;
    int filter_mode;
    float filter_env_amount;
    float env_attack;
    float env_decay;
    float env_sustain;
    float env_release;
    PresetDistortionSettings distortion;
    PresetChorusSettings chorus;
    PresetDelaySettings delay;
    PresetReverbSettings reverb;
    PresetCompressorSettings compressor;
    PresetArpSettings arp;
} PresetData;

void preset_init(PresetData* preset);
cJSON* preset_to_json(const PresetData* preset);
bool preset_from_json(PresetData* preset, const cJSON* json);
bool preset_save_file(const PresetData* preset, const char* path, bool pretty);
bool preset_load_file(PresetData* preset, const char* path);

// Shared helpers exposed for project serialization reuse
char* preset_read_text_file(const char* path, long* out_size);
bool preset_write_text_file(const char* path, const char* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // PRESET_H
