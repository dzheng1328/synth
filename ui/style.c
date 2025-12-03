#include "style.h"

static const UiPalette kPalette = {
    .panel_bg        = {17, 22, 36, 255},
    .panel_bezel     = {8, 12, 22, 255},
    .panel_inset     = {28, 34, 52, 255},
    .knob_face       = {36, 42, 60, 255},
    .knob_highlight  = {104, 148, 255, 255},
    .accent          = {82, 232, 255, 255},
    .accent_glow     = {126, 186, 255, 200},
    .led_off         = {48, 54, 74, 255},
    .led_on          = {126, 250, 222, 255},
    .white_key       = {235, 241, 248, 255},
    .black_key       = {14, 18, 28, 255},
    .key_pressed     = {114, 169, 255, 255},
    .text            = {229, 241, 255, 255},
    .border          = {32, 47, 74, 255},
    .panel_header_top    = {24, 34, 60, 255},
    .panel_header_bottom = {8, 12, 22, 255},
    .meter_positive  = {112, 220, 255, 255},
    .meter_negative  = {255, 134, 176, 255},
    .chip_bg         = {33, 45, 68, 255},
    .chip_border     = {66, 104, 156, 255},
};

const UiPalette* ui_style_palette(void) {
    return &kPalette;
}

static const UiMetrics kMetrics = {
    .top_panel_height = 160.0f,
    .knob_diameter_large = 72.0f,
    .knob_diameter_small = 48.0f,
    .knob_label_spacing = 14.0f,
    .led_diameter = 9.0f,
    .pitch_mod_wheel_width = 128.0f,
    .pitch_mod_wheel_height = 84.0f,
    .keyboard_height = 170.0f,
    .padding_lr = 16.0f,
    .column_gap = 12.0f,
    .min_layout_width = 980.0f,
};

const UiMetrics* ui_style_metrics(void) {
    return &kMetrics;
}

static const UiAnimationTokens kAnim = {
    .knob_lerp_factor = 0.35f,
    .led_fade_ms = 180.0f,
    .target_fps = 60.0f,
};

const UiAnimationTokens* ui_style_animation(void) {
    return &kAnim;
}

static const UiTypography kType = {
    .body_px = 15.0f,
    .micro_label_px = 11.0f,
    .tooltip_px = 13.0f,
};

const UiTypography* ui_style_typography(void) {
    return &kType;
}
