#ifndef SIM_H
#define SIM_H
#include <stdint.h>
#include "events.h"

#define AVATAR_W 24
#define AVATAR_H 24

int      sim_init(uint32_t seed, int panel_w, int panel_h, int gap_w);
void     sim_update(int elapsed_ms);
void     sim_render(void);
uint8_t *sim_framebuffer(void);
int      sim_framebuffer_len(void);
void     sim_request(int vignette_id);
uint32_t sim_poll_event(void);
void     sim_render_static(void);
void     sim_render_avatar(void);
uint8_t *sim_avatar_buffer(void);
int      sim_avatar_len(void);

#endif
