#ifndef MAP_H_HEADER
#define MAP_H_HEADER
#include <stdint.h>
#include "rng.h"

#define MAP_W 48
#define MAP_H 48
#define MAP_MAX_PATH 512
#define MAP_MAX_SPAWNS 64

enum { CELL_FLOOR = 0, CELL_BRICK, CELL_CONCRETE, CELL_WINDOW, CELL_NEON, CELL_SIGN, CELL_POSTER, CELL_DUMPSTER, CELL_COUNT };
enum { FLOOR_ASPHALT = 0, FLOOR_PUDDLE = 1 };
enum { PROP_NONE = 0, PROP_LAMP, PROP_TRASH, PROP_PAPER, PROP_NEWSPAPER, PROP_DOG, PROP_START };

typedef struct { int kind; int x, y; } Spawn;
typedef struct { int x, y; } Tile;
typedef struct { Tile t[MAP_MAX_PATH]; int n; } Path;

typedef struct {
    int w, h;
    uint8_t cell[MAP_H][MAP_W];        /* CELL_* */
    uint8_t floor_kind[MAP_H][MAP_W];  /* FLOOR_* (floor cells only) */
    uint8_t prop[MAP_H][MAP_W];        /* PROP_* (floor cells only) */
    float   light[MAP_H][MAP_W];       /* 0..1 */
    Spawn   spawns[MAP_MAX_SPAWNS];
    int     n_spawns;
    int     start_x, start_y;
} Map;

extern const char *const CITY_MAP[MAP_H];

int   map_parse(Map *m, const char *const *rows, int w, int h);   /* 0 ok, -1 bad size or char */
int   map_is_floor(const Map *m, int x, int y);                   /* 0 outside the map */
int   map_is_walkable(const Map *m, int x, int y);                /* floor without solid props */
int   map_wall_kind(const Map *m, int x, int y);                  /* CELL_*; outside is CELL_BRICK */
float map_light(const Map *m, int x, int y);                      /* 0.25 outside */
int   map_bfs(const Map *m, Tile from, Tile to, Path *out);       /* 1 if found; path has both ends */
int   map_bfs_random(const Map *m, Rng *rng, Tile from, Tile to, Path *out);
int   map_distances(const Map *m, Tile from, int16_t *dist);      /* walkable-tile distances; -1 unreachable */
int   map_is_connected(const Map *m);                            /* all walkable tiles are connected */
int   map_is_hiding_spot(const Map *m, int x, int y);             /* floor, 4-adjacent to a trash can or dumpster */
int   map_pick_hiding_spot(const Map *m, Rng *rng, Tile from, int min_steps, int max_steps, Tile *out); /* random spot with min_steps <= dist <= max_steps; else dist >= min_steps; else the farthest */

#endif
