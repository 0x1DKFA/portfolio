#ifndef FMATH_H
#define FMATH_H

static inline float fm_abs(float v) { return v < 0 ? -v : v; }
static inline float fm_min(float a, float b) { return a < b ? a : b; }
static inline float fm_max(float a, float b) { return a > b ? a : b; }
static inline float fm_clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Newton's method; no libm in the freestanding build. */
static inline float fm_sqrt(float v) {
    if (v <= 0.0f) return 0.0f;
    float g = v > 1.0f ? v : 1.0f;
    for (int i = 0; i < 20; i++) g = 0.5f * (g + v / g);
    return g;
}

#endif
