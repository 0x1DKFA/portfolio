#ifndef SIM_H_HEADER
#define SIM_H_HEADER
#include <stdint.h>
#include "events.h"

int      sim_init(uint32_t seed, int w, int h);
void     sim_update(int elapsed_ms);
void     sim_render(void);
uint8_t *sim_framebuffer(void);
int      sim_framebuffer_len(void);
uint32_t sim_poll_event(void);
void     sim_render_static(void);

#endif
