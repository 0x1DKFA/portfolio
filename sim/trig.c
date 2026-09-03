#include "trig.h"

static float g_table[TRIG_TABLE + 1];
static int g_ready;

/* Taylor series on [-pi/2, pi/2]; error under 4e-6 there. */
static float taylor_sin(float x) {
    float x2 = x * x;
    float term = x, sum = x;
    term *= -x2 / (2.0f * 3.0f);  sum += term;
    term *= -x2 / (4.0f * 5.0f);  sum += term;
    term *= -x2 / (6.0f * 7.0f);  sum += term;
    term *= -x2 / (8.0f * 9.0f);  sum += term;
    term *= -x2 / (10.0f * 11.0f); sum += term;
    return sum;
}

/* Exact-ish sine for the table: reduce to [-pi/2, pi/2] by symmetry. */
static float ref_sin(float a) {
    if (a > TRIG_PI) a -= TRIG_TAU;
    if (a > TRIG_HALF_PI) a = TRIG_PI - a;
    else if (a < -TRIG_HALF_PI) a = -TRIG_PI - a;
    return taylor_sin(a);
}

void trig_init(void) {
    if (g_ready) return;
    for (int i = 0; i <= TRIG_TABLE; i++) g_table[i] = ref_sin((float)i * TRIG_TAU / (float)TRIG_TABLE);
    g_ready = 1;
}

float trig_wrap(float a) {
    if (a != a) return 0.0f;                                   /* NaN */
    if (a > 1e9f || a < -1e9f) return 0.0f;                    /* beyond the supported range; see trig.h */
    a -= (float)(int)(a / TRIG_TAU) * TRIG_TAU;                /* now within one turn of zero */
    if (a > TRIG_PI) a -= TRIG_TAU;
    if (a <= -TRIG_PI) a += TRIG_TAU;
    return a;
}

float trig_sin(float a) {
    if (!g_ready) trig_init();
    float t = a / TRIG_TAU;
    t -= (float)(int)t;                /* fractional turns, may be negative */
    if (t < 0.0f) t += 1.0f;
    float pos = t * (float)TRIG_TABLE;
    int i = (int)pos;
    float frac = pos - (float)i;
    if (i >= TRIG_TABLE) { i = TRIG_TABLE - 1; frac = 1.0f; }
    return g_table[i] + (g_table[i + 1] - g_table[i]) * frac;
}

float trig_cos(float a) { return trig_sin(a + TRIG_HALF_PI); }

/* atan on [0, 1], max error ~1e-5 (Abramowitz & Stegun 4.4.49 style polynomial). */
static float atan_unit(float t) {
    float t2 = t * t;
    return t * (0.99997726f + t2 * (-0.33262347f + t2 * (0.19354346f + t2 * (-0.11643287f
           + t2 * (0.05265332f + t2 * (-0.01172120f))))));
}

float trig_atan2(float y, float x) {
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    float r;
    if (ay <= ax) r = atan_unit(ay / ax);
    else          r = TRIG_HALF_PI - atan_unit(ax / ay);
    if (x < 0) r = TRIG_PI - r;
    if (y < 0) r = -r;
    return r;
}
