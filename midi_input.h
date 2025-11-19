#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef void (*midi_event_handler)(const MidiEvent* event, void* userdata);

// Initialize the MIDI subsystem and begin listening for input.
// Safe to call multiple times; repeated calls are ignored.
void midi_input_start(void);

// Shut down the MIDI subsystem and release OS resources.
void midi_input_stop(void);

// Drain all pending MIDI events while invoking the handler.
void midi_queue_drain(midi_event_handler handler, void* userdata);

// Push a MIDI event into the queue from any producer thread (UI, MIDI driver, etc.).
void midi_queue_push_event(const MidiEvent* event);

// Convenience helpers for generating events locally (e.g., QWERTY keyboard)
void midi_queue_send_note_on(uint8_t note, uint8_t velocity);
void midi_queue_send_note_off(uint8_t note);

#ifdef __cplusplus
}
#endif

#endif // MIDI_INPUT_H
