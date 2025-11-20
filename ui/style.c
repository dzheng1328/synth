#include "style.h"

static const UiPalette kPalette = {
    .panel_bg        = {26, 26, 26, 255},
    .panel_bezel     = {15, 15, 15, 255},
    .panel_inset     = {37, 37, 37, 255},
    .knob_face       = {43, 43, 43, 255},
    .knob_highlight  = {58, 58, 58, 255},
    .accent          = {255, 90, 0, 255},
    .led_off         = {47, 47, 47, 255},
    .led_on          = {255, 140, 51, 255},
    .white_key       = {244, 240, 230, 255},
    .black_key       = {17, 17, 17, 255},
    .key_pressed     = {214, 184, 139, 255},
    .text            = {237, 237, 237, 255},
    .border          = {27, 27, 27, 255},
};

const UiPalette* ui_style_palette(void) {
    return &kPalette;
}

static const UiMetrics kMetrics = {
    .top_panel_height = 140.0f,
    .knob_diameter_large = 64.0f,
    .knob_diameter_small = 44.0f,
    .knob_label_spacing = 16.0f,
    .led_diameter = 8.0f,
    .pitch_mod_wheel_width = 120.0f,
    .pitch_mod_wheel_height = 80.0f,
    .keyboard_height = 160.0f,
    .padding_lr = 12.0f,
    .column_gap = 8.0f,
    .min_layout_width = 900.0f,
};

const UiMetrics* ui_style_metrics(void) {
    return &kMetrics;
}

static const UiAnimationTokens kAnim = {
    .knob_lerp_factor = 0.25f,
    .led_fade_ms = 150.0f,
    .target_fps = 60.0f,
};

const UiAnimationTokens* ui_style_animation(void) {
    return &kAnim;
}

static const UiTypography kType = {
    .body_px = 14.0f,
    .micro_label_px = 11.0f,
    .tooltip_px = 12.0f,
};

const UiTypography* ui_style_typography(void) {
    return &kType;
}
