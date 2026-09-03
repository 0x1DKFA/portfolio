#include "test.h"
#include "trig.h"

void test_trig(void) {
    trig_init();
    trig_init();                                    /* idempotent */
    CHECK_NEAR(trig_sin(0.0f), 0.0f, 1e-4);
    CHECK_NEAR(trig_sin(TRIG_PI / 6.0f), 0.5f, 1e-3);
    CHECK_NEAR(trig_sin(TRIG_PI / 4.0f), 0.70710678f, 1e-3);
    CHECK_NEAR(trig_sin(TRIG_HALF_PI), 1.0f, 1e-3);
    CHECK_NEAR(trig_sin(TRIG_PI), 0.0f, 1e-3);
    CHECK_NEAR(trig_sin(-TRIG_HALF_PI), -1.0f, 1e-3);
    CHECK_NEAR(trig_sin(7.0f * TRIG_TAU + TRIG_PI / 6.0f), 0.5f, 1e-3);   /* wraps many turns */
    CHECK_NEAR(trig_cos(0.0f), 1.0f, 1e-3);
    CHECK_NEAR(trig_cos(TRIG_PI / 3.0f), 0.5f, 1e-3);
    CHECK_NEAR(trig_cos(TRIG_PI), -1.0f, 1e-3);
    CHECK_NEAR(trig_cos(-TRIG_PI / 3.0f), 0.5f, 1e-3);
    for (int i = 0; i < 360; i++) {                 /* sin^2 + cos^2 = 1 */
        float a = (float)i * TRIG_TAU / 360.0f;
        float s = trig_sin(a), c = trig_cos(a);
        CHECK_NEAR(s * s + c * c, 1.0f, 2e-3);
    }
    CHECK_NEAR(trig_atan2(0.0f, 1.0f), 0.0f, 0.005);
    CHECK_NEAR(trig_atan2(1.0f, 1.0f), TRIG_PI / 4.0f, 0.005);
    CHECK_NEAR(trig_atan2(1.0f, 0.0f), TRIG_HALF_PI, 0.005);
    CHECK_NEAR(trig_atan2(1.0f, -1.0f), 3.0f * TRIG_PI / 4.0f, 0.005);
    CHECK_NEAR(trig_atan2(0.0f, -1.0f), TRIG_PI, 0.005);
    CHECK_NEAR(trig_atan2(-1.0f, -1.0f), -3.0f * TRIG_PI / 4.0f, 0.005);
    CHECK_NEAR(trig_atan2(-1.0f, 0.0f), -TRIG_HALF_PI, 0.005);
    CHECK_NEAR(trig_atan2(-1.0f, 1.0f), -TRIG_PI / 4.0f, 0.005);
    CHECK_NEAR(trig_atan2(0.5f, 2.0f), 0.24497866f, 0.005);
    CHECK_NEAR(trig_atan2(0.0f, 0.0f), 0.0f, 1e-6);
    CHECK_NEAR(trig_wrap(0.0f), 0.0f, 1e-6);
    CHECK_NEAR(trig_wrap(TRIG_TAU + 0.5f), 0.5f, 1e-4);
    CHECK_NEAR(trig_wrap(-TRIG_TAU - 0.5f), -0.5f, 1e-4);
    CHECK_NEAR(trig_wrap(TRIG_PI + 0.1f), -TRIG_PI + 0.1f, 1e-4);
    CHECK(trig_wrap(TRIG_PI) > 3.14f && trig_wrap(TRIG_PI) <= TRIG_PI + 1e-6f);
}
