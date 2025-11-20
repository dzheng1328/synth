#include "knob_custom.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "draw_helpers.h"
#include "style.h"

#ifndef NK_PI
#define NK_PI 3.14159265358979323846f
#endif

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static float to_normalized(const UiKnobState* knob, float value) {
    const float range = knob->max_value - knob->min_value;
    if (range <= 0.0f) return 0.0f;
    return clampf((value - knob->min_value) / range, 0.0f, 1.0f);
}

static float from_normalized(const UiKnobState* knob, float normalized) {
    return knob->min_value + normalized * (knob->max_value - knob->min_value);
}

static float perceptual_from_normalized(float normalized) {
    return powf(clampf(normalized, 0.0f, 1.0f), 1.0f / 3.0f);
}

static float normalized_from_perceptual(float perceptual) {
    float p = clampf(perceptual, 0.0f, 1.0f);
    return p * p * p;
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

void ui_knob_state_init(UiKnobState* knob,
                        float min_value,
                        float max_value,
                        float default_value) {
    if (!knob) return;
    knob->min_value = min_value;
    knob->max_value = max_value > min_value ? max_value : min_value + 1.0f;
    knob->default_value = clampf(default_value, knob->min_value, knob->max_value);
    knob->value = knob->default_value;
    knob->normalized = to_normalized(knob, knob->value);
    knob->display_normalized = knob->normalized;
    knob->perceptual_value = perceptual_from_normalized(knob->normalized);
    knob->dragging = 0;
    knob->elapsed_time = 0.0;
    knob->last_click_time = -1.0;
}

static void set_value_from_perceptual(UiKnobState* knob, float perceptual) {
    knob->perceptual_value = clampf(perceptual, 0.0f, 1.0f);
    knob->normalized = normalized_from_perceptual(knob->perceptual_value);
    knob->value = from_normalized(knob, knob->normalized);
}

bool ui_knob_render(struct nk_context* ctx,
                    UiKnobState* knob,
                    const UiKnobConfig* config) {
    if (!ctx || !knob) return false;

    const UiMetrics* metrics = ui_style_metrics();
    const UiTypography* type = ui_style_typography();
    const UiAnimationTokens* anim = ui_style_animation();

    struct nk_rect bounds = nk_widget_bounds(ctx);
    if (!nk_widget(&bounds, ctx)) {
        return false;
    }

    struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
    struct nk_input* in = &ctx->input;

    float delta_time = ctx->delta_time_seconds;
    if (delta_time <= 0.0f) {
        delta_time = 1.0f / 60.0f;
    }
    knob->elapsed_time += delta_time;

    const float label_space = metrics->knob_label_spacing + type->micro_label_px;
    float diameter = config && config->diameter_override > 0.0f
                         ? config->diameter_override
                         : metrics->knob_diameter_large;
    diameter = fminf(diameter, bounds.w);
    diameter = fminf(diameter, bounds.h - label_space);
    diameter = fmaxf(diameter, metrics->knob_diameter_small);

    struct nk_rect knob_rect = bounds;
    knob_rect.h = diameter;
    knob_rect.y += 4.0f;
    knob_rect.w = diameter;
    knob_rect.x = bounds.x + (bounds.w - diameter) * 0.5f;

    struct nk_vec2 center = {
        knob_rect.x + knob_rect.w * 0.5f,
        knob_rect.y + knob_rect.h * 0.5f
    };

    bool value_changed = false;
    const float base_sensitivity = (config && config->sensitivity > 0.0f)
                                       ? config->sensitivity
                                       : 0.0035f;
    const float fine_modifier = (config && config->fine_modifier > 0.0f)
                                    ? config->fine_modifier
                                    : 0.1f;
    const float double_click_window = (config && config->double_click_window > 0.0f)
                                          ? config->double_click_window
                                          : 0.25f;

    const struct nk_rect input_rect = knob_rect;
    const nk_bool hovered = nk_input_is_mouse_hovering_rect(in, input_rect);

    if (nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, input_rect, nk_true)) {
        if (!knob->dragging) {
            knob->dragging = 1;
        }
    } else if (!in->mouse.buttons[NK_BUTTON_LEFT].down) {
        knob->dragging = 0;
    }

    if (knob->dragging && in->mouse.buttons[NK_BUTTON_LEFT].down) {
        float delta = -in->mouse.delta.y;
        if (in->keyboard.keys[NK_KEY_SHIFT].down) {
            delta *= fine_modifier;
        }
        float perceptual = knob->perceptual_value + delta * base_sensitivity;
        set_value_from_perceptual(knob, perceptual);
        value_changed = true;

        if (config && config->snap_increment > 0.0f && in->keyboard.keys[NK_KEY_CTRL].down) {
            float snap = config->snap_increment;
            float snapped = roundf(knob->value / snap) * snap;
            snapped = clampf(snapped, knob->min_value, knob->max_value);
            float normalized = clampf((snapped - knob->min_value) / (knob->max_value - knob->min_value), 0.0f, 1.0f);
            set_value_from_perceptual(knob, perceptual_from_normalized(normalized));
        }
    }

    if (hovered && in->mouse.buttons[NK_BUTTON_LEFT].clicked) {
        double since_last = knob->elapsed_time - knob->last_click_time;
        if (knob->last_click_time >= 0.0 && since_last <= double_click_window) {
            set_value_from_perceptual(knob, perceptual_from_normalized(to_normalized(knob, knob->default_value)));
            value_changed = true;
        }
        knob->last_click_time = knob->elapsed_time;
    }

    knob->display_normalized = lerp(knob->display_normalized, knob->normalized, anim->knob_lerp_factor);

    ui_draw_knob(canvas, center, diameter, knob->display_normalized, (hovered || knob->dragging) ? 1.0f : 0.0f);

    if (config && config->label) {
        const struct nk_user_font* font = ctx->style.font;
        struct nk_rect label_rect = {
            bounds.x,
            knob_rect.y + knob_rect.h + metrics->knob_label_spacing * 0.5f,
            bounds.w,
            type->micro_label_px + 6.0f
        };
    ui_draw_small_label(canvas, font, label_rect, config->label, NK_TEXT_CENTERED);
    }

    if (hovered) {
        char tooltip[64];
        if (config && config->unit) {
            snprintf(tooltip, sizeof(tooltip), "%.2f %s", knob->value, config->unit);
        } else {
            snprintf(tooltip, sizeof(tooltip), "%.2f", knob->value);
        }
        nk_tooltip(ctx, tooltip);
    }

    return value_changed;
}
