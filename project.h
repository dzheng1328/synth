#ifndef PROJECT_H
#define PROJECT_H

#include <stdbool.h>

#include "preset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[64];
    char author[64];
    char notes[128];
} ProjectMetadata;

typedef struct {
    ProjectMetadata meta;
    char preset_path[512];
    char sample_path[512];
    char export_path[512];
    float export_duration_seconds;
    float tempo;
    PresetData preset;
} ProjectData;

void project_init(ProjectData* project);
cJSON* project_to_json(const ProjectData* project);
bool project_from_json(ProjectData* project, const cJSON* json);
bool project_save_file(const ProjectData* project, const char* path, bool pretty);
bool project_load_file(ProjectData* project, const char* path);

#ifdef __cplusplus
}
#endif

#endif // PROJECT_H
