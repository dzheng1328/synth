#include "midi_input.h"
#include "param_queue.h"
#include "midi_shim.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool g_midi_running = false;

void midi_queue_push_event(const MidiEvent* event) {
    if (!event) {
        return;
    }
    if (!midi_queue_enqueue(event)) {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr, "⚠️ MIDI queue overflow — dropping events.\n");
            warned = 1;
        }
    }
}

void midi_queue_drain(midi_event_handler handler, void* userdata) {
    if (!handler) {
        return;
    }
    MidiEvent event;
    while (midi_queue_dequeue(&event)) {
        handler(&event, userdata);
    }
}

void midi_queue_send_note_on(uint8_t note, uint8_t velocity) {
    MidiEvent event = {
        .type = MIDI_EVENT_NOTE_ON,
        .channel = 0,
        .data1 = note,
        .data2 = velocity
    };
    midi_queue_push_event(&event);
}

void midi_queue_send_note_off(uint8_t note) {
    MidiEvent event = {
        .type = MIDI_EVENT_NOTE_OFF,
        .channel = 0,
        .data1 = note,
        .data2 = 0
    };
    midi_queue_push_event(&event);
}

void midi_input_start(void) {
    if (g_midi_running) {
        return;
    }
    if (!midi_shim_start()) {
        fprintf(stderr, "⚠️ MIDI shim failed to start; hardware input disabled.\n");
        return;
    }
    g_midi_running = true;
}

void midi_input_stop(void) {
    if (!g_midi_running) {
        return;
    }
    midi_shim_stop();
    g_midi_running = false;
}

void midi_input_list_ports(void) {
    midi_shim_list_ports();
}
