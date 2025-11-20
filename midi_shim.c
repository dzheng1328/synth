#include "midi_shim.h"

#include "midi_input.h"
#include "param_queue.h"

#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

static MIDIClientRef g_midi_client = 0;
static MIDIPortRef g_midi_input_port = 0;
static int g_sources_connected = 0;
static bool g_shim_running = false;

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
            running_status = byte;
            i++;
            continue;
        }

        if (running_status == 0) {
            i++;
            continue;
        }

        uint8_t status_type = running_status & 0xF0;
        switch (status_type) {
            case 0xC0:
            case 0xD0:
                midi_handle_message(running_status, byte, 0);
                i++;
                break;
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            case 0xE0:
                if (i + 1 >= length) {
                    return;
                }
                midi_handle_message(running_status, byte, bytes[i + 1]);
                i += 2;
                break;
            default:
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

static void cfstring_to_cstr(CFStringRef string, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    buffer[0] = '\0';
    if (!string) {
        return;
    }
    CFStringGetCString(string, buffer, (CFIndex)size, kCFStringEncodingUTF8);
}

bool midi_shim_start(void) {
    if (g_shim_running) {
        return true;
    }

    if (MIDIClientCreate(CFSTR("Synth MIDI"), NULL, NULL, &g_midi_client) != noErr) {
        fprintf(stderr, "❌ Unable to create MIDI client.\n");
        return false;
    }

    if (MIDIInputPortCreate(g_midi_client,
                            CFSTR("Input"),
                            midi_read_proc,
                            NULL,
                            &g_midi_input_port) != noErr) {
        fprintf(stderr, "❌ Unable to create MIDI input port.\n");
        MIDIClientDispose(g_midi_client);
        g_midi_client = 0;
        return false;
    }

    ItemCount sources = MIDIGetNumberOfSources();
    g_sources_connected = 0;
    for (ItemCount i = 0; i < sources; ++i) {
        MIDIEndpointRef source = MIDIGetSource(i);
        if (source != 0) {
            if (MIDIPortConnectSource(g_midi_input_port, source, NULL) == noErr) {
                g_sources_connected++;
            }
        }
    }

    g_shim_running = true;
    printf("🎛  MIDI shim online (%d/%lu sources connected).\n",
           g_sources_connected,
           (unsigned long)sources);
    return true;
}

void midi_shim_stop(void) {
    if (!g_shim_running) {
        return;
    }

    MIDIPortDispose(g_midi_input_port);
    g_midi_input_port = 0;
    MIDIClientDispose(g_midi_client);
    g_midi_client = 0;
    g_sources_connected = 0;
    g_shim_running = false;
    printf("⏹  MIDI shim stopped.\n");
}

void midi_shim_list_ports(void) {
    ItemCount sources = MIDIGetNumberOfSources();
    printf("🎹 Detected MIDI sources (%lu):\n", (unsigned long)sources);
    for (ItemCount i = 0; i < sources; ++i) {
        MIDIEndpointRef source = MIDIGetSource(i);
        if (!source) {
            continue;
        }
        CFStringRef name = NULL;
        if (MIDIObjectGetStringProperty(source, kMIDIPropertyDisplayName, &name) != noErr) {
            continue;
        }
        char buffer[256];
        cfstring_to_cstr(name, buffer, sizeof(buffer));
        printf("  • %s\n", buffer[0] ? buffer : "(unnamed port)");
        if (name) {
            CFRelease(name);
        }
    }
    if (sources == 0) {
        printf("  (no sources found)\n");
    }
}

#else // !__APPLE__

bool midi_shim_start(void) {
    fprintf(stderr, "⚠️ MIDI shim unavailable on this platform build.\n");
    return false;
}

void midi_shim_stop(void) {
    // no-op
}

void midi_shim_list_ports(void) {
    printf("ℹ️ MIDI shim list not supported on this platform.\n");
}

#endif // __APPLE__
