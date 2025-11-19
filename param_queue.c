#include "param_queue.h"
#include "synth_engine.h"
#include <stdio.h>
#include <string.h>

// Ring buffer storage
static PaUtilRingBuffer g_param_queue;
static ParamChange g_param_buffer[PARAM_QUEUE_SIZE];

#ifdef PARAM_DEBUG
#define PARAM_LOG(fmt, ...) printf("[PARAM] " fmt "\n", ##__VA_ARGS__)
#else
#define PARAM_LOG(fmt, ...)
#endif

void param_queue_init(void) {
    // Initialize parameter queue
    PaUtil_InitializeRingBuffer(&g_param_queue, sizeof(ParamChange), 
                                 PARAM_QUEUE_SIZE, g_param_buffer);
    
    printf("✅ Parameter queues initialized (lock-free)\n");
}

int param_queue_push(ParamType type, float value) {
    ParamChange change = {type, value};
    ring_buffer_size_t written = PaUtil_WriteRingBuffer(&g_param_queue, &change, 1);
    
    if (written == 0) {
        printf("⚠️ Parameter queue full! Dropping change.\n");
        return 0;
    }

    PARAM_LOG("push type=%d value=%f", type, value);
    return 1;
}

int param_queue_pop(ParamChange* out_change) {
    if (!out_change) {
        return 0;
    }
    if (PaUtil_ReadRingBuffer(&g_param_queue, out_change, 1) == 1) {
        PARAM_LOG("pop type=%d value=%f", out_change->type, out_change->value);
        return 1;
    }
    return 0;
}

void param_queue_drain(param_queue_handler handler, void* userdata) {
    if (!handler) return;
    ParamChange change;
    while (param_queue_pop(&change)) {
        handler(&change, userdata);
    }
}
