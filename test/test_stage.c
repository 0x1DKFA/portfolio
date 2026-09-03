#include "test.h"
#include "stage.h"

static const Color TEST_PAL[1] = { { 0, 0, 0, 255 } };

static int steps, renders;
static float last_dt;
static void step_fn(Stage *s, float dt) { (void)s; steps++; last_dt = dt; }
static void render_fn(Stage *s) { (void)s; renders++; }
static uint8_t px[8 * 8 * 4];

void test_stage(void) {
    Stage st;
    int marker = 7;
    stage_init(&st, px, 8, 8, TEST_PAL, step_fn, render_fn, &marker);
    CHECK_EQ(st.fb.w, 8);
    CHECK(st.palette == TEST_PAL);
    CHECK(*(int *)st.state == 7);

    steps = 0;
    int n = 0;
    for (int i = 0; i < 100; i++) n += stage_update(&st, 10);   /* one second in 10ms slices */
    CHECK(n >= 59 && n <= 60);
    CHECK_EQ(steps, n);
    CHECK_NEAR(last_dt, STAGE_STEP, 1e-6);
    CHECK(st.accum >= 0.0f && st.accum < STAGE_STEP);

    steps = 0;
    n = stage_update(&st, 5000);              /* clamped to 250ms */
    CHECK(n >= 14 && n <= 15);

    steps = 0;
    n = stage_update(&st, 0);
    CHECK_EQ(n, 0);
    n = stage_update(&st, -50);
    CHECK_EQ(n, 0);

    /* two 8ms updates make roughly one step */
    st.accum = 0; steps = 0;
    stage_update(&st, 8); stage_update(&st, 9);
    CHECK_EQ(steps, 1);

    renders = 0;
    stage_render(&st);
    CHECK_EQ(renders, 1);
}
