#include "preset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* preset_read_text_file(const char* path, long* out_size) {
    if (!path) {
        return NULL;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    buffer[read] = '\0';
    if (out_size) {
        *out_size = (long)read;
    }
    return buffer;
}

bool preset_write_text_file(const char* path, const char* data, size_t size) {
    if (!path || !data) {
        return false;
    }
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

void preset_init(PresetData* preset) {
    if (!preset) {
        return;
    }
    memset(preset, 0, sizeof(*preset));
    preset->version = PRESET_SCHEMA_VERSION;
    snprintf(preset->meta.name, sizeof(preset->meta.name), "Init Patch");
    snprintf(preset->meta.author, sizeof(preset->meta.author), "Anonymous");
    snprintf(preset->meta.category, sizeof(preset->meta.category), "Utility");
    snprintf(preset->meta.description, sizeof(preset->meta.description),
             "Default initialized preset");
    preset->tempo = 120.0f;
    preset->master_volume = 0.7f;
    preset->filter_cutoff = 8000.0f;
    preset->filter_resonance = 0.3f;
    preset->filter_mode = 0;
    preset->filter_env_amount = 0.0f;
    preset->env_attack = 0.01f;
    preset->env_decay = 0.1f;
    preset->env_sustain = 0.7f;
    preset->env_release = 0.3f;
    preset->distortion.drive = 5.0f;
    preset->distortion.mix = 0.5f;
    preset->chorus.rate = 0.5f;
    preset->chorus.depth = 10.0f;
    preset->chorus.mix = 0.5f;
    preset->delay.time = 0.3f;
    preset->delay.feedback = 0.4f;
    preset->delay.mix = 0.3f;
    preset->reverb.size = 0.5f;
    preset->reverb.damping = 0.5f;
    preset->reverb.mix = 0.3f;
    preset->compressor.threshold = 0.7f;
    preset->compressor.ratio = 4.0f;
    preset->arp.rate_multiplier = 1.0f;
    preset->arp.mode = 0;
}

static cJSON* preset_metadata_to_json(const PresetMetadata* meta) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }
    cJSON_AddStringToObject(obj, "name", meta->name);
    cJSON_AddStringToObject(obj, "author", meta->author);
    cJSON_AddStringToObject(obj, "category", meta->category);
    cJSON_AddStringToObject(obj, "description", meta->description);
    return obj;
}

static void preset_metadata_from_json(PresetMetadata* meta, const cJSON* json) {
    if (!meta || !json) {
        return;
    }
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON* author = cJSON_GetObjectItemCaseSensitive(json, "author");
    const cJSON* category = cJSON_GetObjectItemCaseSensitive(json, "category");
    const cJSON* description = cJSON_GetObjectItemCaseSensitive(json, "description");
    if (cJSON_IsString(name) && name->valuestring) {
        snprintf(meta->name, sizeof(meta->name), "%s", name->valuestring);
    }
    if (cJSON_IsString(author) && author->valuestring) {
        snprintf(meta->author, sizeof(meta->author), "%s", author->valuestring);
    }
    if (cJSON_IsString(category) && category->valuestring) {
        snprintf(meta->category, sizeof(meta->category), "%s", category->valuestring);
    }
    if (cJSON_IsString(description) && description->valuestring) {
        snprintf(meta->description, sizeof(meta->description), "%s", description->valuestring);
    }
}

