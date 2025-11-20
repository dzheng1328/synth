#ifndef SYNTH_TYPES_H
#define SYNTH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "include/params.h"

// ============================================================================
// Parameter messaging primitives
// ============================================================================

typedef enum {
    PARAM_FLOAT = 0,
    PARAM_INT,
    PARAM_BOOL
} ParamType;

typedef struct {
    uint32_t id;      // Application-specific parameter identifier
    ParamType type;   // Data type stored in the union
    union {
        float f;
        int   i;
        int   b;
    } value;
} ParamMsg;

/* Example ParamMsg JSON:
   { "id": 1024, "type": "float", "value": 1200.0 }
*/

// ============================================================================
// MIDI + Sequencer queues
// ============================================================================

typedef enum {
    MIDI_EVENT_NOTE_ON = 0,
    MIDI_EVENT_NOTE_OFF,
    MIDI_EVENT_CONTROL_CHANGE,
    MIDI_EVENT_PITCH_BEND,
    MIDI_EVENT_AFTERTOUCH,
    MIDI_EVENT_PROGRAM_CHANGE
} MidiEventType;

typedef struct {
    MidiEventType type;
    uint8_t channel;
    uint8_t data1;   // note/cc/program or LSB for pitch bend
    uint8_t data2;   // velocity/value/MSB for pitch bend
} MidiEvent;

typedef struct {
    uint64_t sample_frame; // absolute frame at which to trigger
    uint8_t note;
    uint8_t velocity;
    uint8_t length_frames;
} SeqEvent;

#endif // SYNTH_TYPES_H
