#include "param_queue.h"
#include "synth_engine.h"
#include <stdio.h>
#include <string.h>

// Ring buffer storage
static PaUtilRingBuffer g_param_queue;
static ParamMsg g_param_buffer[PARAM_QUEUE_SIZE];

static PaUtilRingBuffer g_midi_queue;
static MidiEvent g_midi_buffer[MIDI_QUEUE_SIZE];

static PaUtilRingBuffer g_seq_queue;
static SeqEvent g_seq_buffer[SEQ_QUEUE_SIZE];

#ifdef PARAM_DEBUG
#define PARAM_LOG(fmt, ...) printf("[PARAM] " fmt "\n", ##__VA_ARGS__)
#else
#define PARAM_LOG(fmt, ...)
#endif

void param_queue_init(void) {
    PaUtil_InitializeRingBuffer(&g_param_queue, sizeof(ParamMsg),
                                PARAM_QUEUE_SIZE, g_param_buffer);

    PaUtil_InitializeRingBuffer(&g_midi_queue, sizeof(MidiEvent),
                                MIDI_QUEUE_SIZE, g_midi_buffer);

    PaUtil_InitializeRingBuffer(&g_seq_queue, sizeof(SeqEvent),
                                SEQ_QUEUE_SIZE, g_seq_buffer);

    printf("✅ Parameter/MIDI/Seq queues initialized (lock-free)\n");
}

bool param_queue_enqueue(const ParamMsg* msg) {
    if (!msg) {
        return false;
    }
    ring_buffer_size_t written = PaUtil_WriteRingBuffer(&g_param_queue, msg, 1);
    if (written == 0) {
        printf("⚠️ Parameter queue full! Dropping change.\n");
        return false;
    }
    PARAM_LOG("push param id=%u", msg->id);
    return true;
}

bool param_queue_dequeue(ParamMsg* out_change) {
    if (!out_change) {
        return false;
    }
    if (PaUtil_ReadRingBuffer(&g_param_queue, out_change, 1) == 1) {
        PARAM_LOG("pop param id=%u", out_change->id);
        return true;
    }
    return false;
}

void param_queue_drain(param_queue_handler handler, void* userdata) {
    if (!handler) return;
    ParamMsg change;
    while (param_queue_dequeue(&change)) {
        handler(&change, userdata);
    }
}

bool midi_queue_enqueue(const MidiEvent* event) {
    if (!event) {
        return false;
    }
    if (PaUtil_WriteRingBuffer(&g_midi_queue, event, 1) == 0) {
        printf("⚠️ MIDI queue full! Dropping event.\n");
        return false;
    }
    return true;
}

bool midi_queue_dequeue(MidiEvent* event) {
    if (!event) {
        return false;
    }
    return PaUtil_ReadRingBuffer(&g_midi_queue, event, 1) == 1;
}

bool seq_event_enqueue(const SeqEvent* event) {
    if (!event) {
        return false;
    }
    if (PaUtil_WriteRingBuffer(&g_seq_queue, event, 1) == 0) {
        printf("⚠️ Sequencer queue full! Dropping event.\n");
        return false;
    }
    return true;
}

bool seq_event_dequeue(SeqEvent* event) {
    if (!event) {
        return false;
    }
    return PaUtil_ReadRingBuffer(&g_seq_queue, event, 1) == 1;
}
