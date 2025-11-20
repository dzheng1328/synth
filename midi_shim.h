#ifndef MIDI_SHIM_H
#define MIDI_SHIM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the underlying platform MIDI subsystem.
bool midi_shim_start(void);

// Shutdown the MIDI subsystem.
void midi_shim_stop(void);

// Print the available hardware MIDI ports.
void midi_shim_list_ports(void);

#ifdef __cplusplus
}
#endif

#endif // MIDI_SHIM_H
