#ifndef UI_KNOB_CUSTOM_H
#define UI_KNOB_CUSTOM_H

#include <stdbool.h>

#include "nuklear.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UiKnobState {
    float min_value;
    float max_value;
    float default_value;
    float value;
    float normalized;
    float display_normalized;
    float perceptual_value;
    int dragging;
    double elapsed_time;
    double last_click_time;
} UiKnobState;

typedef struct UiKnobConfig {
    const char* label;
    const char* unit;
    float snap_increment;      // applied when ALT is held (0 disables)
    float sensitivity;         // perceptual delta per pixel (default 0.0035)
    float fine_modifier;       // multiplier when SHIFT is held (default 0.1)
    float double_click_window; // seconds (default 0.25)
    float diameter_override;   // optional explicit diameter in px (0 = auto)
} UiKnobConfig;

void ui_knob_state_init(UiKnobState* knob,
                        float min_value,
                        float max_value,
                        float default_value);

// Renders the knob in the current layout cell. Returns true if the value changed.
bool ui_knob_render(struct nk_context* ctx,
                    UiKnobState* knob,
                    const UiKnobConfig* config);

#ifdef __cplusplus
}
#endif

#endif // UI_KNOB_CUSTOM_H
