#ifndef RAYCAST_H_HEADER
#define RAYCAST_H_HEADER
#include <stdint.h>
#include "draw.h"
#include "map.h"
#include "camera.h"
#include "textures.h"
#include "decals.h"

#define RAY_WALL_TOP 2.0f
#define RAY_WALL_BOTTOM 0.5f
#define RAY_FOG 0.35f
#define RAY_SIDE_SHADE 0.8f
#define RAY_MAX_DIST 64.0f

typedef struct {
    int hit; float dist; int side; int tex; int tile_x, tile_y; float u; float light; uint32_t variant;
} RayHit;

float raycast_shade(float dist, float light, int side);
float raycast_row_dist(float proj, int row, int horizon);
int   raycast_column(const Map *m, const Camera *cam, float proj, int w, int col, RayHit *out);
void  raycast_sky(const Textures *t, const Camera *cam, float proj, Framebuffer *fb, int horizon);
void  raycast_walls(const Map *m, const Textures *t, const Color *pal, const Camera *cam, float proj,
                    Framebuffer *fb, int horizon, float *depth);
void  raycast_floor(const Map *m, const Textures *t, const Color *pal, const Decals *d, const Camera *cam, float proj,
                    Framebuffer *fb, int horizon, const float *depth);

#endif
