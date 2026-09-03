#include "events.h"

void events_init(EventQueue *q) { q->head = 0; q->count = 0; }

uint32_t event_pack(int stage, int type, int a, int b) {
    return ((uint32_t)(type & 0xFF) << 24) | ((uint32_t)(stage & 0xFF) << 16)
         | ((uint32_t)(a & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}

int event_type(uint32_t e)  { return (int)((e >> 24) & 0xFF); }
int event_stage(uint32_t e) { return (int)((e >> 16) & 0xFF); }
int event_a(uint32_t e)     { return (int)((e >> 8) & 0xFF); }
int event_b(uint32_t e)     { return (int)(e & 0xFF); }

void events_push(EventQueue *q, uint32_t e) {
    if (q->count == EVENT_QUEUE_CAP) {           /* drop oldest */
        q->head = (q->head + 1) % EVENT_QUEUE_CAP;
        q->count--;
    }
    q->items[(q->head + q->count) % EVENT_QUEUE_CAP] = e;
    q->count++;
}

uint32_t events_pop(EventQueue *q) {
    if (q->count == 0) return 0u;
    uint32_t e = q->items[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_CAP;
    q->count--;
    return e;
}

int events_count(const EventQueue *q) { return q->count; }