cJSON* preset_to_json(const PresetData* preset) {
    if (!preset) {
        return NULL;
    }
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddNumberToObject(root, "version", preset->version);
    cJSON_AddItemToObject(root, "metadata", preset_metadata_to_json(&preset->meta));

    cJSON* values = cJSON_AddObjectToObject(root, "values");
    cJSON_AddNumberToObject(values, "tempo", preset->tempo);
    cJSON_AddNumberToObject(values, "masterVolume", preset->master_volume);
    cJSON_AddNumberToObject(values, "filterCutoff", preset->filter_cutoff);
    cJSON_AddNumberToObject(values, "filterResonance", preset->filter_resonance);
    cJSON_AddNumberToObject(values, "filterMode", preset->filter_mode);
    cJSON_AddNumberToObject(values, "filterEnvAmount", preset->filter_env_amount);
    cJSON_AddNumberToObject(values, "envAttack", preset->env_attack);
    cJSON_AddNumberToObject(values, "envDecay", preset->env_decay);
    cJSON_AddNumberToObject(values, "envSustain", preset->env_sustain);
    cJSON_AddNumberToObject(values, "envRelease", preset->env_release);

    cJSON* fx = cJSON_AddObjectToObject(values, "fx");

    cJSON* dist = cJSON_AddObjectToObject(fx, "distortion");
    cJSON_AddBoolToObject(dist, "enabled", preset->distortion.enabled);
    cJSON_AddNumberToObject(dist, "drive", preset->distortion.drive);
    cJSON_AddNumberToObject(dist, "mix", preset->distortion.mix);

    cJSON* chorus = cJSON_AddObjectToObject(fx, "chorus");
    cJSON_AddBoolToObject(chorus, "enabled", preset->chorus.enabled);
    cJSON_AddNumberToObject(chorus, "rate", preset->chorus.rate);
    cJSON_AddNumberToObject(chorus, "depth", preset->chorus.depth);
    cJSON_AddNumberToObject(chorus, "mix", preset->chorus.mix);

    cJSON* delay = cJSON_AddObjectToObject(fx, "delay");
    cJSON_AddBoolToObject(delay, "enabled", preset->delay.enabled);
    cJSON_AddNumberToObject(delay, "time", preset->delay.time);
    cJSON_AddNumberToObject(delay, "feedback", preset->delay.feedback);
    cJSON_AddNumberToObject(delay, "mix", preset->delay.mix);

    cJSON* reverb = cJSON_AddObjectToObject(fx, "reverb");
    cJSON_AddBoolToObject(reverb, "enabled", preset->reverb.enabled);
    cJSON_AddNumberToObject(reverb, "size", preset->reverb.size);
    cJSON_AddNumberToObject(reverb, "damping", preset->reverb.damping);
    cJSON_AddNumberToObject(reverb, "mix", preset->reverb.mix);

    cJSON* comp = cJSON_AddObjectToObject(fx, "compressor");
    cJSON_AddBoolToObject(comp, "enabled", preset->compressor.enabled);
    cJSON_AddNumberToObject(comp, "threshold", preset->compressor.threshold);
    cJSON_AddNumberToObject(comp, "ratio", preset->compressor.ratio);

    cJSON* arp = cJSON_AddObjectToObject(values, "arp");
    cJSON_AddBoolToObject(arp, "enabled", preset->arp.enabled);
    cJSON_AddNumberToObject(arp, "rateMultiplier", preset->arp.rate_multiplier);
    cJSON_AddNumberToObject(arp, "mode", preset->arp.mode);

    return root;
}

static float json_number_or_default(const cJSON* obj, const char* key, float fallback) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return (float)item->valuedouble;
    }
    return fallback;
}

static bool json_bool_or_default(const cJSON* obj, const char* key, bool fallback) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return fallback;
}

