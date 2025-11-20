#ifndef UI_STYLE_H
#define UI_STYLE_H

#include "nuklear.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UiPalette {
    struct nk_color panel_bg;
    struct nk_color panel_bezel;
    struct nk_color panel_inset;
    struct nk_color knob_face;
    struct nk_color knob_highlight;
    struct nk_color accent;
    struct nk_color led_off;
    struct nk_color led_on;
    struct nk_color white_key;
    struct nk_color black_key;
    struct nk_color key_pressed;
    struct nk_color text;
    struct nk_color border;
} UiPalette;

const UiPalette* ui_style_palette(void);

typedef struct UiMetrics {
    float top_panel_height;
    float knob_diameter_large;
    float knob_diameter_small;
    float knob_label_spacing;
    float led_diameter;
    float pitch_mod_wheel_width;
    float pitch_mod_wheel_height;
    float keyboard_height;
    float padding_lr;
    float column_gap;
    float min_layout_width;
} UiMetrics;

const UiMetrics* ui_style_metrics(void);

typedef struct UiAnimationTokens {
    float knob_lerp_factor;
    float led_fade_ms;
    float target_fps;
} UiAnimationTokens;

const UiAnimationTokens* ui_style_animation(void);

typedef struct UiTypography {
    float body_px;
    float micro_label_px;
    float tooltip_px;
} UiTypography;

const UiTypography* ui_style_typography(void);

#ifdef __cplusplus
}
#endif

#endif // UI_STYLE_H
