#include "draw_helpers.h"

#include <math.h>
#include <string.h>

#include "ui/style.h"

#ifndef NK_PI
#define NK_PI 3.14159265358979323846f
#endif

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void ui_draw_panel_bezel(struct nk_command_buffer* cmd, struct nk_rect bounds, float corner_radius) {
    const UiPalette* palette = ui_style_palette();
    nk_fill_rect(cmd, bounds, corner_radius, palette->panel_bezel);
}

void ui_draw_panel_inset(struct nk_command_buffer* cmd, struct nk_rect bounds, float corner_radius) {
    const UiPalette* palette = ui_style_palette();
    nk_fill_rect(cmd, bounds, corner_radius, palette->panel_inset);
    struct nk_rect inner = bounds;
    const float border = 2.0f;
    inner.x += border;
    inner.y += border;
    inner.w -= border * 2.0f;
    inner.h -= border * 2.0f;
    nk_fill_rect(cmd, inner, corner_radius - 1.0f, palette->panel_bg);
}

void ui_draw_knob(struct nk_command_buffer* cmd,
                  struct nk_vec2 center,
                  float diameter,
                  float normalized_value,
                  float highlight) {
    const UiPalette* palette = ui_style_palette();
    const float clamped_value = clamp01(normalized_value);
    const float radius = diameter * 0.5f;

    struct nk_rect outer = nk_rect(center.x - radius, center.y - radius, diameter, diameter);
    nk_fill_circle(cmd, outer, palette->panel_bezel);

    const float inset = diameter * 0.08f;
    struct nk_rect face = nk_rect(outer.x + inset, outer.y + inset,
                                  outer.w - inset * 2.0f, outer.h - inset * 2.0f);
    nk_fill_circle(cmd, face, palette->knob_face);

    if (highlight > 0.0f) {
        struct nk_color hl = palette->knob_highlight;
        hl.a = (nk_byte)(clamp01(highlight) * 255.0f);
        nk_stroke_circle(cmd, face, 2.0f, hl);
    }

    const float start_angle = -150.0f * (NK_PI / 180.0f);
    const float sweep = 300.0f * (NK_PI / 180.0f);
    const float angle = start_angle + sweep * clamped_value;

    const float indicator_len = radius * 0.75f;
    struct nk_vec2 start = center;
    struct nk_vec2 end = {
        center.x + cosf(angle) * indicator_len,
        center.y + sinf(angle) * indicator_len
    };

    nk_stroke_line(cmd, start.x, start.y, end.x, end.y, 3.0f, palette->accent);
}

void ui_draw_led(struct nk_command_buffer* cmd,
                 struct nk_vec2 center,
                 float intensity) {
    const UiPalette* palette = ui_style_palette();
    const float t = clamp01(intensity);

    struct nk_color core = palette->led_off;
    core.r = (nk_byte)(core.r + (palette->led_on.r - core.r) * t);
    core.g = (nk_byte)(core.g + (palette->led_on.g - core.g) * t);
    core.b = (nk_byte)(core.b + (palette->led_on.b - core.b) * t);

    struct nk_color halo = palette->led_on;
    halo.a = (nk_byte)(120 * t);

    const float led_radius = ui_style_metrics()->led_diameter * 0.5f;
    struct nk_rect halo_rect = nk_rect(center.x - led_radius * 1.5f,
                                       center.y - led_radius * 1.5f,
                                       led_radius * 3.0f,
                                       led_radius * 3.0f);
    nk_fill_circle(cmd, halo_rect, halo);

    struct nk_rect core_rect = nk_rect(center.x - led_radius,
                                       center.y - led_radius,
                                       led_radius * 2.0f,
                                       led_radius * 2.0f);
    nk_fill_circle(cmd, core_rect, core);
}

void ui_draw_small_label(struct nk_command_buffer* cmd,
                         const struct nk_user_font* font,
                         struct nk_rect bounds,
                         const char* text,
                         enum nk_text_alignment align) {
    if (!font || !cmd || !text) {
        return;
    }
    const UiPalette* palette = ui_style_palette();
    (void)align;
    nk_draw_text(cmd, bounds, text, (int)strlen(text), font, palette->panel_bg, palette->text);
}

void ui_draw_mod_meter(struct nk_command_buffer* cmd,
                       struct nk_rect bounds,
                       float amount) {
    if (!cmd) {
        return;
    }
    const UiPalette* palette = ui_style_palette();
    const float clamped = amount < -1.0f ? -1.0f : (amount > 1.0f ? 1.0f : amount);

    struct nk_rect outer = bounds;
    nk_fill_rect(cmd, outer, 3.0f, palette->panel_bezel);

    struct nk_rect inner = outer;
    const float inset = 2.0f;
    inner.x += inset;
    inner.y += inset;
    inner.w = inner.w > inset * 2.0f ? inner.w - inset * 2.0f : inner.w;
    inner.h = inner.h > inset * 2.0f ? inner.h - inset * 2.0f : inner.h;
    nk_fill_rect(cmd, inner, 2.0f, palette->panel_inset);

    const float center = inner.x + inner.w * 0.5f;
    const float half_width = inner.w * 0.5f;

    if (clamped >= 0.0f) {
        struct nk_rect positive = {
            center,
            inner.y,
            half_width * clamped,
            inner.h
        };
        nk_fill_rect(cmd, positive, 2.0f, palette->meter_positive);
    } else {
        const float span = half_width * -clamped;
        struct nk_rect negative = {
            center - span,
            inner.y,
            span,
            inner.h
        };
        nk_fill_rect(cmd, negative, 2.0f, palette->meter_negative);
    }

    nk_stroke_line(cmd, center, inner.y, center, inner.y + inner.h, 1.0f, palette->border);
}