bool preset_from_json(PresetData* preset, const cJSON* json) {
    if (!preset || !json) {
        return false;
    }
    preset_init(preset);

    const cJSON* version = cJSON_GetObjectItemCaseSensitive(json, "version");
    if (cJSON_IsNumber(version)) {
        preset->version = version->valueint;
    }

    const cJSON* meta = cJSON_GetObjectItemCaseSensitive(json, "metadata");
    if (cJSON_IsObject(meta)) {
        preset_metadata_from_json(&preset->meta, meta);
    }

    const cJSON* values = cJSON_GetObjectItemCaseSensitive(json, "values");
    if (!cJSON_IsObject(values)) {
        return false;
    }

    preset->tempo = json_number_or_default(values, "tempo", preset->tempo);
    preset->master_volume = json_number_or_default(values, "masterVolume", preset->master_volume);
    preset->filter_cutoff = json_number_or_default(values, "filterCutoff", preset->filter_cutoff);
    preset->filter_resonance = json_number_or_default(values, "filterResonance", preset->filter_resonance);
    preset->filter_mode = (int)json_number_or_default(values, "filterMode", (float)preset->filter_mode);
    preset->filter_env_amount = json_number_or_default(values, "filterEnvAmount", preset->filter_env_amount);
    preset->env_attack = json_number_or_default(values, "envAttack", preset->env_attack);
    preset->env_decay = json_number_or_default(values, "envDecay", preset->env_decay);
    preset->env_sustain = json_number_or_default(values, "envSustain", preset->env_sustain);
    preset->env_release = json_number_or_default(values, "envRelease", preset->env_release);

    const cJSON* fx = cJSON_GetObjectItemCaseSensitive(values, "fx");
    if (cJSON_IsObject(fx)) {
        const cJSON* dist = cJSON_GetObjectItemCaseSensitive(fx, "distortion");
        if (cJSON_IsObject(dist)) {
            preset->distortion.enabled = json_bool_or_default(dist, "enabled", preset->distortion.enabled);
            preset->distortion.drive = json_number_or_default(dist, "drive", preset->distortion.drive);
            preset->distortion.mix = json_number_or_default(dist, "mix", preset->distortion.mix);
        }
        const cJSON* chorus = cJSON_GetObjectItemCaseSensitive(fx, "chorus");
        if (cJSON_IsObject(chorus)) {
            preset->chorus.enabled = json_bool_or_default(chorus, "enabled", preset->chorus.enabled);
            preset->chorus.rate = json_number_or_default(chorus, "rate", preset->chorus.rate);
            preset->chorus.depth = json_number_or_default(chorus, "depth", preset->chorus.depth);
            preset->chorus.mix = json_number_or_default(chorus, "mix", preset->chorus.mix);
        }
        const cJSON* delay = cJSON_GetObjectItemCaseSensitive(fx, "delay");
        if (cJSON_IsObject(delay)) {
            preset->delay.enabled = json_bool_or_default(delay, "enabled", preset->delay.enabled);
            preset->delay.time = json_number_or_default(delay, "time", preset->delay.time);
            preset->delay.feedback = json_number_or_default(delay, "feedback", preset->delay.feedback);
            preset->delay.mix = json_number_or_default(delay, "mix", preset->delay.mix);
        }
        const cJSON* reverb = cJSON_GetObjectItemCaseSensitive(fx, "reverb");
        if (cJSON_IsObject(reverb)) {
            preset->reverb.enabled = json_bool_or_default(reverb, "enabled", preset->reverb.enabled);
            preset->reverb.size = json_number_or_default(reverb, "size", preset->reverb.size);
            preset->reverb.damping = json_number_or_default(reverb, "damping", preset->reverb.damping);
            preset->reverb.mix = json_number_or_default(reverb, "mix", preset->reverb.mix);
        }
        const cJSON* comp = cJSON_GetObjectItemCaseSensitive(fx, "compressor");
        if (cJSON_IsObject(comp)) {
            preset->compressor.enabled = json_bool_or_default(comp, "enabled", preset->compressor.enabled);
            preset->compressor.threshold = json_number_or_default(comp, "threshold", preset->compressor.threshold);
            preset->compressor.ratio = json_number_or_default(comp, "ratio", preset->compressor.ratio);
        }
    }

    const cJSON* arp = cJSON_GetObjectItemCaseSensitive(values, "arp");
    if (cJSON_IsObject(arp)) {
        preset->arp.enabled = json_bool_or_default(arp, "enabled", preset->arp.enabled);
        preset->arp.rate_multiplier = json_number_or_default(arp, "rateMultiplier", preset->arp.rate_multiplier);
        preset->arp.mode = (int)json_number_or_default(arp, "mode", (float)preset->arp.mode);
    }

    return true;
}

bool preset_save_file(const PresetData* preset, const char* path, bool pretty) {
    if (!preset || !path) {
        return false;
    }
    cJSON* json = preset_to_json(preset);
    if (!json) {
        return false;
    }
    char* output = pretty ? cJSON_PrintBuffered(json, 1024, 1) : cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!output) {
        return false;
    }
    size_t len = strlen(output);
    bool ok = preset_write_text_file(path, output, len);
    cJSON_free(output);
    return ok;
}

bool preset_load_file(PresetData* preset, const char* path) {
    if (!preset || !path) {
        return false;
    }
    long length = 0;
    char* buffer = preset_read_text_file(path, &length);
    if (!buffer) {
        return false;
    }
    cJSON* json = cJSON_ParseWithLength(buffer, (size_t)length);
    free(buffer);
    if (!json) {
        return false;
    }
    bool ok = preset_from_json(preset, json);
    cJSON_Delete(json);
    return ok;
}
