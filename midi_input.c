#include "midi_input.h"
#include "pa_ringbuffer.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MIDI_QUEUE_SIZE 512

static PaUtilRingBuffer g_midi_queue;
static MidiEvent g_midi_buffer[MIDI_QUEUE_SIZE];
static bool g_queue_ready = false;
static bool g_midi_running = false;

static void midi_queue_init(void) {
    if (g_queue_ready) {
        return;
    }
    PaUtil_InitializeRingBuffer(&g_midi_queue,
                                sizeof(MidiEvent),
                                MIDI_QUEUE_SIZE,
                                g_midi_buffer);
    g_queue_ready = true;
}

void midi_queue_push_event(const MidiEvent* event) {
    if (!g_queue_ready || !event) {
        return;
    }
    ring_buffer_size_t written = PaUtil_WriteRingBuffer(&g_midi_queue, event, 1);
    if (written == 0) {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr, "⚠️ MIDI queue overflow — dropping events.\n");
            warned = 1;
        }
    }
}

static int midi_queue_pop(MidiEvent* out_event) {
    if (!g_queue_ready || !out_event) {
        return 0;
    }
    return (int)PaUtil_ReadRingBuffer(&g_midi_queue, out_event, 1);
}

void midi_queue_drain(midi_event_handler handler, void* userdata) {
    if (!handler) {
        return;
    }
    MidiEvent event;
    while (midi_queue_pop(&event) == 1) {
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

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

static MIDIClientRef g_midi_client = 0;
static MIDIPortRef g_midi_input_port = 0;

static void midi_handle_message(uint8_t status, uint8_t data1, uint8_t data2) {
    MidiEvent event = {0};
    uint8_t status_type = status & 0xF0;
    uint8_t channel = status & 0x0F;
    event.channel = channel;

    switch (status_type) {
        case 0x80: // Note Off
            event.type = MIDI_EVENT_NOTE_OFF;
            event.data1 = data1;
            event.data2 = data2;
            break;
        case 0x90: // Note On
            if (data2 == 0) {
                event.type = MIDI_EVENT_NOTE_OFF;
            } else {
                event.type = MIDI_EVENT_NOTE_ON;
            }
            event.data1 = data1;
            event.data2 = data2;
            break;
        case 0xA0: // Polyphonic aftertouch
            event.type = MIDI_EVENT_AFTERTOUCH;
            event.data1 = data1;
            event.data2 = data2;
            break;
        case 0xB0: // Control Change
            event.type = MIDI_EVENT_CONTROL_CHANGE;
            event.data1 = data1;
            event.data2 = data2;
            break;
        case 0xC0: // Program Change
            event.type = MIDI_EVENT_PROGRAM_CHANGE;
            event.data1 = data1;
            event.data2 = 0;
            break;
        case 0xD0: // Channel aftertouch
            event.type = MIDI_EVENT_AFTERTOUCH;
            event.data1 = 0;
            event.data2 = data1;
            break;
        case 0xE0: // Pitch Bend
            event.type = MIDI_EVENT_PITCH_BEND;
            event.data1 = data1;
            event.data2 = data2;
            break;
        default:
            return;
    }

    midi_queue_push_event(&event);
}

static void midi_parse_packet(const uint8_t* bytes, size_t length) {
    if (!bytes || length == 0) {
        return;
    }

    uint8_t running_status = 0;
    size_t i = 0;
    while (i < length) {
        uint8_t byte = bytes[i];

        if (byte & 0x80) {
            // Status byte
            running_status = byte;
            i++;
            continue;
        }

        if (running_status == 0) {
            // Data byte without status — skip
            i++;
            continue;
        }

        uint8_t status_type = running_status & 0xF0;
        switch (status_type) {
            case 0xC0:
            case 0xD0: {
                // Program change / channel pressure: 1 data byte
                midi_handle_message(running_status, byte, 0);
                i++;
                break;
            }
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            case 0xE0: {
                if (i + 1 >= length) {
                    return; // Wait for more data
                }
                uint8_t data1 = byte;
                uint8_t data2 = bytes[i + 1];
                midi_handle_message(running_status, data1, data2);
                i += 2;
                break;
            }
            default:
                // Unsupported / system common — skip
                i++;
                break;
        }
    }
}

static void midi_read_proc(const MIDIPacketList* pktlist,
                           void* readProcRefCon,
                           void* srcConnRefCon) {
    (void)readProcRefCon;
    (void)srcConnRefCon;

    const MIDIPacket* packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        midi_parse_packet(packet->data, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

void midi_input_start(void) {
    if (g_midi_running) {
        return;
    }

    midi_queue_init();

    if (MIDIClientCreate(CFSTR("SynthPro MIDI"), NULL, NULL, &g_midi_client) != noErr) {
        fprintf(stderr, "⚠️ Unable to create MIDI client.\n");
        return;
    }

    if (MIDIInputPortCreate(g_midi_client,
                            CFSTR("Input"),
                            midi_read_proc,
                            NULL,
                            &g_midi_input_port) != noErr) {
        fprintf(stderr, "⚠️ Unable to create MIDI input port.\n");
        MIDIClientDispose(g_midi_client);
        g_midi_client = 0;
        return;
    }

    ItemCount sources = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < sources; ++i) {
        MIDIEndpointRef source = MIDIGetSource(i);
        if (source != 0) {
            MIDIPortConnectSource(g_midi_input_port, source, NULL);
        }
    }

    printf("🎛  MIDI input online (%lu sources).\n", (unsigned long)sources);
    g_midi_running = true;
}

void midi_input_stop(void) {
    if (!g_midi_running) {
        return;
    }

    MIDIPortDispose(g_midi_input_port);
    g_midi_input_port = 0;
    MIDIClientDispose(g_midi_client);
    g_midi_client = 0;
    g_midi_running = false;
    printf("⏹  MIDI input stopped.\n");
}

#else // !__APPLE__

void midi_input_start(void) {
    midi_queue_init();
    if (!g_midi_running) {
        printf("⚠️ MIDI input not implemented on this platform build.\n");
        g_midi_running = true;
    }
}

void midi_input_stop(void) {
    if (g_midi_running) {
        printf("ℹ️ MIDI input stub shutdown.\n");
        g_midi_running = false;
    }
}

#endif // __APPLE__
