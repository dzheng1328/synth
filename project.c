#include "project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void project_init(ProjectData* project) {
    if (!project) {
        return;
    }
    memset(project, 0, sizeof(*project));
    snprintf(project->meta.name, sizeof(project->meta.name), "New Project");
    snprintf(project->meta.author, sizeof(project->meta.author), "Producer");
    snprintf(project->meta.notes, sizeof(project->meta.notes), "Session notes");
    snprintf(project->preset_path, sizeof(project->preset_path), "presets/Init.json");
    snprintf(project->sample_path, sizeof(project->sample_path), "samples/demo.wav");
    snprintf(project->export_path, sizeof(project->export_path), "exports/bounce.wav");
    project->export_duration_seconds = 8.0f;
    project->tempo = 120.0f;
    preset_init(&project->preset);
}

static cJSON* project_metadata_to_json(const ProjectMetadata* meta) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }
    cJSON_AddStringToObject(obj, "name", meta->name);
    cJSON_AddStringToObject(obj, "author", meta->author);
    cJSON_AddStringToObject(obj, "notes", meta->notes);
    return obj;
}

static void project_metadata_from_json(ProjectMetadata* meta, const cJSON* json) {
    if (!meta || !json) {
        return;
    }
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON* author = cJSON_GetObjectItemCaseSensitive(json, "author");
    const cJSON* notes = cJSON_GetObjectItemCaseSensitive(json, "notes");
    if (cJSON_IsString(name) && name->valuestring) {
        snprintf(meta->name, sizeof(meta->name), "%s", name->valuestring);
    }
    if (cJSON_IsString(author) && author->valuestring) {
        snprintf(meta->author, sizeof(meta->author), "%s", author->valuestring);
    }
    if (cJSON_IsString(notes) && notes->valuestring) {
        snprintf(meta->notes, sizeof(meta->notes), "%s", notes->valuestring);
    }
}

cJSON* project_to_json(const ProjectData* project) {
    if (!project) {
        return NULL;
    }
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddItemToObject(root, "metadata", project_metadata_to_json(&project->meta));
    cJSON_AddStringToObject(root, "presetPath", project->preset_path);
    cJSON_AddStringToObject(root, "samplePath", project->sample_path);
    cJSON_AddStringToObject(root, "exportPath", project->export_path);
    cJSON_AddNumberToObject(root, "exportDuration", project->export_duration_seconds);
    cJSON_AddNumberToObject(root, "tempo", project->tempo);

    cJSON* preset_obj = preset_to_json(&project->preset);
    if (!preset_obj) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "preset", preset_obj);
    return root;
}

bool project_from_json(ProjectData* project, const cJSON* json) {
    if (!project || !json) {
        return false;
    }
    project_init(project);

    const cJSON* meta = cJSON_GetObjectItemCaseSensitive(json, "metadata");
    if (cJSON_IsObject(meta)) {
        project_metadata_from_json(&project->meta, meta);
    }

    const cJSON* preset_path = cJSON_GetObjectItemCaseSensitive(json, "presetPath");
    if (cJSON_IsString(preset_path) && preset_path->valuestring) {
        snprintf(project->preset_path, sizeof(project->preset_path), "%s", preset_path->valuestring);
    }
    const cJSON* sample_path = cJSON_GetObjectItemCaseSensitive(json, "samplePath");
    if (cJSON_IsString(sample_path) && sample_path->valuestring) {
        snprintf(project->sample_path, sizeof(project->sample_path), "%s", sample_path->valuestring);
    }
    const cJSON* export_path = cJSON_GetObjectItemCaseSensitive(json, "exportPath");
    if (cJSON_IsString(export_path) && export_path->valuestring) {
        snprintf(project->export_path, sizeof(project->export_path), "%s", export_path->valuestring);
    }

    const cJSON* export_duration = cJSON_GetObjectItemCaseSensitive(json, "exportDuration");
    if (cJSON_IsNumber(export_duration)) {
        project->export_duration_seconds = (float)export_duration->valuedouble;
    }
    const cJSON* tempo = cJSON_GetObjectItemCaseSensitive(json, "tempo");
    if (cJSON_IsNumber(tempo)) {
        project->tempo = (float)tempo->valuedouble;
    }

    const cJSON* preset_obj = cJSON_GetObjectItemCaseSensitive(json, "preset");
    if (!cJSON_IsObject(preset_obj)) {
        return false;
    }
    if (!preset_from_json(&project->preset, preset_obj)) {
        return false;
    }
    return true;
}

bool project_save_file(const ProjectData* project, const char* path, bool pretty) {
    if (!project || !path) {
        return false;
    }
    cJSON* json = project_to_json(project);
    if (!json) {
        return false;
    }
    char* output = pretty ? cJSON_PrintBuffered(json, 2048, 1) : cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!output) {
        return false;
    }
    size_t len = strlen(output);
    bool ok = preset_write_text_file(path, output, len);
    cJSON_free(output);
    return ok;
}

bool project_load_file(ProjectData* project, const char* path) {
    if (!project || !path) {
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
    bool ok = project_from_json(project, json);
    cJSON_Delete(json);
    return ok;
}
