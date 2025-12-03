#ifndef UI_DRAW_HELPERS_H
#define UI_DRAW_HELPERS_H

#include "nuklear.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_draw_panel_bezel(struct nk_command_buffer* cmd, struct nk_rect bounds, float corner_radius);
void ui_draw_panel_inset(struct nk_command_buffer* cmd, struct nk_rect bounds, float corner_radius);

void ui_draw_knob(struct nk_command_buffer* cmd,
                  struct nk_vec2 center,
                  float diameter,
                  float normalized_value,
                  float highlight);

void ui_draw_led(struct nk_command_buffer* cmd,
                 struct nk_vec2 center,
                 float intensity);

void ui_draw_small_label(struct nk_command_buffer* cmd,
                         const struct nk_user_font* font,
                         struct nk_rect bounds,
                         const char* text,
                         enum nk_text_alignment align);

void ui_draw_mod_meter(struct nk_command_buffer* cmd,
                       struct nk_rect bounds,
                       float amount);

#ifdef __cplusplus
}
#endif

#endif // UI_DRAW_HELPERS_H
