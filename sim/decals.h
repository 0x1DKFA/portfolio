#ifndef DECALS_H_HEADER
#define DECALS_H_HEADER
#include <stdint.h>
#include "map.h"

#define DECAL_MAX 512
#define DECAL_PER_TILE 8
#define DECAL_SPACING 0.5f
#define DECAL_OFFSET 0.15f
#define DECAL_HALF_LEN 0.22f
#define DECAL_HALF_WID 0.09f
#define DECAL_GLOW_BRIGHT_PAIRS 8
#define DECAL_GLOW_DIM 0.35f

typedef struct { float x, y, ca, sa; int index; int active; } Footprint;

typedef struct {
    Footprint p[DECAL_MAX];
    int n;
    int16_t cell[MAP_H][MAP_W][DECAL_PER_TILE];   /* print indices, -1 empty */
    int wp_first[MAP_MAX_PATH];                    /* first print index laid at or after waypoint i */
    int head;                                      /* prints with index >= head are ahead */
} Decals;

void  decals_clear(Decals *d);
int   decals_lay_trail(Decals *d, const Path *path);
void  decals_set_head_waypoint(Decals *d, int waypoint);
float decals_sample(const Decals *d, float fx, float fy);
int   decals_tile_has_print(const Decals *d, int tx, int ty);
int   decals_cull_behind(Decals *d, float cx, float cy, float max_behind);

#endif
