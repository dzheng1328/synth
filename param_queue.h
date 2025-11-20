#ifndef PARAM_QUEUE_H
#define PARAM_QUEUE_H

#include "pa_ringbuffer.h"
#include <stdint.h>
#include <stdbool.h>
#include "synth_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Global ring buffer size
#define PARAM_QUEUE_SIZE 256
#define MIDI_QUEUE_SIZE 256
#define SEQ_QUEUE_SIZE 512

// Initialize the parameter queue system
void param_queue_init(void);

// Push a parameter change from UI thread (returns true on success)
bool param_queue_enqueue(const ParamMsg* msg);

// Pop a single parameter change in the audio thread
bool param_queue_dequeue(ParamMsg* out_change);

// Drain all pending changes while invoking a callback (optional helper)
typedef void (*param_queue_handler)(const ParamMsg* change, void* userdata);
void param_queue_drain(param_queue_handler handler, void* userdata);

// MIDI event queue helpers
bool midi_queue_enqueue(const MidiEvent* event);
bool midi_queue_dequeue(MidiEvent* event);

// Sequencer event queue helpers
bool seq_event_enqueue(const SeqEvent* event);
bool seq_event_dequeue(SeqEvent* event);

#ifdef __cplusplus
}
#endif

#endif // PARAM_QUEUE_H
