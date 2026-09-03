#include "camera.h"
#include "trig.h"
#include "fmath.h"

void camera_init(Camera *c, float x, float y, float angle) {
    c->x = x; c->y = y; c->angle = trig_wrap(angle);
    c->target_x = x; c->target_y = y; c->walking = 0; c->auto_face = 0;
    c->target_angle = c->angle; c->turning = 0;
    c->bob_phase = 0.0f; c->pitch_px = 0; c->speed = CAM_WALK_SPEED;
}

void camera_walk_to(Camera *c, float x, float y) { c->target_x = x; c->target_y = y; c->walking = 1; c->auto_face = 1; }
void camera_stop(Camera *c) { c->walking = 0; }

void camera_face(Camera *c, float x, float y) {
    c->target_angle = trig_atan2(y - c->y, x - c->x); c->turning = 1; c->auto_face = 0;
}

void camera_turn_to(Camera *c, float angle) { c->target_angle = trig_wrap(angle); c->turning = 1; c->auto_face = 0; }

int camera_arrived(const Camera *c) { return !c->walking; }

int camera_facing(const Camera *c, float tol) { return fm_abs(trig_wrap(c->target_angle - c->angle)) <= tol; }

float camera_dist(const Camera *c, float x, float y) {
    float dx = x - c->x, dy = y - c->y;
    return fm_sqrt(dx * dx + dy * dy);
}

void camera_step(Camera *c, float dt) {
    if (c->walking) {
        float dx = c->target_x - c->x, dy = c->target_y - c->y;
        float dist = fm_sqrt(dx * dx + dy * dy), step = c->speed * dt;
        if (c->auto_face && dist > 0.001f) { c->target_angle = trig_atan2(dy, dx); c->turning = 1; }
        if (dist <= step || dist < 0.0005f) { c->x = c->target_x; c->y = c->target_y; c->walking = 0; }
        else { c->x += dx / dist * step; c->y += dy / dist * step; c->bob_phase += TRIG_TAU * CAM_STEP_HZ * dt; }
    }
    if (c->turning) {
        float d = trig_wrap(c->target_angle - c->angle), maxd = CAM_TURN_RATE * dt;
        if (fm_abs(d) <= maxd) { c->angle = trig_wrap(c->target_angle); c->turning = 0; }
        else c->angle = trig_wrap(c->angle + (d > 0 ? maxd : -maxd));
    }
}

int camera_bob_px(const Camera *c) {
    if (!c->walking) return 0;
    float b = trig_sin(c->bob_phase) * (float)CAM_BOB_PX;
    return (int)(b + (b >= 0 ? 0.5f : -0.5f));
}

float camera_fov(void) { return CAM_FOV_DEG * TRIG_PI / 180.0f; }

float camera_proj(int w) {
    float half = camera_fov() * 0.5f;
    return ((float)w * 0.5f) * trig_cos(half) / trig_sin(half);
}
