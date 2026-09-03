#ifndef EVENTS_H
#define EVENTS_H
#include <stdint.h>

#define EVENT_QUEUE_CAP 32

enum { STAGE_BUGS = 0, STAGE_OASIS = 1 };
enum { EV_NONE = 0, EV_VIGNETTE_START = 1, EV_VIGNETTE_END = 2, EV_BUG_SQUASHED = 3 };

typedef struct {
    uint32_t items[EVENT_QUEUE_CAP];
    int head;
    int count;
} EventQueue;

void     events_init(EventQueue *q);
uint32_t event_pack(int stage, int type, int a, int b);
int      event_type(uint32_t e);
int      event_stage(uint32_t e);
int      event_a(uint32_t e);
int      event_b(uint32_t e);
void     events_push(EventQueue *q, uint32_t e);
uint32_t events_pop(EventQueue *q);
int      events_count(const EventQueue *q);

#endif
