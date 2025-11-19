#ifndef PARAM_QUEUE_H
#define PARAM_QUEUE_H

#include "pa_ringbuffer.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Types of parameters that can be changed
typedef enum {
    PARAM_MASTER_VOLUME = 0,
    PARAM_TEMPO,
    PARAM_FX_DISTORTION_ENABLED,
    PARAM_FX_DISTORTION_DRIVE,
    PARAM_FX_DISTORTION_MIX,
    PARAM_FX_CHORUS_ENABLED,
    PARAM_FX_CHORUS_RATE,
    PARAM_FX_CHORUS_DEPTH,
    PARAM_FX_CHORUS_MIX,
    PARAM_FX_COMP_ENABLED,
    PARAM_FX_COMP_THRESHOLD,
    PARAM_FX_COMP_RATIO,
    PARAM_FX_DELAY_ENABLED,
    PARAM_FX_DELAY_TIME,
    PARAM_FX_DELAY_FEEDBACK,
    PARAM_FX_DELAY_MIX,
    PARAM_FX_REVERB_ENABLED,
    PARAM_FX_REVERB_SIZE,
    PARAM_FX_REVERB_DAMPING,
    PARAM_FX_REVERB_MIX,
    PARAM_ARP_ENABLED,
    PARAM_ARP_RATE,
    PARAM_ARP_MODE,
    PARAM_FILTER_CUTOFF,
    PARAM_FILTER_RESONANCE,
    PARAM_FILTER_MODE,
    PARAM_FILTER_ENV_AMOUNT,
    PARAM_ENV_ATTACK,
    PARAM_ENV_DECAY,
    PARAM_ENV_SUSTAIN,
    PARAM_ENV_RELEASE,
} ParamType;

// Structure for a parameter change
typedef struct {
    ParamType type;
    float value;
} ParamChange;

// Global ring buffer size
#define PARAM_QUEUE_SIZE 256

// Initialize the parameter queue system
void param_queue_init(void);

// Push a parameter change from UI thread (returns 1 on success, 0 if full)
int param_queue_push(ParamType type, float value);

// Pop a single parameter change in the audio thread (returns 1 when available)
int param_queue_pop(ParamChange* out_change);

// Drain all pending changes while invoking a callback (optional helper)
typedef void (*param_queue_handler)(const ParamChange* change, void* userdata);
void param_queue_drain(param_queue_handler handler, void* userdata);

#ifdef __cplusplus
}
#endif

#endif // PARAM_QUEUE_H
