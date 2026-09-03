#include "test.h"
#include "events.h"

void test_events(void) {
    uint32_t e = event_pack(STAGE_BUGS, EV_VIGNETTE_START, 2, 1);
    CHECK(e != 0u);
    CHECK_EQ(event_type(e), EV_VIGNETTE_START);
    CHECK_EQ(event_stage(e), STAGE_BUGS);
    CHECK_EQ(event_a(e), 2);
    CHECK_EQ(event_b(e), 1);
    CHECK(event_pack(STAGE_OASIS, EV_BUG_SQUASHED, 0, 0) != 0u);

    EventQueue q;
    events_init(&q);
    CHECK_EQ(events_count(&q), 0);
    CHECK_EQ(events_pop(&q), 0u);

    events_push(&q, event_pack(0, EV_VIGNETTE_START, 1, 0));
    events_push(&q, event_pack(0, EV_BUG_SQUASHED, 0, 0));
    events_push(&q, event_pack(0, EV_VIGNETTE_END, 1, 0));
    CHECK_EQ(events_count(&q), 3);
    CHECK_EQ(event_type(events_pop(&q)), EV_VIGNETTE_START);
    CHECK_EQ(event_type(events_pop(&q)), EV_BUG_SQUASHED);
    CHECK_EQ(event_type(events_pop(&q)), EV_VIGNETTE_END);
    CHECK_EQ(events_pop(&q), 0u);

    /* overflow drops the oldest */
    for (int i = 0; i < EVENT_QUEUE_CAP + 5; i++)
        events_push(&q, event_pack(0, EV_BUG_SQUASHED, i & 0xFF, 0));
    CHECK_EQ(events_count(&q), EVENT_QUEUE_CAP);
    CHECK_EQ(event_a(events_pop(&q)), 5);
}
