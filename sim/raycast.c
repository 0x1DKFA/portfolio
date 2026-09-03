#include "raycast.h"
#include "palette.h"
#include "trig.h"
#include "fmath.h"

typedef struct { float dx, dy; } Ray;

static float tan_half_fov(void) { float half = camera_fov() * 0.5f; return trig_sin(half) / trig_cos(half); }

static Ray ray_for(const Camera *cam, int w, int col) {
    float ca = trig_cos(cam->angle), sa = trig_sin(cam->angle);
    float camera_x = 2.0f * ((float)col + 0.5f) / (float)w - 1.0f;
    float t = tan_half_fov();
    Ray r = { ca + (-sa) * t * camera_x, sa + ca * t * camera_x };
    return r;
}

float raycast_shade(float dist, float light, int side) {
    float fog = 1.0f / (1.0f + RAY_FOG * dist);
    float s = fog * (0.45f + 0.55f * light) * (side ? RAY_SIDE_SHADE : 1.0f);
    return s > 1.0f ? 1.0f : s;
}

float raycast_row_dist(float proj, int row, int horizon) {
    int d = row - horizon; if (d < 1) d = 1;
    return RAY_WALL_BOTTOM * proj / (float)d;
}

int raycast_column(const Map *m, const Camera *cam, float proj, int w, int col, RayHit *out) {
    (void)proj;
    Ray r = ray_for(cam, w, col);
    int map_x = (int)cam->x, map_y = (int)cam->y;
    float delta_x = r.dx == 0.0f ? 1e30f : fm_abs(1.0f / r.dx);
    float delta_y = r.dy == 0.0f ? 1e30f : fm_abs(1.0f / r.dy);
    int step_x = r.dx < 0 ? -1 : 1, step_y = r.dy < 0 ? -1 : 1;
    float side_x = r.dx < 0 ? (cam->x - (float)map_x) * delta_x : ((float)map_x + 1.0f - cam->x) * delta_x;
    float side_y = r.dy < 0 ? (cam->y - (float)map_y) * delta_y : ((float)map_y + 1.0f - cam->y) * delta_y;
    int side = 0, prev_x = map_x, prev_y = map_y;
    out->hit = 0;
    for (int i = 0; i < 256; i++) {
        prev_x = map_x; prev_y = map_y;
        if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = 0; }
        else                 { side_y += delta_y; map_y += step_y; side = 1; }
        if (!map_is_floor(m, map_x, map_y)) { out->hit = 1; break; }
    }
    if (!out->hit) { out->dist = RAY_MAX_DIST; return 0; }
    float dist = side == 0 ? side_x - delta_x : side_y - delta_y;
    if (dist < 0.01f) dist = 0.01f;
    float wall = side == 0 ? cam->y + dist * r.dy : cam->x + dist * r.dx;
    float u = wall - (float)(int)wall; if (u < 0) u += 1.0f;
    if ((side == 0 && r.dx > 0) || (side == 1 && r.dy < 0)) u = 1.0f - u;
    out->dist = dist; out->side = side; out->tile_x = map_x; out->tile_y = map_y;
    out->tex = texture_for_cell(map_wall_kind(m, map_x, map_y));
    out->u = u; out->light = map_light(m, prev_x, prev_y);
    out->variant = texture_hash(map_x, map_y);
    return 1;
}

void raycast_sky(const Textures *t, const Camera *cam, float proj, Framebuffer *fb, int horizon) {
    (void)proj;
    int rows = horizon < fb->h ? horizon : fb->h;
    float th = tan_half_fov();
    for (int col = 0; col < fb->w; col++) {
        float camera_x = 2.0f * ((float)col + 0.5f) / (float)fb->w - 1.0f;
        float a = trig_wrap(cam->angle + trig_atan2(camera_x * th, 1.0f));
        float turns = (a + TRIG_PI) / TRIG_TAU;                       /* 0..1 */
        int sx = (int)(turns * (float)SKY_W);
        int base = SKY_H - horizon;
        for (int row = 0; row < rows; row++) {
            Color c = texture_sky(t, sx, row + base);
            uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
            px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
        }
    }
}

void raycast_walls(const Map *m, const Textures *t, const Color *pal, const Camera *cam, float proj,
                   Framebuffer *fb, int horizon, float *depth) {
    for (int col = 0; col < fb->w; col++) {
        RayHit hit;
        raycast_column(m, cam, proj, fb->w, col, &hit);
        depth[col] = hit.dist;
        if (!hit.hit) continue;
        float top_f = (float)horizon - RAY_WALL_TOP / hit.dist * proj;
        float bot_f = (float)horizon + RAY_WALL_BOTTOM / hit.dist * proj;
        int top = (int)top_f, bot = (int)bot_f;
        float span = bot_f - top_f; if (span < 1.0f) span = 1.0f;
        int r0 = top < 0 ? 0 : top, r1 = bot > fb->h ? fb->h : bot;
        int tu = (int)(hit.u * (float)TEX_SIZE); if (tu >= TEX_SIZE) tu = TEX_SIZE - 1;
        float shade = raycast_shade(hit.dist, hit.light, hit.side);
        for (int row = r0; row < r1; row++) {
            int tv = (int)(((float)row - top_f) / span * (float)TEX_SIZE);
            if (tv < 0) tv = 0; if (tv >= TEX_SIZE) tv = TEX_SIZE - 1;
            Color c = draw_shade(texture_wall(t, pal, hit.tex, hit.variant, tu, tv), shade);
            uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
            px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
        }
    }
}

static Color mix(Color a, Color b, float t) {
    Color c;
    c.r = (uint8_t)((float)a.r + ((float)b.r - (float)a.r) * t);
    c.g = (uint8_t)((float)a.g + ((float)b.g - (float)a.g) * t);
    c.b = (uint8_t)((float)a.b + ((float)b.b - (float)a.b) * t);
    c.a = 255;
    return c;
}

void raycast_floor(const Map *m, const Textures *t, const Color *pal, const Decals *d, const Camera *cam, float proj,
                   Framebuffer *fb, int horizon, const float *depth) {
    for (int col = 0; col < fb->w; col++) {
        Ray r = ray_for(cam, fb->w, col);
        int start = horizon + (int)(RAY_WALL_BOTTOM / depth[col] * proj) + 1;
        if (start < horizon + 1) start = horizon + 1;
        for (int row = start; row < fb->h; row++) {
            float dist = raycast_row_dist(proj, row, horizon);
            float fx = cam->x + dist * r.dx, fy = cam->y + dist * r.dy;
            int tx = (int)fx, ty = (int)fy;
            if (fx < 0 || fy < 0 || tx >= m->w || ty >= m->h) continue;
            int tex = m->floor_kind[ty][tx] == FLOOR_PUDDLE ? TEX_PUDDLE : TEX_ASPHALT;
            int u = (int)((fx - (float)tx) * (float)TEX_SIZE), v = (int)((fy - (float)ty) * (float)TEX_SIZE);
            if (u >= TEX_SIZE) u = TEX_SIZE - 1; if (v >= TEX_SIZE) v = TEX_SIZE - 1;
            float fog = 1.0f / (1.0f + RAY_FOG * dist);
            Color c;
            float glow = decals_sample(d, fx, fy);
            if (glow > 0.0f) c = draw_shade(mix(pal[COL_FOOTPRINT_DIM], pal[COL_FOOTPRINT], glow), 0.6f + 0.4f * fog);
            else c = draw_shade(t->wall[tex][v * TEX_SIZE + u], raycast_shade(dist, map_light(m, tx, ty), 0));
            uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
            px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
        }
    }
}
