# Bug Smasher Profile — Phase 1b Implementation Plan (first-person alley hunt)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the two side panels and their vignettes with one full-viewport first-person raycaster: a night-time city of back alleys where the camera follows glowing footprints to a hidden bug, smashes it with a toy hammer, and starts a new trail, behind a compact left-aligned hero card.

**Architecture:** The page layer stays HTML, CSS, and one small shim, now driving a single fixed canvas and no hover linking. The C simulation keeps its engine core (draw, font, rng, events, stage, build, tests) and replaces the side-view world with: trig tables, an ASCII tile map with BFS, procedural textures generated at init, a camera, a DDA raycaster (walls with a depth buffer, floor casting with footprint decals, a skyline strip), depth-tested sprites, dogs, wind-blown trash, a hidden bug, a HUD hammer, and a hunt state machine. Eight exports replace twelve.

**Tech Stack:** C11 (freestanding for wasm, libc only in tests), `zig cc` (Zig 0.16 installed) for wasm32, GNU Make, Node 24 smoke test, plain HTML/CSS/ES-module JavaScript, Chrome DevTools for browser checks.

**Spec:** `docs/superpowers/specs/2026-09-03-bug-smasher-profile-design.md` (revision 4). This plan covers sections 1 to 6 and 8 to 18; section 7 (settlement) stays phase 2.

## Global Constraints

- No libc, no libm in `sim/`: only `stdint.h`, `stddef.h`, `stdbool.h` from the compiler. `trig_sin`/`trig_cos`/`trig_atan2` and `fm_sqrt` replace math functions. Float arithmetic itself is fine.
- All exports are integers or pointers; marked `SIM_EXPORT("name")`. Hunt-stage exports, exactly eight: `sim_init(seed, w, h)`, `sim_update`, `sim_render`, `sim_framebuffer`, `sim_framebuffer_len`, `sim_poll_event`, `sim_render_static`, `memory`.
- `sim_init` accepts `w` in 200..640 and `h` in 160..320; framebuffer is `w * h * 4` bytes; the static buffer is `640 * 320 * 4`.
- Events: `(type << 24) | (stage << 16) | (a << 8) | b`; the hunt stage emits only `EV_BUG_SQUASHED` (type 3, stage 0).
- Geometry: wall height 2.5 tiles, eye height 0.5, field of view 85°, fog `1 / (1 + 0.35 * dist)`, walk speed 1.4 tiles/s, turn rate 2.5 rad/s, head bob amplitude 3 px, inspect dip 12 px over 0.6 s, celebrate hop 8 px over 0.5 s.
- Hunt timings: LOOK 1.5 s, inspect every 8 ± 3 s, APPROACH within 2.5 tiles (peek) and 1.5 tiles (exposed), SMASH 0.4 s with the hit at 0.25 s, CELEBRATE 1.0 s, new hiding spot at least 12 BFS steps away.
- Footprints: pairs every 0.5 tile, ±0.15 tile off the centre line, bright for the 8 pairs ahead, dim behind, culled more than 6 tiles behind.
- Map: 48 × 48, legend `# = W N S P D . ~ L T c n d @`, one connected floor component.
- Textures 64×64 walls and floor, 32×32 sprites (lamp 32×64), sky strip 1024×192, HUD 96×64 × 3 frames, all generated at init. Neon text defaults to `ROHIT`.
- Budgets: `sim.wasm` under 65536 bytes; `shim.js` under 2560 bytes gzipped; the page makes exactly four requests plus a `data:` favicon.
- Native tests build with `-fsanitize=address,undefined -Wall -Wextra` and pass with zero warnings. `make test` after every task.
- Commit messages: plain, **no `Co-Authored-By` trailer** (owner's preference for this repo).
- The `Stage` struct, `events`, `draw`, `font`, `rng`, `fmath.h`, `freestanding.c`, `export.h`, `Makefile`, and the test harness stay as they are unless a task says otherwise.

---

## File Structure

```
index.html                 compact left card, one fixed scene canvas          (rewrite)
styles.css                 card over canvas, mobile, no-sim                   (rewrite)
shim.js                    load, loop, blit, counter, static mode, fallback   (rewrite)
sim.wasm                   build output                                       (rebuild)
sim/
  export.h rng.* events.* draw.* font.* fmath.h stage.* freestanding.c        (keep)
  trig.h / trig.c          sin/cos table, atan2, wrap                          (new)
  palette.h / palette.c    night palette                                      (replace)
  map.h / map.c            Map, CITY_MAP, parse, light, BFS, hiding spots     (new)
  textures.h / textures.c  Textures struct and procedural generation          (new)
  camera.h / camera.c      Camera, steering, bob, pitch                       (new)
  decals.h / decals.c      footprint trail grid                               (new)
  sprites.h / sprites.c    Sprite list, projection, depth-tested draw         (replace)
  raycast.h / raycast.c    sky, walls, floor                                  (new)
  hud.h / hud.c            hammer overlay, +1 flash, scaled text              (new)
  actors/dog.h / dog.c     wandering dogs                                     (new)
  actors/trash.h / trash.c loose trash and wind                               (new)
  actors/bug.h / bug.c     hidden bug                                         (new)
  hunt.h / hunt.c          state machine                                      (new)
  world.h / world.c        World: everything above, step and render           (replace)
  sim.h / sim.c            exports                                            (replace)
test/
  test.h test_main.c test_harness.c test_rng.c test_events.c test_draw.c test_font.c test_stage.c (keep)
  test_trig.c test_map.c test_textures.c test_camera.c test_decals.c test_sprites.c
  test_raycast.c test_hud.c test_actors.c test_hunt.c test_sim.c              (new)
  wasm_smoke.mjs                                                              (rewrite)
```

Removed: `sim/bug.*`, `sim/hero.*`, old `sim/world.*`, old `sim/sprites.*`, `sim/scene.*`, `sim/vignette.h`, `sim/vignettes/`, old `sim/palette.*`, old `sim/sim.*`, and their tests.

Conventions: every header has an include guard named `<FILE>_H_HEADER` (this repo hit collisions between guards and constants before). Actor headers live in `sim/actors/` and are included as `"actors/dog.h"`; the Makefile passes `-Isim` and globs `sim/*.c sim/vignettes/*.c`, so **Task 1 adds `sim/actors/*.c` to the Makefile globs**.

---

### Task 1: Remove the side-panel world and keep the build green

**Files:**
- Delete: `sim/bug.h`, `sim/bug.c`, `sim/hero.h`, `sim/hero.c`, `sim/world.h`, `sim/world.c`, `sim/sprites.h`, `sim/sprites.c`, `sim/scene.h`, `sim/scene.c`, `sim/vignette.h`, `sim/vignettes/` (whole directory), `sim/palette.h`, `sim/palette.c`, `sim/sim.h`, `sim/sim.c`, `test/test_bug.c`, `test/test_hero.c`, `test/test_world.c`, `test/test_sprites.c`, `test/test_patrol.c`, `test/test_scene.c`, `test/test_crossover.c`, `test/test_race.c`, `test/test_detective.c`, `test/test_regression.c`, `test/test_sim.c`
- Modify: `test/test_main.c`, `test/test_stage.c`, `Makefile`

**Interfaces:**
- Produces: a tree where `make test` runs harness, rng, events, draw, font, stage and passes; `make sim.wasm` still links (a module with no exports, which `make smoke` rejects until Task 13 — expected).

- [ ] **Step 1: Delete the old modules and tests**

```bash
git rm -q sim/bug.h sim/bug.c sim/hero.h sim/hero.c sim/world.h sim/world.c sim/sprites.h sim/sprites.c \
  sim/scene.h sim/scene.c sim/vignette.h sim/palette.h sim/palette.c sim/sim.h sim/sim.c
git rm -rq sim/vignettes
git rm -q test/test_bug.c test/test_hero.c test/test_world.c test/test_sprites.c test/test_patrol.c \
  test/test_scene.c test/test_crossover.c test/test_race.c test/test_detective.c test/test_regression.c test/test_sim.c
```

- [ ] **Step 2: Trim `test/test_main.c`**

Replace the file with:

```c
#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);
void test_rng(void);
void test_events(void);
void test_draw(void);
void test_font(void);
void test_stage(void);

int main(void) {
    RUN(test_harness);
    RUN(test_rng);
    RUN(test_events);
    RUN(test_draw);
    RUN(test_font);
    RUN(test_stage);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
```

- [ ] **Step 3: Make `test/test_stage.c` independent of the palette**

In `test/test_stage.c`, remove `#include "palette.h"`, add after the includes:

```c
static const Color TEST_PAL[1] = { { 0, 0, 0, 255 } };
```

and replace every `PALETTE_BUGS` with `TEST_PAL` (the `stage_init` argument and the `CHECK(st.palette == ...)` line).

- [ ] **Step 4: Add the actors directory to the Makefile globs**

In `Makefile` change the two `SIM_*` lines to:

```make
SIM_SRC  := $(wildcard sim/*.c sim/vignettes/*.c sim/actors/*.c)
SIM_HDR  := $(wildcard sim/*.h sim/vignettes/*.h sim/actors/*.h)
```

- [ ] **Step 5: Run the tests**

Run: `make clean && make test 2>&1 | tail -2`
Expected: `... checks, 0 failures` with zero warnings (about 2240 checks: harness 3, rng, events, draw, font, stage).

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Remove the side-panel world, vignettes, and scene ahead of the alley hunt"
```

---

### Task 2: Trig tables and atan2 without libm

**Files:**
- Create: `sim/trig.h`, `sim/trig.c`, `test/test_trig.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `TRIG_PI`, `TRIG_TAU`, `TRIG_HALF_PI`; `void trig_init(void)` (idempotent, fills a 1024-entry sine table); `float trig_sin(float)`, `float trig_cos(float)` for any angle with linear interpolation (error under 1e-4); `float trig_atan2(float y, float x)` in `(-π, π]` (error under 0.005 rad); `float trig_wrap(float a)` into `(-π, π]`.

- [ ] **Step 1: Write the failing tests**

`test/test_trig.c`:

```c
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
```

Add `void test_trig(void);` and `RUN(test_trig);` to `test/test_main.c` after `test_stage`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `trig.h` not found.

- [ ] **Step 3: Implement**

`sim/trig.h`:

```c
#ifndef TRIG_H_HEADER
#define TRIG_H_HEADER

#define TRIG_PI 3.14159265f
#define TRIG_TAU 6.28318531f
#define TRIG_HALF_PI 1.57079633f
#define TRIG_TABLE 1024

void  trig_init(void);
float trig_sin(float a);
float trig_cos(float a);
float trig_atan2(float y, float x);   /* (-pi, pi] */
float trig_wrap(float a);             /* into (-pi, pi] */

#endif
```

`sim/trig.c`:

```c
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
    while (a > TRIG_PI) a -= TRIG_TAU;
    while (a <= -TRIG_PI) a += TRIG_TAU;
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
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`, zero warnings.

- [ ] **Step 5: Commit**

```bash
git add sim/trig.h sim/trig.c test/test_trig.c test/test_main.c
git commit -m "Add trig tables and atan2 approximation"
```

---

### Task 3: Night palette

**Files:**
- Create: `sim/palette.h`, `sim/palette.c`, `test/test_palette.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `PALETTE_NIGHT[COL_COUNT]` with the `COL_*` enum below. Everything that draws takes `const Color *pal` and indexes it with these names.

- [ ] **Step 1: Write the failing test**

`test/test_palette.c`:

```c
#include "test.h"
#include "palette.h"

void test_palette(void) {
    for (int i = 0; i < COL_COUNT; i++) CHECK_EQ(PALETTE_NIGHT[i].a, 255);
    CHECK(PALETTE_NIGHT[COL_SKY_TOP].b > PALETTE_NIGHT[COL_SKY_TOP].r);
    CHECK(PALETTE_NIGHT[COL_FOOTPRINT].r > 200 && PALETTE_NIGHT[COL_FOOTPRINT].g > 160 && PALETTE_NIGHT[COL_FOOTPRINT].b < 100);
    CHECK(PALETTE_NIGHT[COL_HAMMER].r > PALETTE_NIGHT[COL_HAMMER].b);
    CHECK(PALETTE_NIGHT[COL_BRICK].r > PALETTE_NIGHT[COL_BRICK].g);
    CHECK(PALETTE_NIGHT[COL_ASPHALT].r < 80);
    CHECK(COL_COUNT >= 30);
}
```

Add `void test_palette(void);` and `RUN(test_palette);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `palette.h` not found.

- [ ] **Step 3: Implement**

`sim/palette.h`:

```c
#ifndef PALETTE_H_HEADER
#define PALETTE_H_HEADER
#include "draw.h"

enum {
    COL_BG = 0, COL_SKY_TOP, COL_SKY_BOTTOM, COL_STAR, COL_TOWER, COL_TOWER_WINDOW,
    COL_BRICK, COL_BRICK_DARK, COL_MORTAR, COL_CONCRETE, COL_CONCRETE_DARK, COL_GRIME,
    COL_WINDOW_LIT, COL_WINDOW_LIT2, COL_WINDOW_DARK, COL_NEON, COL_NEON_GLOW, COL_SIGN,
    COL_POSTER, COL_POSTER_INK, COL_DUMPSTER, COL_DUMPSTER_DARK,
    COL_ASPHALT, COL_ASPHALT_DARK, COL_PUDDLE, COL_PUDDLE_LIGHT,
    COL_FOOTPRINT, COL_FOOTPRINT_DIM, COL_LAMP, COL_LAMP_LIGHT,
    COL_DOG, COL_DOG_DARK, COL_TRASH, COL_TRASH_DARK, COL_PAPER,
    COL_BUG, COL_BUG_DARK, COL_DUST, COL_HAND, COL_HAMMER, COL_HANDLE, COL_TEXT,
    COL_COUNT
};

extern const Color PALETTE_NIGHT[COL_COUNT];

#endif
```

`sim/palette.c`:

```c
#include "palette.h"

const Color PALETTE_NIGHT[COL_COUNT] = {
    [COL_BG]            = COLOR(10, 12, 24),
    [COL_SKY_TOP]       = COLOR(6, 8, 22),
    [COL_SKY_BOTTOM]    = COLOR(28, 30, 64),
    [COL_STAR]          = COLOR(200, 205, 230),
    [COL_TOWER]         = COLOR(16, 18, 34),
    [COL_TOWER_WINDOW]  = COLOR(240, 200, 110),
    [COL_BRICK]         = COLOR(120, 60, 52),
    [COL_BRICK_DARK]    = COLOR(96, 46, 42),
    [COL_MORTAR]        = COLOR(70, 60, 62),
    [COL_CONCRETE]      = COLOR(92, 94, 104),
    [COL_CONCRETE_DARK] = COLOR(70, 72, 82),
    [COL_GRIME]         = COLOR(44, 44, 52),
    [COL_WINDOW_LIT]    = COLOR(250, 210, 120),
    [COL_WINDOW_LIT2]   = COLOR(120, 210, 230),
    [COL_WINDOW_DARK]   = COLOR(30, 34, 52),
    [COL_NEON]          = COLOR(255, 80, 200),
    [COL_NEON_GLOW]     = COLOR(150, 40, 120),
    [COL_SIGN]          = COLOR(200, 60, 50),
    [COL_POSTER]        = COLOR(210, 200, 170),
    [COL_POSTER_INK]    = COLOR(60, 50, 70),
    [COL_DUMPSTER]      = COLOR(40, 100, 70),
    [COL_DUMPSTER_DARK] = COLOR(24, 64, 46),
    [COL_ASPHALT]       = COLOR(46, 48, 58),
    [COL_ASPHALT_DARK]  = COLOR(34, 36, 44),
    [COL_PUDDLE]        = COLOR(40, 52, 90),
    [COL_PUDDLE_LIGHT]  = COLOR(70, 90, 140),
    [COL_FOOTPRINT]     = COLOR(255, 220, 70),
    [COL_FOOTPRINT_DIM] = COLOR(140, 110, 40),
    [COL_LAMP]          = COLOR(60, 62, 70),
    [COL_LAMP_LIGHT]    = COLOR(255, 236, 170),
    [COL_DOG]           = COLOR(170, 120, 70),
    [COL_DOG_DARK]      = COLOR(90, 60, 40),
    [COL_TRASH]         = COLOR(110, 116, 128),
    [COL_TRASH_DARK]    = COLOR(60, 64, 74),
    [COL_PAPER]         = COLOR(220, 220, 210),
    [COL_BUG]           = COLOR(120, 200, 110),
    [COL_BUG_DARK]      = COLOR(30, 60, 30),
    [COL_DUST]          = COLOR(170, 170, 190),
    [COL_HAND]          = COLOR(240, 200, 170),
    [COL_HAMMER]        = COLOR(250, 200, 60),
    [COL_HANDLE]        = COLOR(120, 80, 60),
    [COL_TEXT]          = COLOR(235, 235, 245),
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/palette.h sim/palette.c test/test_palette.c test/test_main.c
git commit -m "Add the night palette"
```

---

### Task 4: The city map, parsing, light map, BFS, hiding spots

**Files:**
- Create: `sim/map.h`, `sim/map.c`, `test/test_map.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `MAP_W 48`, `MAP_H 48`, `MAP_MAX_PATH 512`, `MAP_MAX_SPAWNS 64`; enums `CELL_FLOOR, CELL_BRICK, CELL_CONCRETE, CELL_WINDOW, CELL_NEON, CELL_SIGN, CELL_POSTER, CELL_DUMPSTER, CELL_COUNT`; `FLOOR_ASPHALT, FLOOR_PUDDLE`; `PROP_NONE, PROP_LAMP, PROP_TRASH, PROP_PAPER, PROP_NEWSPAPER, PROP_DOG, PROP_START`; `Spawn {kind,x,y}`, `Tile {x,y}`, `Path {t[], n}`, `Map`; `CITY_MAP[MAP_H]`; `map_parse`, `map_is_floor`, `map_wall_kind`, `map_light`, `map_bfs`, `map_distances`, `map_is_connected`, `map_is_hiding_spot`, `map_pick_hiding_spot`.
- Every row of `CITY_MAP` is exactly 48 characters; `map_parse` rejects any other length. If the parser rejects a row while you transcribe, count that row's characters and fix the typo; do not change the parser.

- [ ] **Step 1: Write the failing tests**

`test/test_map.c`:

```c
#include "test.h"
#include "map.h"

static const char *const SMALL[8] = {
    "########",
    "#......#",
    "#.####.#",
    "#.#..#.#",
    "#.#..#.#",
    "#.####.#",
    "#......#",
    "########",
};

static const char *const BAD_CHAR[2] = { "##", "#x" };
static const char *const BAD_LEN[2]  = { "##", "###" };

void test_map(void) {
    static Map m;
    CHECK_EQ(map_parse(&m, CITY_MAP, MAP_W, MAP_H), 0);
    CHECK_EQ(m.w, 48); CHECK_EQ(m.h, 48);
    CHECK_EQ(m.start_x, 2); CHECK_EQ(m.start_y, 2);
    CHECK(map_is_floor(&m, 2, 2));
    CHECK(!map_is_floor(&m, 0, 0));
    CHECK(!map_is_floor(&m, -1, 5));
    CHECK(!map_is_floor(&m, 5, 48));
    CHECK_EQ(map_wall_kind(&m, 0, 0), CELL_BRICK);
    CHECK_EQ(map_wall_kind(&m, -3, 0), CELL_BRICK);
    CHECK_EQ(map_wall_kind(&m, 12, 15), CELL_NEON);
    CHECK_EQ(map_wall_kind(&m, 33, 18), CELL_SIGN);
    CHECK_EQ(map_wall_kind(&m, 21, 25), CELL_DUMPSTER);
    CHECK_EQ(map_wall_kind(&m, 30, 34), CELL_DUMPSTER);
    CHECK_EQ(map_wall_kind(&m, 3, 12), CELL_CONCRETE);
    CHECK_EQ(map_wall_kind(&m, 4, 3), CELL_WINDOW);
    CHECK_EQ(map_wall_kind(&m, 21, 4), CELL_POSTER);
    CHECK_EQ(m.floor_kind[1][9], FLOOR_PUDDLE);
    CHECK_EQ(m.floor_kind[2][2], FLOOR_ASPHALT);

    int dogs = 0, lamps = 0, trash = 0, paper = 0, news = 0;
    for (int i = 0; i < m.n_spawns; i++) {
        switch (m.spawns[i].kind) {
        case PROP_DOG: dogs++; break; case PROP_LAMP: lamps++; break; case PROP_TRASH: trash++; break;
        case PROP_PAPER: paper++; break; case PROP_NEWSPAPER: news++; break; default: break;
        }
    }
    CHECK_EQ(dogs, 3);
    CHECK(lamps >= 12);
    CHECK(trash >= 8);
    CHECK(paper >= 3);
    CHECK(news >= 3);
    CHECK_EQ(m.prop[6][1], PROP_DOG);
    CHECK_EQ(m.prop[1][3], PROP_LAMP);

    /* light: a lamp tile is fully lit, far floor is ambient */
    CHECK_NEAR(map_light(&m, 3, 1), 1.0f, 1e-4);
    CHECK_NEAR(map_light(&m, 2, 2), 0.25f + 0.75f * (1.0f - 1.41421f / 3.0f), 1e-3);   /* one lamp at distance sqrt(2) */
    CHECK_NEAR(map_light(&m, 24, 38), 0.25f, 1e-4);                                        /* no lamp within 3 */
    CHECK(map_is_connected(&m));

    /* hiding spots: floor next to a trash can or dumpster */
    CHECK(map_is_hiding_spot(&m, 20, 25));     /* west of the dumpster at (21,25) */
    CHECK(map_is_hiding_spot(&m, 17, 1));      /* next to the trash can at (18,1) */
    CHECK(!map_is_hiding_spot(&m, 18, 1));     /* the trash tile itself is not a spot */
    CHECK(!map_is_hiding_spot(&m, 2, 2));
    CHECK(!map_is_hiding_spot(&m, 21, 25));    /* a wall */

    Rng rng; rng_seed(&rng, 3u);
    Tile from = { 2, 2 }, spot;
    for (int i = 0; i < 20; i++) {
        CHECK(map_pick_hiding_spot(&m, &rng, from, 12, &spot));
        CHECK(map_is_hiding_spot(&m, spot.x, spot.y));
        Path p;
        CHECK(map_bfs(&m, from, spot, &p));
        CHECK(p.n - 1 >= 12);
        CHECK_EQ(p.t[0].x, 2); CHECK_EQ(p.t[0].y, 2);
        CHECK_EQ(p.t[p.n - 1].x, spot.x); CHECK_EQ(p.t[p.n - 1].y, spot.y);
        for (int k = 1; k < p.n; k++) {          /* consecutive tiles are 4-neighbours on floor */
            int dx = p.t[k].x - p.t[k - 1].x, dy = p.t[k].y - p.t[k - 1].y;
            CHECK((dx == 0 && (dy == 1 || dy == -1)) || (dy == 0 && (dx == 1 || dx == -1)));
            CHECK(map_is_floor(&m, p.t[k].x, p.t[k].y));
        }
    }

    /* small map: BFS detour length, unreachable pocket, connectivity false */
    static Map s;
    CHECK_EQ(map_parse(&s, SMALL, 8, 8), 0);
    Path p;
    CHECK(map_bfs(&s, (Tile){1, 1}, (Tile){6, 6}, &p));
    CHECK_EQ(p.n, 11);
    CHECK(!map_bfs(&s, (Tile){1, 1}, (Tile){3, 3}, &p));
    CHECK(!map_is_connected(&s));
    static int16_t dist[8 * 8];
    int reach = map_distances(&s, (Tile){1, 1}, dist);
    CHECK_EQ(dist[1 * 8 + 1], 0);
    CHECK_EQ(dist[6 * 8 + 6], 10);
    CHECK_EQ(dist[3 * 8 + 3], -1);
    CHECK_EQ(reach, 20);
    CHECK(!map_pick_hiding_spot(&s, &rng, (Tile){1, 1}, 1, &spot));   /* no T or D anywhere */

    CHECK_EQ(map_parse(&s, BAD_CHAR, 2, 2), -1);
    CHECK_EQ(map_parse(&s, BAD_LEN, 2, 2), -1);
    CHECK_EQ(map_parse(&s, SMALL, MAP_W + 1, 8), -1);
}
```

Add `void test_map(void);` and `RUN(test_map);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `map.h` not found.

- [ ] **Step 3: Implement**

`sim/map.h`:

```c
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
int   map_wall_kind(const Map *m, int x, int y);                  /* CELL_*; outside is CELL_BRICK */
float map_light(const Map *m, int x, int y);                      /* 0.25 outside */
int   map_bfs(const Map *m, Tile from, Tile to, Path *out);       /* 1 if found; path has both ends */
int   map_distances(const Map *m, Tile from, int16_t *dist);      /* dist[y*w+x]; -1 unreachable; returns reachable count */
int   map_is_connected(const Map *m);
int   map_is_hiding_spot(const Map *m, int x, int y);             /* floor, 4-adjacent to a trash can or dumpster */
int   map_pick_hiding_spot(const Map *m, Rng *rng, Tile from, int min_steps, Tile *out);

#endif
```

`sim/map.c`:

```c
#include "map.h"
#include "fmath.h"

/* 48 x 48. Legend: # brick, = concrete, W windows, N neon name sign, S generic sign, P poster,
 * D dumpster (wall), . asphalt, ~ puddle, L lamp, T trash can, c paper, n newspaper, d dog, @ start.
 * Alleys: full-width rows 1-2, 10-11, 19-20, 28-29, 37-38, 46; columns 1-2, 10-11, 19-20, 28-29, 37-38, 46.
 * Dead ends: column 46 rows 3-9, column 28-29 rows 12-15, column 10-11 rows 21-24, column 37-38 rows 30-33. */
const char *const CITY_MAP[MAP_H] = {
    "################################################",
    "#..L.....~........T.........L.........c......~.#",
    "#.@............................................#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#########",
    "#..#######L.=W===W=~.P#####W..=======..#W#######",
    "#..W#####W..=======..#######.TW=====W..#########",
    "#d.#######..W=====W..W######..=======n.#####W###",
    "#..#W###W#..=======..#######..=W===W=..#########",
    "#..#######..=W===W=c.######P..=======..W#####W##",
    "#..#W#W#W#..=======..#######..=W=W=W=..#########",
    "#.......L..........~..........T..........L.....#",
    "#..c...........................n...............#",
    "#..=======..#W#W#W#..=======#########..=W=W=W=.#",
    "#L.=W===W=..#######..=W===W=##P######..=======.#",
    "#..=======..W#####W..=======#########..W=====W.#",
    "#..=W===W=..N######..=======#########T.=======.#",
    "#..=======..#######..=W===W=..#######..=W===W=.#",
    "#..=W===W=~.#####W#..=======..#######..=======.#",
    "#..=======..#W#W#W#..=======..###S###..=W=W=W=.#",
    "#....T...........L...........~..........c......#",
    "#...................d..........................#",
    "#..#########=W=W=W=..#######..=======..#W#W#W#.#",
    "#..#W###W###=======..P#####W..=W===W=..#######.#",
    "#..#########W=====W~.#######..=======L.W#####W.#",
    "#..W#####W##=======..#######..=W===W=..#######.#",
    "#..#######..=W===W=..D######..=======..#####W#.#",
    "#.T#W###W#..=======..#######n.=W===W=..#######.#",
    "#..#######..=W=W=W=..#######..=======..#W#W#W#.#",
    "#.L.........~.........T.........L..........n...#",
    "#.........................................c....#",
    "#..=======..#######..=W=W=W=..#########=======.#",
    "#..=W===W=..#W###W#..=======..P#####W##=W===W=.#",
    "#~.=======..#######..W=====W..#########=======.#",
    "#..=W===W=L.W#####W..=======..#########W=====W.#",
    "#..=======..#######T.=W===W=..D######..=======.#",
    "#..=W===W=..#W###W#..=======..#######..=W===W=.#",
    "#..=======..#######..=W=W=W=..#######..=======.#",
    "#....~..........L..........T..........L.......d#",
    "#..............................................#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#######.#",
    "#..#######..=W===W=..P#####W..=======..#W#####.#",
    "#c.W#####W..=======L.#######..W=====W..#######.#",
    "#..#######..W=====W..W######..=======..#####W#.#",
    "#..#W###W#..=======..#######~.=W===W=..#######.#",
    "#..#######T.=W===W=..######P..=======..W#####W.#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#######.#",
    "#.L..........T..........L..........T..........L#",
    "################################################",
};

static int legend(char c, int *cell, int *floor_kind, int *prop) {
    *cell = CELL_FLOOR; *floor_kind = FLOOR_ASPHALT; *prop = PROP_NONE;
    switch (c) {
    case '#': *cell = CELL_BRICK; return 1;
    case '=': *cell = CELL_CONCRETE; return 1;
    case 'W': *cell = CELL_WINDOW; return 1;
    case 'N': *cell = CELL_NEON; return 1;
    case 'S': *cell = CELL_SIGN; return 1;
    case 'P': *cell = CELL_POSTER; return 1;
    case 'D': *cell = CELL_DUMPSTER; return 1;
    case '.': return 1;
    case '~': *floor_kind = FLOOR_PUDDLE; return 1;
    case 'L': *prop = PROP_LAMP; return 1;
    case 'T': *prop = PROP_TRASH; return 1;
    case 'c': *prop = PROP_PAPER; return 1;
    case 'n': *prop = PROP_NEWSPAPER; return 1;
    case 'd': *prop = PROP_DOG; return 1;
    case '@': *prop = PROP_START; return 1;
    default: return 0;
    }
}

static void compute_light(Map *m) {
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) m->light[y][x] = 0.25f;
    for (int i = 0; i < m->n_spawns; i++) {
        if (m->spawns[i].kind != PROP_LAMP) continue;
        int lx = m->spawns[i].x, ly = m->spawns[i].y;
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
            int x = lx + dx, y = ly + dy;
            if (x < 0 || y < 0 || x >= m->w || y >= m->h) continue;
            float d = fm_sqrt((float)(dx * dx + dy * dy));
            if (d >= 3.0f) continue;
            m->light[y][x] = fm_min(1.0f, m->light[y][x] + 0.75f * (1.0f - d / 3.0f));
        }
    }
}

int map_parse(Map *m, const char *const *rows, int w, int h) {
    if (w < 3 || h < 3 || w > MAP_W || h > MAP_H) return -1;
    m->w = w; m->h = h; m->n_spawns = 0; m->start_x = 1; m->start_y = 1;
    for (int y = 0; y < h; y++) {
        const char *row = rows[y];
        for (int x = 0; x < w; x++) {
            int cell, fk, prop;
            if (row[x] == '\0' || !legend(row[x], &cell, &fk, &prop)) return -1;
            m->cell[y][x] = (uint8_t)cell;
            m->floor_kind[y][x] = (uint8_t)fk;
            m->prop[y][x] = (uint8_t)prop;
            if (prop == PROP_START) { m->start_x = x; m->start_y = y; }
            else if (prop != PROP_NONE && m->n_spawns < MAP_MAX_SPAWNS) {
                m->spawns[m->n_spawns].kind = prop; m->spawns[m->n_spawns].x = x; m->spawns[m->n_spawns].y = y;
                m->n_spawns++;
            }
        }
        if (row[w] != '\0') return -1;          /* row longer than w */
    }
    compute_light(m);
    return 0;
}

int map_is_floor(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    return m->cell[y][x] == CELL_FLOOR;
}

int map_wall_kind(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return CELL_BRICK;
    return m->cell[y][x];
}

float map_light(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0.25f;
    return m->light[y][x];
}

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };

int map_distances(const Map *m, Tile from, int16_t *dist) {
    static int queue[MAP_W * MAP_H];
    int n = m->w * m->h, head = 0, tail = 0, reach = 0;
    for (int i = 0; i < n; i++) dist[i] = -1;
    if (!map_is_floor(m, from.x, from.y)) return 0;
    dist[from.y * m->w + from.x] = 0;
    queue[tail++] = from.y * m->w + from.x;
    while (head < tail) {
        int cur = queue[head++], cx = cur % m->w, cy = cur / m->w;
        reach++;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!map_is_floor(m, nx, ny)) continue;
            int ni = ny * m->w + nx;
            if (dist[ni] >= 0) continue;
            dist[ni] = (int16_t)(dist[cur] + 1);
            queue[tail++] = ni;
        }
    }
    return reach;
}

int map_bfs(const Map *m, Tile from, Tile to, Path *out) {
    static int16_t dist[MAP_W * MAP_H];
    out->n = 0;
    if (!map_is_floor(m, to.x, to.y)) return 0;
    map_distances(m, from, dist);
    int ti = to.y * m->w + to.x;
    if (dist[ti] < 0 || dist[ti] + 1 > MAP_MAX_PATH) return 0;
    int len = dist[ti] + 1;
    out->n = len;
    Tile cur = to;
    for (int k = len - 1; k >= 0; k--) {           /* walk downhill in distance back to from */
        out->t[k] = cur;
        if (k == 0) break;
        int cd = dist[cur.y * m->w + cur.x];
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + DX[d], ny = cur.y + DY[d];
            if (map_is_floor(m, nx, ny) && dist[ny * m->w + nx] == cd - 1) { cur.x = nx; cur.y = ny; break; }
        }
    }
    return 1;
}

int map_is_connected(const Map *m) {
    static int16_t dist[MAP_W * MAP_H];
    int floors = 0;
    Tile any = { -1, -1 };
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++)
        if (m->cell[y][x] == CELL_FLOOR) { floors++; if (any.x < 0) { any.x = x; any.y = y; } }
    if (floors == 0) return 1;
    return map_distances(m, any, dist) == floors;
}

int map_is_hiding_spot(const Map *m, int x, int y) {
    if (!map_is_floor(m, x, y) || m->prop[y][x] == PROP_TRASH) return 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], ny = y + DY[d];
        if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h) continue;
        if (m->cell[ny][nx] == CELL_DUMPSTER) return 1;
        if (m->cell[ny][nx] == CELL_FLOOR && m->prop[ny][nx] == PROP_TRASH) return 1;
    }
    return 0;
}

int map_pick_hiding_spot(const Map *m, Rng *rng, Tile from, int min_steps, Tile *out) {
    static int16_t dist[MAP_W * MAP_H];
    static Tile cands[MAP_W * MAP_H];
    int n = 0, best = -1, best_d = 0;
    map_distances(m, from, dist);
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int d = dist[y * m->w + x];
        if (d <= 0 || !map_is_hiding_spot(m, x, y)) continue;
        if (d >= min_steps) { cands[n].x = x; cands[n].y = y; n++; }
        if (d > best_d) { best_d = d; best = y * m->w + x; }
    }
    if (n > 0) { *out = cands[rng_range(rng, 0, n - 1)]; return 1; }
    if (best >= 0) { out->x = best % m->w; out->y = best / m->w; return 1; }
    return 0;
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`, zero warnings. If `map_parse` returns -1 on `CITY_MAP`, one row is not 48 characters: find it with `awk 'length != 48 {print NR": "length}'` on the rows and fix the transcription.

- [ ] **Step 5: Commit**

```bash
git add sim/map.h sim/map.c test/test_map.c test/test_main.c
git commit -m "Add the city tile map with parsing, light map, BFS, and hiding spots"
```

---

### Task 5: Full alphabet, scaled text, and procedural textures

**Files:**
- Modify: `sim/font.h`, `sim/font.c`, `test/test_font.c`
- Create: `sim/textures.h`, `sim/textures.c`, `test/test_textures.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces in font: the remaining capital letters `A B F G H I J P Q R S Y Z` (so any name renders), and `void draw_text_scaled(Framebuffer*, int x, int y, const char *s, int scale, Color c)` (each glyph pixel becomes a `scale × scale` block; advance is `4 * scale`).
- Produces in textures: constants `TEX_SIZE 64`, `SPR_W 32`, `SPR_H 64` (storage; real height in `sprite_h`), `SKY_W 1024`, `SKY_H 192`, `HUD_W 96`, `HUD_H 64`, `HUD_FRAMES 3`, `WINDOW_PANE_ALPHA 200`; enums `TEX_BRICK, TEX_CONCRETE, TEX_WINDOW, TEX_NEON, TEX_SIGN, TEX_POSTER, TEX_DUMPSTER, TEX_ASPHALT, TEX_PUDDLE, TEX_COUNT` and `SPR_DOG_A, SPR_DOG_B, SPR_DOG_SNIFF, SPR_TRASHCAN, SPR_PAPER_A, SPR_PAPER_B, SPR_NEWS_A, SPR_NEWS_B, SPR_BUG_HIDDEN, SPR_BUG_PEEK, SPR_BUG_EXPOSED, SPR_DUST_A, SPR_DUST_B, SPR_LAMP, SPR_COUNT`; `Textures` struct; `textures_generate(Textures*, Rng*, const Color *pal, const char *neon_text)`; `int texture_for_cell(int cell)`; `Color texture_wall(const Textures*, const Color *pal, int tex, uint32_t variant, int u, int v)` (resolves window panes to lit or dark by `variant` bits, always alpha 255); `Color texture_sprite(const Textures*, int spr, int u, int v)` (alpha 0 = transparent); `Color texture_sky(const Textures*, int x, int y)` (x wraps, y clamps); `uint32_t texture_hash(int x, int y)`.
- Textures are drawn with the existing `draw.h` primitives on a `Framebuffer` laid over the texture arrays (`Color` is four bytes, so `(uint8_t *)array` is a valid RGBA buffer).

- [ ] **Step 1: Write the failing tests**

Append to `test/test_font.c` inside `test_font`, before the closing brace:

```c
    /* full alphabet and scaled text */
    CHECK(font_glyph('R') != NULL); CHECK(font_glyph('H') != NULL); CHECK(font_glyph('I') != NULL);
    CHECK(font_glyph('A') != NULL); CHECK(font_glyph('Z') != NULL); CHECK(font_glyph('q') != NULL);
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text_scaled(&fb, 0, 0, "1", 2, COLOR(255, 255, 255));
    CHECK(lit(&fb, 2, 0)); CHECK(lit(&fb, 3, 0)); CHECK(lit(&fb, 2, 1)); CHECK(lit(&fb, 3, 1));
    CHECK(!lit(&fb, 0, 0)); CHECK(!lit(&fb, 4, 0));
    CHECK(lit(&fb, 0, 2)); CHECK(lit(&fb, 3, 3)); CHECK(!lit(&fb, 4, 2));   /* row 1 "## " scaled to rows 2-3, cols 0-3 (the test buffer is 8 rows tall) */
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text_scaled(&fb, 0, 0, "11", 2, COLOR(255, 255, 255));
    CHECK(lit(&fb, 10, 0));                                     /* second glyph starts at x = 8 */
```

`test/test_textures.c`:

```c
#include "test.h"
#include "textures.h"
#include "palette.h"
#include "map.h"

static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static int count_color(const Color *px, int n, Color c) {
    int k = 0; for (int i = 0; i < n; i++) if (same(px[i], c)) k++; return k;
}

static int distinct(const Color *px, int n) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int j = 0; j < i && j < 400; j++) if (same(px[i], px[j])) { seen = 1; break; }
        if (!seen) k++;
        if (k > 3) return k;
    }
    return k;
}

static uint32_t checksum(const Color *px, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h = (h ^ px[i].r) * 16777619u; h = (h ^ px[i].g) * 16777619u; h = (h ^ px[i].b) * 16777619u; h = (h ^ px[i].a) * 16777619u; }
    return h;
}

void test_textures(void) {
    static Textures t, t2;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 11u);
    textures_generate(&t, &rng, pal, "ROHIT");

    for (int k = 0; k < TEX_COUNT; k++) CHECK(distinct(t.wall[k], TEX_SIZE * TEX_SIZE) >= 2);
    CHECK(count_color(t.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE, pal[COL_MORTAR]) > 200);
    CHECK(count_color(t.wall[TEX_NEON], TEX_SIZE * TEX_SIZE, pal[COL_NEON]) > 40);
    CHECK(count_color(t.wall[TEX_NEON], TEX_SIZE * TEX_SIZE, pal[COL_NEON_GLOW]) > 40);
    CHECK(count_color(t.wall[TEX_SIGN], TEX_SIZE * TEX_SIZE, pal[COL_TEXT]) > 40);
    CHECK(count_color(t.wall[TEX_POSTER], TEX_SIZE * TEX_SIZE, pal[COL_POSTER]) > 800);
    CHECK(count_color(t.wall[TEX_DUMPSTER], TEX_SIZE * TEX_SIZE, pal[COL_DUMPSTER]) > 1500);
    CHECK(count_color(t.wall[TEX_ASPHALT], TEX_SIZE * TEX_SIZE, pal[COL_ASPHALT_DARK]) > 60);
    CHECK(count_color(t.wall[TEX_PUDDLE], TEX_SIZE * TEX_SIZE, pal[COL_PUDDLE_LIGHT]) > 100);

    /* window panes carry the marker alpha and resolve per variant */
    int panes = 0;
    for (int i = 0; i < TEX_SIZE * TEX_SIZE; i++) if (t.wall[TEX_WINDOW][i].a == WINDOW_PANE_ALPHA) panes++;
    CHECK(panes > 1000);
    int diff = 0, opaque = 1;
    for (int v = 0; v < TEX_SIZE; v++) for (int u = 0; u < TEX_SIZE; u++) {
        Color a = texture_wall(&t, pal, TEX_WINDOW, 0xFFFFFFFFu, u, v);
        Color b = texture_wall(&t, pal, TEX_WINDOW, 0u, u, v);
        if (a.a != 255 || b.a != 255) opaque = 0;
        if (!same(a, b)) diff++;
    }
    CHECK(opaque);
    CHECK(diff > 500);
    CHECK(same(texture_wall(&t, pal, TEX_BRICK, 0u, 3, 3), t.wall[TEX_BRICK][3 * TEX_SIZE + 3]));
    CHECK_EQ(texture_for_cell(CELL_BRICK), TEX_BRICK);
    CHECK_EQ(texture_for_cell(CELL_NEON), TEX_NEON);
    CHECK_EQ(texture_for_cell(CELL_DUMPSTER), TEX_DUMPSTER);
    CHECK(texture_hash(3, 4) != texture_hash(4, 3));
    CHECK(texture_hash(3, 4) == texture_hash(3, 4));

    /* sprites: transparent corners, real content, heights */
    for (int k = 0; k < SPR_COUNT; k++) {
        CHECK(t.sprite_h[k] == 32 || t.sprite_h[k] == 64);
        CHECK_EQ(texture_sprite(&t, k, 0, 0).a, 0);
        int solid = 0;
        for (int v = 0; v < t.sprite_h[k]; v++) for (int u = 0; u < SPR_W; u++) if (texture_sprite(&t, k, u, v).a == 255) solid++;
        CHECK(solid > 20);
    }
    CHECK_EQ(t.sprite_h[SPR_LAMP], 64);
    CHECK_EQ(t.sprite_h[SPR_DOG_A], 32);
    CHECK(count_color(t.sprite[SPR_DOG_A], SPR_W * SPR_H, pal[COL_DOG]) > 100);
    CHECK(count_color(t.sprite[SPR_BUG_EXPOSED], SPR_W * SPR_H, pal[COL_BUG]) > 60);
    CHECK(count_color(t.sprite[SPR_BUG_HIDDEN], SPR_W * SPR_H, pal[COL_BUG]) < count_color(t.sprite[SPR_BUG_PEEK], SPR_W * SPR_H, pal[COL_BUG]));
    CHECK(checksum(t.sprite[SPR_DOG_A], SPR_W * SPR_H) != checksum(t.sprite[SPR_DOG_B], SPR_W * SPR_H));
    CHECK(count_color(t.sprite[SPR_LAMP], SPR_W * SPR_H, pal[COL_LAMP_LIGHT]) > 20);

    /* sky: gradient at the top, towers with windows at the bottom, wrapping x */
    CHECK(count_color(t.sky, SKY_W, pal[COL_SKY_TOP]) > 900);                /* top row is gradient start, bar a few stars */
    CHECK(count_color(&t.sky[(SKY_H - 1) * SKY_W], SKY_W, pal[COL_TOWER]) > 600);
    CHECK(count_color(t.sky, SKY_W * SKY_H, pal[COL_TOWER_WINDOW]) > 300);
    CHECK(count_color(t.sky, SKY_W * SKY_H, pal[COL_STAR]) >= 40);
    CHECK(same(texture_sky(&t, SKY_W + 5, 100), texture_sky(&t, 5, 100)));
    CHECK(same(texture_sky(&t, -3, 100), texture_sky(&t, SKY_W - 3, 100)));
    CHECK(same(texture_sky(&t, 7, -10), texture_sky(&t, 7, 0)));
    CHECK(same(texture_sky(&t, 7, SKY_H + 10), texture_sky(&t, 7, SKY_H - 1)));

    /* hud frames exist and differ; the hammer head is yellow */
    for (int f = 0; f < HUD_FRAMES; f++) CHECK(count_color(t.hud[f], HUD_W * HUD_H, pal[COL_HAMMER]) > 200);
    CHECK(checksum(t.hud[0], HUD_W * HUD_H) != checksum(t.hud[1], HUD_W * HUD_H));
    CHECK(checksum(t.hud[1], HUD_W * HUD_H) != checksum(t.hud[2], HUD_W * HUD_H));
    CHECK_EQ(t.hud[0][0].a, 0);

    /* deterministic for a seed */
    rng_seed(&rng, 11u);
    textures_generate(&t2, &rng, pal, "ROHIT");
    CHECK(checksum(t.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE) == checksum(t2.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE));
    CHECK(checksum(t.sky, SKY_W * SKY_H) == checksum(t2.sky, SKY_W * SKY_H));
    rng_seed(&rng, 12u);
    textures_generate(&t2, &rng, pal, "ROHIT");
    CHECK(checksum(t.sky, SKY_W * SKY_H) != checksum(t2.sky, SKY_W * SKY_H));
}
```

Add `void test_textures(void);` and `RUN(test_textures);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `draw_text_scaled` undeclared / `textures.h` not found.

- [ ] **Step 3: Extend the font**

In `sim/font.h` add after `draw_text`:

```c
void draw_text_scaled(Framebuffer *fb, int x, int y, const char *s, int scale, Color c);   /* advance 4*scale */
```

In `sim/font.c` add these entries to `GLYPHS` (anywhere inside the array):

```c
    { 'A', "###" "# #" "###" "# #" "# #" },
    { 'B', "## " "# #" "## " "# #" "## " },
    { 'F', "###" "#  " "###" "#  " "#  " },
    { 'G', "###" "#  " "# #" "# #" "###" },
    { 'H', "# #" "# #" "###" "# #" "# #" },
    { 'I', "###" " # " " # " " # " "###" },
    { 'J', "  #" "  #" "  #" "# #" "###" },
    { 'P', "###" "# #" "###" "#  " "#  " },
    { 'Q', "###" "# #" "# #" "###" "  #" },
    { 'R', "###" "# #" "## " "# #" "# #" },
    { 'S', "###" "#  " "###" "  #" "###" },
    { 'Y', "# #" "# #" "###" " # " " # " },
    { 'Z', "###" "  #" " # " "#  " "###" },
```

and append the function:

```c
void draw_text_scaled(Framebuffer *fb, int x, int y, const char *s, int scale, Color c) {
    if (scale < 1) scale = 1;
    for (; *s; s++, x += FONT_ADVANCE * scale) {
        const char *g = font_glyph(*s);
        if (!g) g = BOX;
        for (int row = 0; row < FONT_H; row++)
            for (int col = 0; col < FONT_W; col++)
                if (g[row * FONT_W + col] == '#') draw_rect(fb, x + col * scale, y + row * scale, scale, scale, c);
    }
}
```

- [ ] **Step 4: Implement the textures**

`sim/textures.h`:

```c
#ifndef TEXTURES_H_HEADER
#define TEXTURES_H_HEADER
#include <stdint.h>
#include "draw.h"
#include "rng.h"

#define TEX_SIZE 64
#define SPR_W 32
#define SPR_H 64
#define SKY_W 1024
#define SKY_H 192
#define HUD_W 96
#define HUD_H 64
#define HUD_FRAMES 3
#define WINDOW_PANE_ALPHA 200

enum { TEX_BRICK = 0, TEX_CONCRETE, TEX_WINDOW, TEX_NEON, TEX_SIGN, TEX_POSTER, TEX_DUMPSTER, TEX_ASPHALT, TEX_PUDDLE, TEX_COUNT };
enum { SPR_DOG_A = 0, SPR_DOG_B, SPR_DOG_SNIFF, SPR_TRASHCAN, SPR_PAPER_A, SPR_PAPER_B, SPR_NEWS_A, SPR_NEWS_B,
       SPR_BUG_HIDDEN, SPR_BUG_PEEK, SPR_BUG_EXPOSED, SPR_DUST_A, SPR_DUST_B, SPR_LAMP, SPR_COUNT };

typedef struct {
    Color wall[TEX_COUNT][TEX_SIZE * TEX_SIZE];
    Color sprite[SPR_COUNT][SPR_W * SPR_H];
    int   sprite_h[SPR_COUNT];
    Color sky[SKY_H * SKY_W];
    Color hud[HUD_FRAMES][HUD_W * HUD_H];
} Textures;

void     textures_generate(Textures *t, Rng *rng, const Color *pal, const char *neon_text);
int      texture_for_cell(int cell);                                                       /* CELL_* -> TEX_* */
Color    texture_wall(const Textures *t, const Color *pal, int tex, uint32_t variant, int u, int v);
Color    texture_sprite(const Textures *t, int spr, int u, int v);
Color    texture_sky(const Textures *t, int x, int y);
uint32_t texture_hash(int x, int y);

#endif
```

`sim/textures.c`:

```c
#include "textures.h"
#include "palette.h"
#include "font.h"
#include "map.h"

static const Color CLEAR = { 0, 0, 0, 0 };

static void begin(Framebuffer *fb, Color *px, int w, int h, Color fill) {
    fb_init(fb, (uint8_t *)px, w, h);
    draw_clear(fb, fill);
}

/* ---- walls ---------------------------------------------------------------- */

static void gen_brick(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_BRICK]);
    for (int r = 0; r < 4; r++) {
        int y = r * 16, off = (r & 1) ? 16 : 0;
        for (int c = -1; c < 3; c++) {
            int x = c * 32 + off;
            if (rng_float(rng) < 0.3f) draw_rect(&fb, x, y, 32, 16, pal[COL_BRICK_DARK]);
            draw_rect(&fb, x, y, 2, 16, pal[COL_MORTAR]);
        }
        draw_rect(&fb, 0, y, TEX_SIZE, 2, pal[COL_MORTAR]);
    }
    draw_dim(&fb, 0, 52, TEX_SIZE, 12, 90);
}

static void gen_concrete(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE]);
    draw_rect(&fb, 0, 21, TEX_SIZE, 3, pal[COL_CONCRETE_DARK]);
    draw_rect(&fb, 0, 42, TEX_SIZE, 3, pal[COL_CONCRETE_DARK]);
    for (int i = 0; i < 6; i++) {
        int x = rng_range(rng, 0, TEX_SIZE - 1), y = rng_range(rng, 10, 50);
        draw_rect(&fb, x, y, 1, TEX_SIZE - y, pal[COL_GRIME]);
    }
    draw_dim(&fb, 0, 54, TEX_SIZE, 10, 70);
}

static void gen_window(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE_DARK]);
    Color pane = pal[COL_WINDOW_DARK]; pane.a = WINDOW_PANE_ALPHA;
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
        int x = 4 + i * 15, y = 4 + j * 15;
        draw_rect(&fb, x - 1, y - 1, 14, 14, pal[COL_MORTAR]);
        draw_rect(&fb, x, y, 12, 12, pane);
    }
}

static void gen_neon(Color *px, const Color *pal, const char *text) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE_DARK]);
    int n = 0; while (text[n] && n < 8) n++;
    int width = n ? (n * 4 - 1) * 3 : 0;
    int scale = 3;
    if (width > 60) { scale = 2; width = (n * 4 - 1) * 2; }
    int x0 = (TEX_SIZE - width) / 2, y0 = (TEX_SIZE - 5 * scale) / 2;
    draw_rect(&fb, 2, y0 - 6, TEX_SIZE - 4, 5 * scale + 12, pal[COL_GRIME]);
    draw_text_scaled(&fb, x0 - 1, y0, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0 + 1, y0, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0 - 1, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0 + 1, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0, text, scale, pal[COL_NEON]);
}

static void gen_sign(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_SIGN]);
    draw_rect(&fb, 0, 0, TEX_SIZE, 2, pal[COL_TEXT]); draw_rect(&fb, 0, TEX_SIZE - 2, TEX_SIZE, 2, pal[COL_TEXT]);
    draw_rect(&fb, 0, 0, 2, TEX_SIZE, pal[COL_TEXT]); draw_rect(&fb, TEX_SIZE - 2, 0, 2, TEX_SIZE, pal[COL_TEXT]);
    draw_text_scaled(&fb, (TEX_SIZE - 15 * 3) / 2, 24, "OPEN", 3, pal[COL_TEXT]);
}

static void gen_poster(Color *px, Rng *rng, const Color *pal) {
    gen_brick(px, rng, pal);
    Framebuffer fb; fb_init(&fb, (uint8_t *)px, TEX_SIZE, TEX_SIZE);
    draw_rect(&fb, 12, 6, 40, 52, pal[COL_POSTER]);
    draw_rect(&fb, 16, 10, 32, 6, pal[COL_POSTER_INK]);
    for (int y = 22; y <= 46; y += 8) draw_rect(&fb, 16, y, 28, 2, pal[COL_POSTER_INK]);
    draw_rect(&fb, 44, 50, 8, 8, pal[COL_BRICK]);
}

static void gen_dumpster(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_DUMPSTER]);
    draw_rect(&fb, 0, 0, TEX_SIZE, 10, pal[COL_DUMPSTER_DARK]);
    for (int x = 16; x < TEX_SIZE; x += 16) draw_rect(&fb, x, 10, 2, 46, pal[COL_DUMPSTER_DARK]);
    draw_rect(&fb, 8, 56, 8, 8, pal[COL_GRIME]); draw_rect(&fb, 48, 56, 8, 8, pal[COL_GRIME]);
}

static void gen_asphalt(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_ASPHALT]);
    for (int i = 0; i < 120; i++) draw_pixel(&fb, rng_range(rng, 0, 63), rng_range(rng, 0, 63), pal[COL_ASPHALT_DARK]);
    int y = rng_range(rng, 20, 44);
    for (int x = 0; x < TEX_SIZE; x++) { draw_pixel(&fb, x, y, pal[COL_ASPHALT_DARK]); y += rng_range(rng, -1, 1); if (y < 0) y = 0; if (y > 63) y = 63; }
}

static void gen_puddle(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_PUDDLE]);
    draw_rect(&fb, 18, 28, 28, 8, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 22, 24, 20, 4, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 22, 36, 20, 4, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 8, 12, 10, 4, pal[COL_PUDDLE_LIGHT]);
}

/* ---- sprites (bottom-anchored, drawn in a 32-wide, h-tall frame) ------------- */

static void gen_dog(Color *px, const Color *pal, int frame) {          /* 0 A, 1 B, 2 sniff */
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    int hy = frame == 2 ? 18 : 12;
    draw_rect(&fb, 8, 16, 16, 8, pal[COL_DOG]);                          /* body */
    draw_rect(&fb, 22, hy, 8, 7, pal[COL_DOG]);                          /* head */
    draw_rect(&fb, 28, hy - 2, 3, 4, pal[COL_DOG_DARK]);                 /* ear */
    draw_pixel(&fb, 27, hy + 2, pal[COL_BUG_DARK]);                      /* eye */
    draw_rect(&fb, 4, frame == 1 ? 12 : 14, 4, 3, pal[COL_DOG_DARK]);    /* tail wags */
    int lx0 = frame == 1 ? 10 : 9, lx1 = frame == 1 ? 12 : 13;
    draw_rect(&fb, lx0, 24, 3, 8, pal[COL_DOG_DARK]); draw_rect(&fb, lx1 + 4, 24, 3, 8, pal[COL_DOG_DARK]);
    draw_rect(&fb, lx0 + 9, 24, 3, 8, pal[COL_DOG_DARK]); draw_rect(&fb, lx1 + 13, 24, 3, 8, pal[COL_DOG_DARK]);
}

static void gen_trashcan(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    draw_rect(&fb, 10, 10, 12, 22, pal[COL_TRASH]);
    draw_rect(&fb, 8, 8, 16, 3, pal[COL_TRASH_DARK]);
    draw_rect(&fb, 10, 16, 12, 1, pal[COL_TRASH_DARK]); draw_rect(&fb, 10, 24, 12, 1, pal[COL_TRASH_DARK]);
}

static void gen_paper(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (frame == 0) { draw_rect(&fb, 12, 26, 8, 6, pal[COL_PAPER]); draw_rect(&fb, 14, 24, 4, 2, pal[COL_PAPER]); }
    else            { draw_rect(&fb, 13, 24, 6, 8, pal[COL_PAPER]); draw_rect(&fb, 11, 27, 2, 3, pal[COL_PAPER]); }
}

static void gen_news(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (frame == 0) { draw_rect(&fb, 8, 26, 16, 6, pal[COL_PAPER]); draw_rect(&fb, 10, 28, 12, 1, pal[COL_POSTER_INK]); }
    else            { draw_rect(&fb, 10, 23, 12, 9, pal[COL_PAPER]); draw_rect(&fb, 12, 25, 8, 1, pal[COL_POSTER_INK]); draw_rect(&fb, 12, 28, 8, 1, pal[COL_POSTER_INK]); }
}

static void gen_bug(Color *px, const Color *pal, int state) {          /* 0 hidden, 1 peek, 2 exposed */
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (state == 0) { draw_rect(&fb, 12, 29, 8, 3, pal[COL_BUG]); return; }
    int top = state == 1 ? 25 : 20, h = state == 1 ? 7 : 12;
    draw_rect(&fb, 10, top, 12, h, pal[COL_BUG]);
    draw_pixel(&fb, 10, top, CLEAR); draw_pixel(&fb, 21, top, CLEAR);
    draw_pixel(&fb, 12, top + 2, pal[COL_BUG_DARK]); draw_pixel(&fb, 19, top + 2, pal[COL_BUG_DARK]);
    draw_pixel(&fb, 12, top - 2, pal[COL_BUG]); draw_pixel(&fb, 11, top - 3, pal[COL_BUG]);
    draw_pixel(&fb, 19, top - 2, pal[COL_BUG]); draw_pixel(&fb, 20, top - 3, pal[COL_BUG]);
    if (state == 2) { draw_pixel(&fb, 11, 31, pal[COL_BUG_DARK]); draw_pixel(&fb, 15, 31, pal[COL_BUG_DARK]); draw_pixel(&fb, 19, 31, pal[COL_BUG_DARK]); }
}

static void gen_dust(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    int r = frame == 0 ? 6 : 10, cx = 16, cy = 22;
    draw_rect(&fb, cx - r, cy - 2, 2 * r, 4, pal[COL_DUST]);
    draw_rect(&fb, cx - 2, cy - r, 4, 2 * r, pal[COL_DUST]);
    draw_rect(&fb, cx - r + 2, cy - r + 2, 3, 3, pal[COL_DUST]); draw_rect(&fb, cx + r - 5, cy - r + 2, 3, 3, pal[COL_DUST]);
    draw_rect(&fb, cx - r + 2, cy + r - 5, 3, 3, pal[COL_DUST]); draw_rect(&fb, cx + r - 5, cy + r - 5, 3, 3, pal[COL_DUST]);
    draw_rect(&fb, cx - 2, cy - 2, 4, 4, CLEAR);
}

static void gen_lamp(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    draw_rect(&fb, 14, 12, 4, 52, pal[COL_LAMP]);
    draw_rect(&fb, 8, 6, 16, 6, pal[COL_LAMP]);
    draw_rect(&fb, 10, 12, 12, 3, pal[COL_LAMP_LIGHT]);
    draw_rect(&fb, 10, 60, 12, 4, pal[COL_LAMP]);
}

/* ---- HUD ------------------------------------------------------------------ */

static void gen_hud(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, HUD_W, HUD_H, CLEAR);
    int hx = 36, hy = 44, handle_x = 46, handle_y = 14, head_x = 34, head_y = 4;
    if (frame == 1) { hy = 40; handle_y = 4; head_x = 26; head_y = -4; }
    if (frame == 2) { hx = 44; hy = 46; handle_x = 52; handle_y = 32; head_x = 40; head_y = 20; }
    draw_rect(&fb, handle_x, handle_y, 6, hy - handle_y + 4, pal[COL_HANDLE]);
    draw_rect(&fb, head_x, head_y, 30, 14, pal[COL_HAMMER]);
    draw_rect(&fb, head_x, head_y + 6, 30, 2, pal[COL_HANDLE]);
    draw_rect(&fb, hx, hy, 24, HUD_H - hy, pal[COL_HAND]);
    draw_rect(&fb, hx + 4, hy + 8, 16, 1, pal[COL_HANDLE]);
}

/* ---- sky ------------------------------------------------------------------ */

static void gen_sky(Color *sky, Rng *rng, const Color *pal) {
    Framebuffer fb; fb_init(&fb, (uint8_t *)sky, SKY_W, SKY_H);
    for (int y = 0; y < SKY_H; y++) {
        int t = y * 255 / (SKY_H - 1);
        Color c = COLOR((pal[COL_SKY_TOP].r * (255 - t) + pal[COL_SKY_BOTTOM].r * t) / 255,
                        (pal[COL_SKY_TOP].g * (255 - t) + pal[COL_SKY_BOTTOM].g * t) / 255,
                        (pal[COL_SKY_TOP].b * (255 - t) + pal[COL_SKY_BOTTOM].b * t) / 255);
        draw_rect(&fb, 0, y, SKY_W, 1, c);
    }
    for (int i = 0; i < 60; i++) draw_pixel(&fb, rng_range(rng, 0, SKY_W - 1), rng_range(rng, 0, 40), pal[COL_STAR]);   /* above the tallest tower */
    int x = 0;
    while (x < SKY_W) {
        int w = rng_range(rng, 24, 64), hgt = rng_range(rng, 40, 150);
        if (x + w > SKY_W) w = SKY_W - x;
        draw_rect(&fb, x, SKY_H - hgt, w, hgt, pal[COL_TOWER]);
        for (int wy = SKY_H - hgt + 4; wy < SKY_H - 3; wy += 5)
            for (int wx = x + 3; wx < x + w - 4; wx += 5)
                if (rng_float(rng) < 0.28f) draw_rect(&fb, wx, wy, 2, 2, pal[COL_TOWER_WINDOW]);
        x += w + rng_range(rng, 0, 6);
    }
}

/* ---- public --------------------------------------------------------------- */

void textures_generate(Textures *t, Rng *rng, const Color *pal, const char *neon_text) {
    gen_brick(t->wall[TEX_BRICK], rng, pal);
    gen_concrete(t->wall[TEX_CONCRETE], rng, pal);
    gen_window(t->wall[TEX_WINDOW], pal);
    gen_neon(t->wall[TEX_NEON], pal, neon_text ? neon_text : "ROHIT");
    gen_sign(t->wall[TEX_SIGN], pal);
    gen_poster(t->wall[TEX_POSTER], rng, pal);
    gen_dumpster(t->wall[TEX_DUMPSTER], pal);
    gen_asphalt(t->wall[TEX_ASPHALT], rng, pal);
    gen_puddle(t->wall[TEX_PUDDLE], pal);

    for (int k = 0; k < SPR_COUNT; k++) t->sprite_h[k] = 32;
    gen_dog(t->sprite[SPR_DOG_A], pal, 0); gen_dog(t->sprite[SPR_DOG_B], pal, 1); gen_dog(t->sprite[SPR_DOG_SNIFF], pal, 2);
    gen_trashcan(t->sprite[SPR_TRASHCAN], pal);
    gen_paper(t->sprite[SPR_PAPER_A], pal, 0); gen_paper(t->sprite[SPR_PAPER_B], pal, 1);
    gen_news(t->sprite[SPR_NEWS_A], pal, 0); gen_news(t->sprite[SPR_NEWS_B], pal, 1);
    gen_bug(t->sprite[SPR_BUG_HIDDEN], pal, 0); gen_bug(t->sprite[SPR_BUG_PEEK], pal, 1); gen_bug(t->sprite[SPR_BUG_EXPOSED], pal, 2);
    gen_dust(t->sprite[SPR_DUST_A], pal, 0); gen_dust(t->sprite[SPR_DUST_B], pal, 1);
    gen_lamp(t->sprite[SPR_LAMP], pal); t->sprite_h[SPR_LAMP] = 64;

    for (int f = 0; f < HUD_FRAMES; f++) gen_hud(t->hud[f], pal, f);
    gen_sky(t->sky, rng, pal);
}

int texture_for_cell(int cell) {
    switch (cell) {
    case CELL_CONCRETE: return TEX_CONCRETE;
    case CELL_WINDOW:   return TEX_WINDOW;
    case CELL_NEON:     return TEX_NEON;
    case CELL_SIGN:     return TEX_SIGN;
    case CELL_POSTER:   return TEX_POSTER;
    case CELL_DUMPSTER: return TEX_DUMPSTER;
    default:            return TEX_BRICK;
    }
}

Color texture_wall(const Textures *t, const Color *pal, int tex, uint32_t variant, int u, int v) {
    Color c = t->wall[tex][v * TEX_SIZE + u];
    if (tex == TEX_WINDOW && c.a == WINDOW_PANE_ALPHA) {
        int i = (u - 4) / 15, j = (v - 4) / 15;
        int bit = (j & 3) * 4 + (i & 3);
        int lit = (variant >> bit) & 1u, warm = (variant >> (bit + 16)) & 1u;
        return lit ? pal[warm ? COL_WINDOW_LIT : COL_WINDOW_LIT2] : pal[COL_WINDOW_DARK];
    }
    c.a = 255;
    return c;
}

Color texture_sprite(const Textures *t, int spr, int u, int v) {
    if (u < 0 || v < 0 || u >= SPR_W || v >= t->sprite_h[spr]) return CLEAR;
    return t->sprite[spr][v * SPR_W + u];
}

Color texture_sky(const Textures *t, int x, int y) {
    x %= SKY_W; if (x < 0) x += SKY_W;
    if (y < 0) y = 0; if (y >= SKY_H) y = SKY_H - 1;
    return t->sky[y * SKY_W + x];
}

uint32_t texture_hash(int x, int y) {
    uint32_t h = (uint32_t)x * 0x9E3779B1u ^ ((uint32_t)y + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
```

- [ ] **Step 5: Run to verify pass**

Run: `make test` → `0 failures`, zero warnings. If a count assertion misses by a little, adjust the drawing (more mortar, larger poster) rather than the test; the counts describe the look the renderer assumes.

- [ ] **Step 6: Commit**

```bash
git add sim/font.h sim/font.c test/test_font.c sim/textures.h sim/textures.c test/test_textures.c test/test_main.c
git commit -m "Add the full alphabet, scaled text, and procedural textures"
```

---

### Task 6: Camera

**Files:**
- Create: `sim/camera.h`, `sim/camera.c`, `test/test_camera.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `CAM_WALK_SPEED 1.4f`, `CAM_TURN_RATE 2.5f`, `CAM_FOV_DEG 85.0f`, `CAM_BOB_PX 3`, `CAM_STEP_HZ 1.6f`; `Camera {x, y, angle, target_x, target_y, walking, auto_face, target_angle, turning, bob_phase, pitch_px, speed}`; `camera_init(cam, x, y, angle)`, `camera_walk_to(cam, x, y)` (sets walking and auto_face), `camera_stop`, `camera_face(cam, x, y)` (turn toward a point, clears auto_face), `camera_turn_to(cam, angle)`, `camera_arrived` (not walking), `camera_facing(cam, tol)`, `camera_dist(cam, x, y)`, `camera_step(cam, dt)`, `camera_bob_px(cam)` (0 when standing, within ±3 when walking), `camera_fov()` in radians, `camera_proj(w)` = `(w/2) / tan(fov/2)`, the projection constant every renderer module uses.
- Convention: map x grows right, y grows down; angle 0 faces +x, +π/2 faces +y. The "right" vector for angle a is `(-sin a, cos a)`.

- [ ] **Step 1: Write the failing tests**

`test/test_camera.c`:

```c
#include "test.h"
#include "camera.h"
#include "trig.h"

#define DT (1.0f / 60.0f)

void test_camera(void) {
    Camera c;
    camera_init(&c, 2.5f, 2.5f, 0.0f);
    CHECK(camera_arrived(&c));
    CHECK_EQ(camera_bob_px(&c), 0);
    CHECK_NEAR(camera_fov(), 85.0f * TRIG_PI / 180.0f, 1e-5);
    CHECK_NEAR(camera_proj(320), 160.0f / 0.91633f, 0.5);
    CHECK_NEAR(camera_proj(640), 2.0f * camera_proj(320), 0.01);
    CHECK_NEAR(camera_dist(&c, 5.5f, 6.5f), 5.0f, 1e-3);

    /* walk 1.4 tiles along +x in one second, auto-facing +x */
    camera_walk_to(&c, 3.9f, 2.5f);
    CHECK(!camera_arrived(&c));
    int bob_seen = 0;
    for (int i = 0; i < 59; i++) { camera_step(&c, DT); int b = camera_bob_px(&c); CHECK(b >= -3 && b <= 3); if (b != 0) bob_seen = 1; }
    CHECK(!camera_arrived(&c));
    CHECK(bob_seen);
    for (int i = 0; i < 3; i++) camera_step(&c, DT);
    CHECK(camera_arrived(&c));
    CHECK_NEAR(c.x, 3.9f, 0.01); CHECK_NEAR(c.y, 2.5f, 1e-4);
    CHECK_EQ(camera_bob_px(&c), 0);

    /* walking toward +y turns the view to +pi/2 at 2.5 rad/s: about 0.63 s */
    camera_walk_to(&c, 3.9f, 5.0f);
    for (int i = 0; i < 30; i++) camera_step(&c, DT);       /* 0.5 s: not yet facing */
    CHECK(!camera_facing(&c, 0.05f));
    for (int i = 0; i < 12; i++) camera_step(&c, DT);       /* 0.7 s total */
    CHECK(camera_facing(&c, 0.05f));
    CHECK_NEAR(c.angle, TRIG_HALF_PI, 0.05);
    camera_stop(&c);
    CHECK(camera_arrived(&c));

    /* turning takes the short way around the seam */
    camera_init(&c, 0, 0, 3.0f);
    camera_turn_to(&c, -3.0f);
    camera_step(&c, DT);
    CHECK(c.angle > 3.0f || c.angle < -3.0f);               /* moved toward +pi, not back through 0 */
    for (int i = 0; i < 12; i++) camera_step(&c, DT);
    CHECK(camera_facing(&c, 0.01f));
    CHECK_NEAR(c.angle, -3.0f, 0.01);

    /* face a point: due -y is -pi/2 */
    camera_init(&c, 1, 1, 0);
    camera_face(&c, 1.0f, -4.0f);
    CHECK_NEAR(c.target_angle, -TRIG_HALF_PI, 0.01);
    CHECK_EQ(c.auto_face, 0);
    for (int i = 0; i < 60; i++) camera_step(&c, DT);
    CHECK(camera_facing(&c, 0.01f));

    /* pitch is plain state the hunt drives */
    c.pitch_px = 12; CHECK_EQ(c.pitch_px, 12);
}
```

Add `void test_camera(void);` and `RUN(test_camera);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `camera.h` not found.

- [ ] **Step 3: Implement**

`sim/camera.h`:

```c
#ifndef CAMERA_H_HEADER
#define CAMERA_H_HEADER

#define CAM_WALK_SPEED 1.4f
#define CAM_TURN_RATE 2.5f
#define CAM_FOV_DEG 85.0f
#define CAM_BOB_PX 3
#define CAM_STEP_HZ 1.6f

typedef struct {
    float x, y, angle;             /* tiles, radians; angle 0 faces +x, +pi/2 faces +y */
    float target_x, target_y;
    int   walking, auto_face;
    float target_angle;
    int   turning;
    float bob_phase;
    int   pitch_px;                /* horizon offset driven by the hunt (dip, hop) */
    float speed;
} Camera;

void  camera_init(Camera *c, float x, float y, float angle);
void  camera_walk_to(Camera *c, float x, float y);
void  camera_stop(Camera *c);
void  camera_face(Camera *c, float x, float y);
void  camera_turn_to(Camera *c, float angle);
int   camera_arrived(const Camera *c);
int   camera_facing(const Camera *c, float tol);
float camera_dist(const Camera *c, float x, float y);
void  camera_step(Camera *c, float dt);
int   camera_bob_px(const Camera *c);
float camera_fov(void);
float camera_proj(int w);      /* (w/2) / tan(fov/2) */

#endif
```

`sim/camera.c`:

```c
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
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/camera.h sim/camera.c test/test_camera.c test/test_main.c
git commit -m "Add the first-person camera with steering, bob, and pitch"
```

---

### Task 7: Footprint decals

**Files:**
- Create: `sim/decals.h`, `sim/decals.c`, `test/test_decals.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `DECAL_MAX 512`, `DECAL_PER_TILE 8`, `DECAL_SPACING 0.5f`, `DECAL_OFFSET 0.15f`, `DECAL_HALF_LEN 0.22f`, `DECAL_HALF_WID 0.09f`, `DECAL_GLOW_BRIGHT_PAIRS 8`, `DECAL_GLOW_DIM 0.35f`; `Footprint {x, y, ca, sa, index, active}`; `Decals`; `decals_clear`, `decals_lay_trail(Decals*, const Path*)` returning prints laid, `decals_set_head_waypoint(Decals*, int wp)`, `decals_sample(const Decals*, fx, fy)` returning 0 outside prints or a glow in `[DECAL_GLOW_DIM, 1]`, `decals_tile_has_print(const Decals*, tx, ty)` (1 if any active print is registered on that tile), `decals_cull_behind(Decals*, cx, cy, max_behind)` returning the count deactivated.
- Prints are registered in every tile their bounding box touches so they render whole across tile edges.

- [ ] **Step 1: Write the failing tests**

`test/test_decals.c`:

```c
#include "test.h"
#include "decals.h"

void test_decals(void) {
    static Decals d;
    decals_clear(&d);
    CHECK_EQ(d.n, 0);
    CHECK_NEAR(decals_sample(&d, 1.5f, 1.5f), 0.0f, 1e-6);

    /* straight path along y = 1 from x = 1 to x = 5: four tiles, a pair every half tile */
    Path p; p.n = 5;
    for (int i = 0; i < 5; i++) { p.t[i].x = 1 + i; p.t[i].y = 1; }
    int laid = decals_lay_trail(&d, &p);
    CHECK(laid >= 16 && laid <= 18);
    CHECK_EQ(d.n, laid);
    CHECK_EQ(d.wp_first[0], 0);
    for (int i = 1; i < p.n; i++) CHECK(d.wp_first[i] >= d.wp_first[i - 1]);
    CHECK(d.wp_first[4] > d.wp_first[1]);

    /* prints sit 0.15 either side of the centre line, oriented along +x */
    int left = 0, right = 0;
    for (int i = 0; i < d.n; i++) {
        CHECK_NEAR(d.p[i].ca, 1.0f, 1e-3); CHECK_NEAR(d.p[i].sa, 0.0f, 1e-3);
        if (d.p[i].y < 1.5f) left++; else right++;
        CHECK(d.p[i].x >= 1.4f && d.p[i].x <= 5.6f);
    }
    CHECK(left >= 8 && right >= 8);

    /* sampling: on a print > 0, on the centre line 0, far away 0 */
    CHECK(decals_sample(&d, d.p[0].x, d.p[0].y) > 0.0f);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, 1.5f), 0.0f, 1e-6);
    CHECK_NEAR(decals_sample(&d, 20.0f, 20.0f), 0.0f, 1e-6);
    CHECK(decals_sample(&d, d.p[0].x + 0.2f, d.p[0].y) > 0.0f);          /* within half length */
    CHECK_NEAR(decals_sample(&d, d.p[0].x + 0.25f, d.p[0].y), 0.0f, 1e-6); /* in the 0.06 gap before the next pair */

    /* glow: bright ahead of the head, dim behind */
    decals_set_head_waypoint(&d, 0);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), 1.0f, 1e-6);
    decals_set_head_waypoint(&d, 3);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), DECAL_GLOW_DIM, 1e-6);
    int h = d.wp_first[3];
    CHECK_NEAR(decals_sample(&d, d.p[h].x, d.p[h].y), 1.0f, 1e-6);

    CHECK(decals_tile_has_print(&d, 2, 1));
    CHECK(!decals_tile_has_print(&d, 2, 4));
    CHECK(!decals_tile_has_print(&d, -1, 1));

    /* culling removes prints more than 2 tiles behind a camera at x = 5.5 */
    int culled = decals_cull_behind(&d, 5.5f, 1.5f, 2.0f);
    CHECK(culled > 0);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), 0.0f, 1e-6);
    CHECK(decals_sample(&d, d.p[h].x, d.p[h].y) > 0.0f);
    CHECK_EQ(decals_cull_behind(&d, 5.5f, 1.5f, 2.0f), 0);
    CHECK(!decals_tile_has_print(&d, 2, 1));                  /* every print on that tile was culled */

    /* an L-shaped path orients the second leg along +y */
    decals_clear(&d);
    p.n = 5; p.t[0] = (Tile){2, 2}; p.t[1] = (Tile){3, 2}; p.t[2] = (Tile){4, 2}; p.t[3] = (Tile){4, 3}; p.t[4] = (Tile){4, 4};
    decals_lay_trail(&d, &p);
    CHECK_NEAR(d.p[d.n - 1].ca, 0.0f, 1e-3); CHECK_NEAR(d.p[d.n - 1].sa, 1.0f, 1e-3);
    CHECK(d.p[d.n - 1].x > 4.3f && d.p[d.n - 1].x < 4.7f);
    CHECK_NEAR(decals_sample(&d, 4.5f, d.p[d.n - 1].y), 0.0f, 1e-6);      /* centre line of the +y leg */

    /* a path too long for the print budget is truncated safely */
    decals_clear(&d);
    p.n = MAP_MAX_PATH;
    for (int i = 0; i < p.n; i++) { p.t[i].x = i % 40; p.t[i].y = i / 40; }
    laid = decals_lay_trail(&d, &p);
    CHECK(laid <= DECAL_MAX);
    CHECK_EQ(d.n, laid);
}
```

Add `void test_decals(void);` and `RUN(test_decals);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `decals.h` not found.

- [ ] **Step 3: Implement**

`sim/decals.h`:

```c
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
```

`sim/decals.c`:

```c
#include "decals.h"
#include "fmath.h"

void decals_clear(Decals *d) {
    d->n = 0; d->head = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) for (int k = 0; k < DECAL_PER_TILE; k++) d->cell[y][x][k] = -1;
    for (int i = 0; i < MAP_MAX_PATH; i++) d->wp_first[i] = 0;
}

static void register_print(Decals *d, int idx) {
    const Footprint *p = &d->p[idx];
    int x0 = (int)(p->x - DECAL_HALF_LEN), x1 = (int)(p->x + DECAL_HALF_LEN);
    int y0 = (int)(p->y - DECAL_HALF_LEN), y1 = (int)(p->y + DECAL_HALF_LEN);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
        for (int k = 0; k < DECAL_PER_TILE; k++) if (d->cell[y][x][k] < 0) { d->cell[y][x][k] = (int16_t)idx; break; }
    }
}

static int add_print(Decals *d, float x, float y, float ca, float sa) {
    if (d->n >= DECAL_MAX) return 0;
    Footprint *p = &d->p[d->n];
    p->x = x; p->y = y; p->ca = ca; p->sa = sa; p->index = d->n; p->active = 1;
    register_print(d, d->n);
    d->n++;
    return 1;
}

int decals_lay_trail(Decals *d, const Path *path) {
    decals_clear(d);
    if (path->n < 2) return 0;
    float carry = 0.0f;                                     /* distance since the last pair */
    for (int i = 0; i + 1 < path->n && i < MAP_MAX_PATH; i++) {
        d->wp_first[i] = d->n;
        float ax = (float)path->t[i].x + 0.5f, ay = (float)path->t[i].y + 0.5f;
        float bx = (float)path->t[i + 1].x + 0.5f, by = (float)path->t[i + 1].y + 0.5f;
        float dx = bx - ax, dy = by - ay, len = fm_sqrt(dx * dx + dy * dy);
        if (len < 0.0001f) continue;
        float ca = dx / len, sa = dy / len;                   /* heading */
        float rx = -sa, ry = ca;                              /* right-hand perpendicular */
        float s = DECAL_SPACING - carry;                      /* first pair position along this segment */
        while (s <= len) {
            float px = ax + ca * s, py = ay + sa * s;
            if (!add_print(d, px - rx * DECAL_OFFSET, py - ry * DECAL_OFFSET, ca, sa)) return d->n;
            if (!add_print(d, px + rx * DECAL_OFFSET, py + ry * DECAL_OFFSET, ca, sa)) return d->n;
            s += DECAL_SPACING;
        }
        carry = len - (s - DECAL_SPACING);
    }
    for (int i = path->n - 1; i < MAP_MAX_PATH; i++) d->wp_first[i] = d->n;
    return d->n;
}

void decals_set_head_waypoint(Decals *d, int waypoint) {
    if (waypoint < 0) waypoint = 0;
    if (waypoint >= MAP_MAX_PATH) waypoint = MAP_MAX_PATH - 1;
    d->head = d->wp_first[waypoint];
}

static float glow_for(const Decals *d, int index) {
    if (index < d->head) return DECAL_GLOW_DIM;
    int pairs_ahead = (index - d->head) / 2;
    if (pairs_ahead < DECAL_GLOW_BRIGHT_PAIRS) return 1.0f;
    float g = 1.0f - 0.5f * (float)(pairs_ahead - DECAL_GLOW_BRIGHT_PAIRS) / (float)DECAL_GLOW_BRIGHT_PAIRS;
    return g < 0.5f ? 0.5f : g;
}

float decals_sample(const Decals *d, float fx, float fy) {
    int tx = (int)fx, ty = (int)fy;
    if (fx < 0 || fy < 0 || tx >= MAP_W || ty >= MAP_H) return 0.0f;
    for (int k = 0; k < DECAL_PER_TILE; k++) {
        int idx = d->cell[ty][tx][k];
        if (idx < 0) break;
        const Footprint *p = &d->p[idx];
        if (!p->active) continue;
        float dx = fx - p->x, dy = fy - p->y;
        float along = dx * p->ca + dy * p->sa, across = -dx * p->sa + dy * p->ca;
        if (fm_abs(along) <= DECAL_HALF_LEN && fm_abs(across) <= DECAL_HALF_WID) return glow_for(d, idx);
    }
    return 0.0f;
}

int decals_tile_has_print(const Decals *d, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return 0;
    for (int k = 0; k < DECAL_PER_TILE; k++) {
        int idx = d->cell[ty][tx][k];
        if (idx < 0) break;
        if (d->p[idx].active) return 1;
    }
    return 0;
}

int decals_cull_behind(Decals *d, float cx, float cy, float max_behind) {
    int culled = 0;
    for (int i = 0; i < d->n && i < d->head; i++) {
        Footprint *p = &d->p[i];
        if (!p->active) continue;
        float dx = p->x - cx, dy = p->y - cy;
        if (dx * dx + dy * dy > max_behind * max_behind) { p->active = 0; culled++; }
    }
    return culled;
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/decals.h sim/decals.c test/test_decals.c test/test_main.c
git commit -m "Add the footprint decal trail"
```

---

### Task 8: World sprites: projection and depth-tested drawing

**Files:**
- Modify: `sim/draw.h`, `sim/draw.c`, `test/test_draw.c` (add `draw_shade`)
- Create: `sim/sprites.h`, `sim/sprites.c`, `test/test_sprites.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces in draw: `Color draw_shade(Color c, float f)` scaling r, g, b by `f` clamped to `[0, 1]`, alpha untouched.
- Produces in sprites: `MAX_SPRITES 64`; `Sprite {active, x, y, kind, size}` (`kind` is an `SPR_*` texture index, `size` the height in tiles); `Sprites {s[], n}`; `SpriteProj {visible, depth, screen_x, height, width, top, bottom}`; `sprites_clear`, `sprites_add(sprites, kind, x, y, size)` returning NULL when full, `sprites_remove(sprites, sprite)`, `sprite_project(cam, proj, w, horizon, sx, sy, size, aspect, out)` returning `visible`, `sprites_draw(sprites, cam, map, textures, pal, fb, depth, horizon, proj)`.
- Projection: `depth = dx*cos a + dy*sin a` (culled below 0.15), `lateral = -dx*sin a + dy*cos a`, `screen_x = w/2 + proj*lateral/depth`, `height = proj*size/depth`, `bottom = horizon + proj*0.5/depth`, `top = bottom - height`, `width = height*aspect`. Shade = `raycast_shade(depth, light, 0)` from Task 9 is not available yet, so sprites compute `fog = 1/(1+0.35*depth)` and `fog*(0.45+0.55*light)` locally with the same constants.

- [ ] **Step 1: Write the failing tests**

Append inside `test_draw` in `test/test_draw.c` before the closing brace:

```c
    Color sh = draw_shade(COLOR(200, 100, 50), 0.5f);
    CHECK(sh.r == 100 && sh.g == 50 && sh.b == 25 && sh.a == 255);
    sh = draw_shade(COLOR(200, 100, 50), 2.0f);
    CHECK(sh.r == 200 && sh.g == 100 && sh.b == 50);
    sh = draw_shade(COLOR(200, 100, 50), -1.0f);
    CHECK(sh.r == 0 && sh.g == 0 && sh.b == 0 && sh.a == 255);
```

`test/test_sprites.c`:

```c
#include "test.h"
#include "sprites.h"
#include "textures.h"
#include "palette.h"
#include "map.h"
#include "camera.h"

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

static uint8_t px[320 * 200 * 4];
static float depth[320];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void test_sprites(void) {
    static Map m; static Textures t; static Sprites sp;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 5u);
    CHECK_EQ(map_parse(&m, ROOM, 7, 7), 0);
    textures_generate(&t, &rng, pal, "ROHIT");
    Camera cam; camera_init(&cam, 2.5f, 3.5f, 0.0f);
    int w = 320, h = 200, horizon = 100;
    float proj = camera_proj(w);

    SpriteProj pr;
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 3.5f, 1.0f, 1.0f, &pr));     /* dead ahead, depth 2 */
    CHECK_NEAR(pr.depth, 2.0f, 1e-4);
    CHECK_EQ(pr.screen_x, 160);
    CHECK_EQ(pr.height, (int)(proj * 1.0f / 2.0f));
    CHECK_EQ(pr.bottom, horizon + (int)(proj * 0.5f / 2.0f));
    CHECK_EQ(pr.top, pr.bottom - pr.height);
    CHECK_EQ(pr.width, pr.height);
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 3.5f, 2.0f, 0.5f, &pr));     /* lamp aspect */
    CHECK_EQ(pr.width, pr.height / 2);
    CHECK(!sprite_project(&cam, proj, w, horizon, 0.5f, 3.5f, 1.0f, 1.0f, &pr));    /* behind */
    CHECK(!sprite_project(&cam, proj, w, horizon, 2.5f, 5.5f, 1.0f, 1.0f, &pr));    /* beside: depth 0 */
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 4.5f, 1.0f, 1.0f, &pr));     /* right of centre (+y is right) */
    CHECK(pr.screen_x > 200);
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 2.5f, 1.0f, 1.0f, &pr));
    CHECK(pr.screen_x < 120);

    /* list management */
    sprites_clear(&sp);
    CHECK_EQ(sp.n, 0);
    Sprite *a = sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f);
    CHECK(a != NULL); CHECK_EQ(sp.n, 1); CHECK(a->active);
    for (int i = 1; i < MAX_SPRITES; i++) CHECK(sprites_add(&sp, SPR_PAPER_A, 1.5f, 1.5f, 0.3f) != NULL);
    CHECK(sprites_add(&sp, SPR_PAPER_A, 1.5f, 1.5f, 0.3f) == NULL);
    sprites_remove(&sp, a);
    CHECK(!a->active);
    CHECK(sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f) != NULL);              /* slot reused */

    /* drawing: occluded by a near depth buffer, visible against a far one */
    Framebuffer fb; fb_init(&fb, px, w, h);
    sprites_clear(&sp);
    sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f);
    for (int i = 0; i < w; i++) depth[i] = 1.0f;
    draw_clear(&fb, pal[COL_BG]);
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    int painted = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!same(fb_get(&fb, x, y), pal[COL_BG])) painted++;
    CHECK_EQ(painted, 0);
    for (int i = 0; i < w; i++) depth[i] = 10.0f;
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    painted = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!same(fb_get(&fb, x, y), pal[COL_BG])) painted++;
    CHECK(painted > 200);
    CHECK_EQ(painted < 100, 0);
    for (int y = 0; y < 30; y++) for (int x = 0; x < w; x++) CHECK(same(fb_get(&fb, x, y), pal[COL_BG]));   /* nothing above the sprite */
    Color c = fb_get(&fb, 160, 138);                                    /* inside the can body */
    CHECK(c.r < pal[COL_TRASH].r && c.r > 20);                           /* shaded, not black */

    /* far-to-near order: the nearer bug wins the centre pixel */
    sprites_clear(&sp);
    sprites_add(&sp, SPR_BUG_EXPOSED, 3.5f, 3.5f, 1.0f);                /* near, depth 1 */
    sprites_add(&sp, SPR_TRASHCAN, 5.5f, 3.5f, 1.0f);                   /* far, depth 3 */
    draw_clear(&fb, pal[COL_BG]);
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    Color mid = fb_get(&fb, 160, horizon + 60);
    CHECK(mid.g > mid.r);                                                /* bug green, not trash grey */
}
```

Add `void test_sprites(void);` and `RUN(test_sprites);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `draw_shade` undeclared.

- [ ] **Step 3: Implement**

Add to `sim/draw.h` after `draw_dim`:

```c
Color draw_shade(Color c, float f);   /* scale rgb by f clamped to [0,1]; alpha unchanged */
```

Append to `sim/draw.c`:

```c
Color draw_shade(Color c, float f) {
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    c.r = (uint8_t)((float)c.r * f);
    c.g = (uint8_t)((float)c.g * f);
    c.b = (uint8_t)((float)c.b * f);
    return c;
}
```

`sim/sprites.h`:

```c
#ifndef SPRITES_H_HEADER
#define SPRITES_H_HEADER
#include "draw.h"
#include "camera.h"
#include "map.h"
#include "textures.h"

#define MAX_SPRITES 64
#define SPRITE_MIN_DEPTH 0.15f

typedef struct { int active; float x, y; int kind; float size; } Sprite;
typedef struct { Sprite s[MAX_SPRITES]; int n; } Sprites;
typedef struct { int visible; float depth; int screen_x; int height, width; int top, bottom; } SpriteProj;

void    sprites_clear(Sprites *sp);
Sprite *sprites_add(Sprites *sp, int kind, float x, float y, float size);
void    sprites_remove(Sprites *sp, Sprite *s);
int     sprite_project(const Camera *cam, float proj, int w, int horizon, float sx, float sy, float size, float aspect, SpriteProj *out);
void    sprites_draw(const Sprites *sp, const Camera *cam, const Map *m, const Textures *t, const Color *pal,
                     Framebuffer *fb, const float *depth, int horizon, float proj);

#endif
```

`sim/sprites.c`:

```c
#include "sprites.h"
#include "trig.h"
#include "fmath.h"

void sprites_clear(Sprites *sp) { sp->n = 0; for (int i = 0; i < MAX_SPRITES; i++) sp->s[i].active = 0; }

Sprite *sprites_add(Sprites *sp, int kind, float x, float y, float size) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        Sprite *s = &sp->s[i];
        if (s->active) continue;
        s->active = 1; s->kind = kind; s->x = x; s->y = y; s->size = size;
        if (i >= sp->n) sp->n = i + 1;
        return s;
    }
    return 0;
}

void sprites_remove(Sprites *sp, Sprite *s) { (void)sp; if (s) s->active = 0; }

int sprite_project(const Camera *cam, float proj, int w, int horizon, float sx, float sy, float size, float aspect, SpriteProj *out) {
    float ca = trig_cos(cam->angle), sa = trig_sin(cam->angle);
    float dx = sx - cam->x, dy = sy - cam->y;
    float depth = dx * ca + dy * sa;
    out->visible = 0;
    if (depth < SPRITE_MIN_DEPTH) return 0;
    float lateral = -dx * sa + dy * ca;
    out->depth = depth;
    out->screen_x = (int)((float)w * 0.5f + proj * lateral / depth);
    out->height = (int)(proj * size / depth);
    out->width = (int)((float)out->height * aspect);
    out->bottom = horizon + (int)(proj * 0.5f / depth);
    out->top = out->bottom - out->height;
    out->visible = out->height > 0 && out->width > 0;
    return out->visible;
}

static float sprite_shade(const Map *m, float depth, float sx, float sy) {
    float fog = 1.0f / (1.0f + 0.35f * depth);
    float light = map_light(m, (int)sx, (int)sy);
    return fog * (0.45f + 0.55f * light);
}

void sprites_draw(const Sprites *sp, const Camera *cam, const Map *m, const Textures *t, const Color *pal,
                  Framebuffer *fb, const float *depth, int horizon, float proj) {
    (void)pal;
    int order[MAX_SPRITES], n = 0;
    SpriteProj pr[MAX_SPRITES];
    for (int i = 0; i < sp->n; i++) {
        const Sprite *s = &sp->s[i];
        if (!s->active) continue;
        int sh = t->sprite_h[s->kind];
        if (!sprite_project(cam, proj, fb->w, horizon, s->x, s->y, s->size, (float)SPR_W / (float)sh, &pr[i])) continue;
        order[n++] = i;
    }
    for (int i = 1; i < n; i++) {                            /* insertion sort, far to near */
        int k = order[i], j = i - 1;
        while (j >= 0 && pr[order[j]].depth < pr[k].depth) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int oi = 0; oi < n; oi++) {
        const Sprite *s = &sp->s[order[oi]];
        const SpriteProj *p = &pr[order[oi]];
        int sh = t->sprite_h[s->kind];
        float shade = sprite_shade(m, p->depth, s->x, s->y);
        int x0 = p->screen_x - p->width / 2;
        for (int col = x0; col < x0 + p->width; col++) {
            if (col < 0 || col >= fb->w) continue;
            if (p->depth >= depth[col]) continue;
            int u = (col - x0) * SPR_W / p->width;
            int r0 = p->top < 0 ? 0 : p->top, r1 = p->bottom > fb->h ? fb->h : p->bottom;
            for (int row = r0; row < r1; row++) {
                int v = (row - p->top) * sh / p->height;
                Color c = texture_sprite(t, s->kind, u, v);
                if (c.a != 255) continue;
                c = draw_shade(c, shade);
                uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
                px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
            }
        }
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/draw.h sim/draw.c test/test_draw.c sim/sprites.h sim/sprites.c test/test_sprites.c test/test_main.c
git commit -m "Add world sprites with projection and depth-tested drawing"
```

---

### Task 9: Raycaster: sky, walls, floor

**Files:**
- Create: `sim/raycast.h`, `sim/raycast.c`, `test/test_raycast.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `RAY_WALL_TOP 2.0f`, `RAY_WALL_BOTTOM 0.5f`, `RAY_FOG 0.35f`, `RAY_SIDE_SHADE 0.8f`, `RAY_MAX_DIST 64.0f`; `RayHit {hit, dist, side, tex, tile_x, tile_y, u, light, variant}`; `float raycast_shade(float dist, float light, int side)`; `float raycast_row_dist(float proj, int row, int horizon)`; `int raycast_column(map, cam, proj, w, col, out)`; `void raycast_sky(textures, cam, proj, fb, horizon)`; `void raycast_walls(map, textures, pal, cam, proj, fb, horizon, depth)`; `void raycast_floor(map, textures, pal, decals, cam, proj, fb, horizon, depth)`.
- Ray for column `col`: `cameraX = 2*(col+0.5)/w - 1`, `dir = (cos a, sin a)`, `plane = (-sin a, cos a) * tan(fov/2)`, `ray = dir + plane*cameraX`. DDA over the tile grid; perpendicular distance; `u` is the fractional hit position along the wall, flipped so textures read left to right from the viewer's side. `light` is the light of the floor tile the ray was in before hitting.
- Render order in the world (Task 12): sky, walls (fills `depth`), floor, sprites, HUD.

- [ ] **Step 1: Write the failing tests**

`test/test_raycast.c`:

```c
#include "test.h"
#include "raycast.h"
#include "decals.h"
#include "palette.h"
#include "trig.h"

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

static uint8_t px[320 * 200 * 4];
static float depth[320];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int yellow(Color c) { return c.r > 180 && c.g > 130 && c.b < 110; }

void test_raycast(void) {
    static Map m; static Textures t; static Decals d;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 9u);
    CHECK_EQ(map_parse(&m, ROOM, 7, 7), 0);
    textures_generate(&t, &rng, pal, "ROHIT");
    int w = 320, h = 200, horizon = 100;
    float proj = camera_proj(w);
    Camera cam; camera_init(&cam, 1.5f, 3.5f, 0.0f);          /* facing +x down the middle */

    CHECK_NEAR(raycast_shade(0.0f, 1.0f, 0), 1.0f, 1e-5);
    CHECK_NEAR(raycast_shade(0.0f, 0.25f, 0), 0.45f + 0.55f * 0.25f, 1e-5);
    CHECK_NEAR(raycast_shade(2.0f, 1.0f, 0), 1.0f / 1.7f, 1e-5);
    CHECK_NEAR(raycast_shade(2.0f, 1.0f, 1), 0.8f / 1.7f, 1e-5);
    CHECK_NEAR(raycast_row_dist(proj, horizon + 50, horizon), 0.5f * proj / 50.0f, 1e-4);
    CHECK_NEAR(raycast_row_dist(proj, horizon + 1, horizon), 0.5f * proj, 1e-4);

    /* centre column hits the far wall at x = 6: distance 4.5 */
    RayHit hit;
    CHECK(raycast_column(&m, &cam, proj, w, w / 2, &hit));
    CHECK(hit.hit);
    CHECK_NEAR(hit.dist, 4.5f, 0.01);
    CHECK_EQ(hit.side, 0);
    CHECK_EQ(hit.tile_x, 6); CHECK_EQ(hit.tile_y, 3);
    CHECK_EQ(hit.tex, TEX_BRICK);
    CHECK(hit.u >= 0.0f && hit.u < 1.0f);
    CHECK_NEAR(hit.light, 0.25f, 1e-5);
    CHECK(raycast_column(&m, &cam, proj, w, 0, &hit));       /* far left ray hits something farther than the centre */
    CHECK(hit.hit);
    CHECK(hit.dist > 2.0f);
    for (int c = 0; c < w / 2; c++) {                          /* the room is symmetric about y = 3.5 */
        RayHit a, b;
        raycast_column(&m, &cam, proj, w, c, &a);
        raycast_column(&m, &cam, proj, w, w - 1 - c, &b);
        CHECK_NEAR(a.dist, b.dist, 0.05);
    }
    Camera side; camera_init(&side, 3.5f, 3.5f, TRIG_HALF_PI);  /* facing +y: hits the y = 6 wall, side 1 */
    CHECK(raycast_column(&m, &side, proj, w, w / 2, &hit));
    CHECK_EQ(hit.side, 1);
    CHECK_NEAR(hit.dist, 2.5f, 0.01);
    CHECK_EQ(hit.tile_y, 6);

    /* sky fills the rows above the horizon and changes with the view direction */
    Framebuffer fb; fb_init(&fb, px, w, h);
    draw_clear(&fb, COLOR(0, 0, 0));
    raycast_sky(&t, &cam, proj, &fb, horizon);
    CHECK(fb_get(&fb, 160, 0).b > 0);
    CHECK(same(fb_get(&fb, 160, horizon + 5), COLOR(0, 0, 0)));   /* nothing below the horizon */
    int diff = 0;
    Camera turned; camera_init(&turned, 1.5f, 3.5f, TRIG_PI);
    static uint8_t px2[320 * 200 * 4];
    Framebuffer fb2; fb_init(&fb2, px2, w, h);
    draw_clear(&fb2, COLOR(0, 0, 0));
    raycast_sky(&t, &turned, proj, &fb2, horizon);
    for (int y = horizon - 40; y < horizon; y++) for (int x = 0; x < w; x += 4) if (!same(fb_get(&fb, x, y), fb_get(&fb2, x, y))) diff++;
    CHECK(diff > 100);

    /* walls: depth buffer, wall pixels, sky left above a far wall */
    Color sky_before = fb_get(&fb, 160, 5);
    raycast_walls(&m, &t, pal, &cam, proj, &fb, horizon, depth);
    CHECK_NEAR(depth[w / 2], 4.5f, 0.01);
    Color wc = fb_get(&fb, 160, horizon);
    CHECK(wc.r > wc.g);                                        /* brick, reddish */
    CHECK(same(fb_get(&fb, 160, 5), sky_before));              /* sky untouched above the far wall (top ~ row 22) */
    int expected_top = horizon - (int)(RAY_WALL_TOP / 4.5f * proj);
    CHECK(expected_top > 10 && expected_top < 40);
    CHECK(fb_get(&fb, 160, expected_top + 2).r > fb_get(&fb, 160, expected_top + 2).b);   /* wall just below its top */

    /* floor: asphalt below the horizon, footprints glow along the trail, never above the horizon */
    decals_clear(&d);
    Path p; p.n = 5;
    for (int i = 0; i < 5; i++) { p.t[i].x = 1 + i; p.t[i].y = 3; }
    decals_lay_trail(&d, &p);
    decals_set_head_waypoint(&d, 0);
    raycast_floor(&m, &t, pal, &d, &cam, proj, &fb, horizon, depth);
    Color fl = fb_get(&fb, 160, h - 1);
    CHECK(fl.b >= fl.r && fl.r > 10);                          /* asphalt-ish, shaded */
    int yellow_below = 0, yellow_above = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        if (yellow(fb_get(&fb, x, y))) { if (y > horizon) yellow_below++; else yellow_above++; }
    }
    CHECK(yellow_below > 40);
    CHECK_EQ(yellow_above, 0);
    /* the floor never paints over the wall: the row just above the wall's bottom is wall-coloured */
    int wall_bottom = horizon + (int)(RAY_WALL_BOTTOM / 4.5f * proj);
    CHECK(fb_get(&fb, 160, wall_bottom - 1).r > fb_get(&fb, 160, wall_bottom - 1).b);
}
```

Add `void test_raycast(void);` and `RUN(test_raycast);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `raycast.h` not found.

- [ ] **Step 3: Implement**

`sim/raycast.h`:

```c
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
```

`sim/raycast.c`:

```c
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
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`. If the symmetry check fails by more than the tolerance at the extreme columns, the ray is not centred on the pixel: keep `col + 0.5`.

- [ ] **Step 5: Commit**

```bash
git add sim/raycast.h sim/raycast.c test/test_raycast.c test/test_main.c
git commit -m "Add the raycaster: sky strip, textured walls with depth, floor with footprints"
```

---

### Task 10: HUD: hammer and the +1 flash

**Files:**
- Create: `sim/hud.h`, `sim/hud.c`, `test/test_hud.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `HUD_SWING_TIME 0.4f`, `HUD_HIT_TIME 0.25f`, `HUD_PLUS_TIME 0.8f`; `Hud {swinging, swing_t, hit_fired, plus_t}`; `hud_init`, `hud_start_swing`, `int hud_step(hud, dt)` returning 1 on exactly the step the hit lands, `hud_flash_plus`, `int hud_frame(hud)` (0 rest, 1 raised for the first 0.15 s of a swing, 2 down after), `hud_draw(hud, textures, pal, fb, bob_px)` blitting the frame at bottom centre offset by `bob_px` and drawing `+1` in footprint yellow while `plus_t > 0`.

- [ ] **Step 1: Write the failing tests**

`test/test_hud.c`:

```c
#include "test.h"
#include "hud.h"
#include "palette.h"

#define DT (1.0f / 60.0f)
static uint8_t px[320 * 200 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int count(const Framebuffer *fb, Color c, int y0, int y1) {
    int n = 0; for (int y = y0; y < y1; y++) for (int x = 0; x < fb->w; x++) if (same(fb_get(fb, x, y), c)) n++; return n;
}

void test_hud(void) {
    static Textures t;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 2u);
    textures_generate(&t, &rng, pal, "ROHIT");
    Hud h; hud_init(&h);
    CHECK_EQ(hud_frame(&h), 0);
    CHECK(!h.swinging);

    hud_start_swing(&h);
    CHECK(h.swinging);
    int hits = 0, hit_step = -1, saw_raised = 0, saw_down = 0;
    for (int i = 0; i < 40; i++) {
        if (hud_step(&h, DT)) { hits++; hit_step = i; }
        if (hud_frame(&h) == 1) saw_raised = 1;
        if (hud_frame(&h) == 2) saw_down = 1;
    }
    CHECK_EQ(hits, 1);
    CHECK(hit_step >= 13 && hit_step <= 16);                 /* 0.25 s */
    CHECK(saw_raised); CHECK(saw_down);
    CHECK(!h.swinging);                                        /* 0.4 s swing is over after 40 steps */
    CHECK_EQ(hud_frame(&h), 0);
    for (int i = 0; i < 10; i++) CHECK(!hud_step(&h, DT));   /* no second hit */

    hud_flash_plus(&h);
    CHECK(h.plus_t > 0.79f);
    for (int i = 0; i < 48; i++) hud_step(&h, DT);
    CHECK(h.plus_t <= 0.001f);

    /* drawing: hammer yellow at the bottom centre, nothing at the top; +1 while flashing; bob shifts it */
    Framebuffer fb; fb_init(&fb, px, 320, 200);
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, 0);
    CHECK(count(&fb, pal[COL_HAMMER], 130, 200) > 150);
    CHECK_EQ(count(&fb, pal[COL_HAMMER], 0, 100), 0);
    CHECK_EQ(count(&fb, pal[COL_FOOTPRINT], 0, 200), 0);
    int hand_row = -1;
    for (int y = 199; y >= 0 && hand_row < 0; y--) if (same(fb_get(&fb, 160, y), pal[COL_HAND])) hand_row = y;
    CHECK(hand_row > 150);
    draw_clear(&fb, pal[COL_BG]);
    hud_flash_plus(&h);
    hud_draw(&h, &t, pal, &fb, 0);
    CHECK(count(&fb, pal[COL_FOOTPRINT], 0, 200) > 20);
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, 3);
    int top_a = 200; for (int y = 0; y < 200; y++) if (same(fb_get(&fb, 160, y), pal[COL_HAMMER]) || same(fb_get(&fb, 150, y), pal[COL_HAMMER])) { top_a = y; break; }
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, -3);
    int top_b = 200; for (int y = 0; y < 200; y++) if (same(fb_get(&fb, 160, y), pal[COL_HAMMER]) || same(fb_get(&fb, 150, y), pal[COL_HAMMER])) { top_b = y; break; }
    CHECK_EQ(top_a - top_b, 6);
}
```

Add `void test_hud(void);` and `RUN(test_hud);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `hud.h` not found.

- [ ] **Step 3: Implement**

`sim/hud.h`:

```c
#ifndef HUD_H_HEADER
#define HUD_H_HEADER
#include "draw.h"
#include "textures.h"

#define HUD_SWING_TIME 0.4f
#define HUD_HIT_TIME 0.25f
#define HUD_PLUS_TIME 0.8f

typedef struct { int swinging; float swing_t; int hit_fired; float plus_t; } Hud;

void hud_init(Hud *h);
void hud_start_swing(Hud *h);
int  hud_step(Hud *h, float dt);          /* 1 on the step the hit lands */
void hud_flash_plus(Hud *h);
int  hud_frame(const Hud *h);             /* 0 rest, 1 raised, 2 down */
void hud_draw(const Hud *h, const Textures *t, const Color *pal, Framebuffer *fb, int bob_px);

#endif
```

`sim/hud.c`:

```c
#include "hud.h"
#include "font.h"
#include "palette.h"

void hud_init(Hud *h) { h->swinging = 0; h->swing_t = 0.0f; h->hit_fired = 0; h->plus_t = 0.0f; }

void hud_start_swing(Hud *h) { h->swinging = 1; h->swing_t = 0.0f; h->hit_fired = 0; }

int hud_step(Hud *h, float dt) {
    int hit = 0;
    if (h->swinging) {
        h->swing_t += dt;
        if (!h->hit_fired && h->swing_t >= HUD_HIT_TIME - 1e-4f) { h->hit_fired = 1; hit = 1; }
        if (h->swing_t >= HUD_SWING_TIME - 1e-4f) { h->swinging = 0; h->swing_t = 0.0f; }
    }
    if (h->plus_t > 0.0f) { h->plus_t -= dt; if (h->plus_t < 0.0f) h->plus_t = 0.0f; }
    return hit;
}

void hud_flash_plus(Hud *h) { h->plus_t = HUD_PLUS_TIME; }

int hud_frame(const Hud *h) {
    if (!h->swinging) return 0;
    return h->swing_t < 0.15f ? 1 : 2;
}

void hud_draw(const Hud *h, const Textures *t, const Color *pal, Framebuffer *fb, int bob_px) {
    int frame = hud_frame(h);
    int x0 = (fb->w - HUD_W) / 2, y0 = fb->h - HUD_H + 4 + bob_px;
    for (int v = 0; v < HUD_H; v++) {
        int row = y0 + v;
        if (row < 0 || row >= fb->h) continue;
        for (int u = 0; u < HUD_W; u++) {
            int col = x0 + u;
            if (col < 0 || col >= fb->w) continue;
            Color c = t->hud[frame][v * HUD_W + u];
            if (c.a != 255) continue;
            uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
            px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
        }
    }
    if (h->plus_t > 0.0f) {
        int rise = (int)((HUD_PLUS_TIME - h->plus_t) * 25.0f);
        draw_text_scaled(fb, fb->w / 2 - 10, y0 - 24 - rise, "+1", 3, pal[COL_FOOTPRINT]);
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/hud.h sim/hud.c test/test_hud.c test/test_main.c
git commit -m "Add the hammer HUD and +1 flash"
```

---

### Task 11: Actors: dogs, wind-blown trash, the hidden bug

**Files:**
- Create: `sim/actors/dog.h`, `sim/actors/dog.c`, `sim/actors/trash.h`, `sim/actors/trash.c`, `sim/actors/bug.h`, `sim/actors/bug.c`, `test/test_actors.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Dog: `DOG_SPEED 1.0f`, `DOG_SNIFF_MIN 1.0f`, `DOG_SNIFF_MAX 2.0f`, `DOG_SNIFF_CHANCE 0.25f`; `DOG_WALK 0, DOG_SNIFF 1`; `Dog {x, y, dir, state, t, anim, tx, ty, spr}`; `dog_init(dog, tile_x, tile_y, sprite)`, `dog_step(dog, map, decals, rng, dt)`. Directions 0 +x, 1 -x, 2 +y, 3 -y. On arriving at a tile that has a footprint (`decals_tile_has_print`) or is next to a trash can, the dog sniffs with probability 0.25. Sprite kind alternates `SPR_DOG_A`/`SPR_DOG_B` at 4 Hz while walking, `SPR_DOG_SNIFF` while sniffing.
- Trash: `TRASH_GUST_MIN 4.0f`, `TRASH_GUST_MAX 10.0f`, `TRASH_GUST_LEN 1.2f`, `TRASH_GUST_SPEED 2.0f`, `TRASH_FRICTION 3.0f`; `Wind {until_next, left, dir_sign}`; `wind_init(wind, rng)`, `wind_step(wind, rng, dt)`, `wind_gusting(wind)`; `LOOSE_PAPER 0, LOOSE_NEWS 1`; `Loose {x, y, vx, vy, kind, anim, spr}`; `loose_init(loose, kind, tile_x, tile_y, sprite)`, `loose_step(loose, map, wind, dt)`. An item accelerates along its alley's open axis during a gust, decays with friction after, never leaves floor tiles, and alternates its two frames while moving.
- Bug: `BUGSTATE_HIDDEN 0, BUGSTATE_PEEK 1, BUGSTATE_EXPOSED 2, BUGSTATE_DEAD 3`; `HiddenBug {state, x, y, spr}`; `bug_place(bug, sprites, x, y)` adds an `SPR_BUG_HIDDEN` sprite of size 0.45, `bug_set_state(bug, sprites, state)` swaps the sprite kind and removes it when dead.

- [ ] **Step 1: Write the failing tests**

`test/test_actors.c`:

```c
#include "test.h"
#include "actors/dog.h"
#include "actors/trash.h"
#include "actors/bug.h"
#include "decals.h"

#define DT (1.0f / 60.0f)

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

void test_actors(void) {
    static Map city, room; static Decals d; static Sprites sp;
    CHECK_EQ(map_parse(&city, CITY_MAP, MAP_W, MAP_H), 0);
    CHECK_EQ(map_parse(&room, ROOM, 7, 7), 0);
    decals_clear(&d);
    sprites_clear(&sp);
    Rng rng; rng_seed(&rng, 1u);

    /* dog on the city: stays on floor for two minutes through dead ends, moves, animates */
    Dog dog;
    Sprite *ds = sprites_add(&sp, SPR_DOG_A, 1.5f, 6.5f, 0.6f);
    dog_init(&dog, 1, 6, ds);
    CHECK_NEAR(dog.x, 1.5f, 1e-5); CHECK_NEAR(dog.y, 6.5f, 1e-5);
    int moved = 0, saw_a = 0, saw_b = 0;
    float px0 = dog.x, py0 = dog.y;
    for (int i = 0; i < 120 * 60; i++) {
        dog_step(&dog, &city, &d, &rng, DT);
        CHECK(map_is_floor(&city, (int)dog.x, (int)dog.y));
        if (dog.x != px0 || dog.y != py0) moved = 1;
        if (ds->kind == SPR_DOG_A) saw_a = 1;
        if (ds->kind == SPR_DOG_B) saw_b = 1;
        if (dog.state == DOG_SNIFF) CHECK_EQ(ds->kind, SPR_DOG_SNIFF);
        CHECK_NEAR(ds->x, dog.x, 1e-5); CHECK_NEAR(ds->y, dog.y, 1e-5);
    }
    CHECK(moved); CHECK(saw_a); CHECK(saw_b);

    /* dog in the room with a footprint trail across the middle row: it sniffs */
    Path trail; trail.n = 5;
    for (int i = 0; i < 5; i++) { trail.t[i].x = 1 + i; trail.t[i].y = 3; }
    decals_lay_trail(&d, &trail);
    Dog dog2; dog_init(&dog2, 3, 3, ds);
    int saw_sniff = 0;
    for (int i = 0; i < 120 * 60 && !saw_sniff; i++) {
        dog_step(&dog2, &room, &d, &rng, DT);
        CHECK(map_is_floor(&room, (int)dog2.x, (int)dog2.y));
        if (dog2.state == DOG_SNIFF) { saw_sniff = 1; CHECK_EQ(ds->kind, SPR_DOG_SNIFF); }
    }
    CHECK(saw_sniff);
    decals_clear(&d);

    /* wind: gusts are scheduled 4-10 s apart and last 1.2 s */
    Wind wind; wind_init(&wind, &rng);
    CHECK(!wind_gusting(&wind));
    CHECK(wind.until_next >= TRASH_GUST_MIN && wind.until_next <= TRASH_GUST_MAX);
    int gust_steps = 0, gusts = 0, was = 0;
    for (int i = 0; i < 60 * 60; i++) {
        wind_step(&wind, &rng, DT);
        int g = wind_gusting(&wind);
        if (g) gust_steps++;
        if (g && !was) gusts++;
        was = g;
    }
    CHECK(gusts >= 5 && gusts <= 15);
    CHECK(gust_steps >= (gusts - 1) * 70 && gust_steps <= gusts * 74);   /* the last gust may be cut off at 60 s */

    /* loose trash: still without wind, moves during a gust, decays after, stops at walls */
    Loose paper;
    Sprite *ps = sprites_add(&sp, SPR_PAPER_A, 3.5f, 3.5f, 0.3f);
    loose_init(&paper, LOOSE_PAPER, 3, 3, ps);
    Wind calm = { 100.0f, 0.0f, 1 };
    for (int i = 0; i < 60; i++) loose_step(&paper, &room, &calm, DT);
    CHECK_NEAR(paper.x, 3.5f, 1e-5); CHECK_NEAR(paper.y, 3.5f, 1e-5);
    CHECK_EQ(ps->kind, SPR_PAPER_A);
    Wind gust = { 100.0f, TRASH_GUST_LEN, 1 };
    int saw_b_frame = 0;
    for (int i = 0; i < 30; i++) { loose_step(&paper, &room, &gust, DT); if (ps->kind == SPR_PAPER_B) saw_b_frame = 1; }
    CHECK(paper.x > 3.6f);
    CHECK(paper.vx > 0.5f);
    CHECK(saw_b_frame);
    for (int i = 0; i < 90; i++) loose_step(&paper, &room, &calm, DT);
    CHECK(paper.vx < 0.05f);
    CHECK(map_is_floor(&room, (int)paper.x, (int)paper.y));
    Wind west = { 100.0f, TRASH_GUST_LEN, -1 };
    for (int i = 0; i < 600; i++) { west.left = TRASH_GUST_LEN; loose_step(&paper, &room, &west, DT); }
    CHECK(paper.x >= 1.0f);                                   /* the x = 0 column is wall */
    CHECK(map_is_floor(&room, (int)paper.x, (int)paper.y));
    CHECK_NEAR(paper.vx, 0.0f, 1e-3);                          /* pinned against the wall */
    CHECK_NEAR(ps->x, paper.x, 1e-5);

    /* the hidden bug swaps sprites and disappears when dead */
    HiddenBug bug;
    int before = 0; for (int i = 0; i < MAX_SPRITES; i++) if (sp.s[i].active) before++;
    bug_place(&bug, &sp, 4.3f, 3.5f);
    CHECK_EQ(bug.state, BUGSTATE_HIDDEN);
    CHECK(bug.spr != NULL); CHECK_EQ(bug.spr->kind, SPR_BUG_HIDDEN);
    bug_set_state(&bug, &sp, BUGSTATE_PEEK);    CHECK_EQ(bug.spr->kind, SPR_BUG_PEEK);
    bug_set_state(&bug, &sp, BUGSTATE_EXPOSED); CHECK_EQ(bug.spr->kind, SPR_BUG_EXPOSED);
    bug_set_state(&bug, &sp, BUGSTATE_DEAD);
    CHECK_EQ(bug.state, BUGSTATE_DEAD);
    CHECK(bug.spr == NULL);
    int after = 0; for (int i = 0; i < MAX_SPRITES; i++) if (sp.s[i].active) after++;
    CHECK_EQ(after, before);
}
```

Add `void test_actors(void);` and `RUN(test_actors);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `actors/dog.h` not found.

- [ ] **Step 3: Implement**

`sim/actors/dog.h`:

```c
#ifndef ACTORS_DOG_H_HEADER
#define ACTORS_DOG_H_HEADER
#include "map.h"
#include "decals.h"
#include "sprites.h"
#include "rng.h"

#define DOG_SPEED 1.0f
#define DOG_SNIFF_MIN 1.0f
#define DOG_SNIFF_MAX 2.0f
#define DOG_SNIFF_CHANCE 0.25f

enum { DOG_WALK = 0, DOG_SNIFF = 1 };

typedef struct { float x, y; int dir; int state; float t; float anim; int tx, ty; Sprite *spr; } Dog;

void dog_init(Dog *d, int tile_x, int tile_y, Sprite *spr);
void dog_step(Dog *d, const Map *m, const Decals *decals, Rng *rng, float dt);

#endif
```

`sim/actors/dog.c`:

```c
#include "actors/dog.h"
#include "fmath.h"

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
static const int REV[4] = { 1, 0, 3, 2 };

void dog_init(Dog *d, int tile_x, int tile_y, Sprite *spr) {
    d->x = (float)tile_x + 0.5f; d->y = (float)tile_y + 0.5f;
    d->tx = tile_x; d->ty = tile_y; d->dir = 0; d->state = DOG_WALK; d->t = 0.0f; d->anim = 0.0f; d->spr = spr;
    if (spr) { spr->x = d->x; spr->y = d->y; spr->kind = SPR_DOG_A; }
}

static int near_interest(const Dog *d, const Map *m, const Decals *decals) {
    if (decals_tile_has_print(decals, d->tx, d->ty)) return 1;
    for (int k = 0; k < 4; k++) {
        int nx = d->tx + DX[k], ny = d->ty + DY[k];
        if (nx >= 0 && ny >= 0 && nx < m->w && ny < m->h && m->cell[ny][nx] == CELL_FLOOR && m->prop[ny][nx] == PROP_TRASH) return 1;
    }
    return 0;
}

static void choose_dir(Dog *d, const Map *m, Rng *rng) {
    int weights[4] = { 0, 0, 0, 0 }, total = 0;
    for (int k = 0; k < 4; k++) {
        if (!map_is_floor(m, d->tx + DX[k], d->ty + DY[k])) continue;
        if (k == REV[d->dir]) continue;
        weights[k] = (k == d->dir) ? 3 : 1;
        total += weights[k];
    }
    if (total == 0) {                                   /* dead end: turn around */
        int k = REV[d->dir];
        if (map_is_floor(m, d->tx + DX[k], d->ty + DY[k])) d->dir = k;
        return;
    }
    int pick = rng_range(rng, 1, total);
    for (int k = 0; k < 4; k++) { pick -= weights[k]; if (weights[k] && pick <= 0) { d->dir = k; return; } }
}

void dog_step(Dog *d, const Map *m, const Decals *decals, Rng *rng, float dt) {
    if (d->state == DOG_SNIFF) {
        d->t -= dt;
        if (d->t <= 0.0f) d->state = DOG_WALK;
    } else {
        float gx = (float)d->tx + 0.5f, gy = (float)d->ty + 0.5f;
        float dx = gx - d->x, dy = gy - d->y, dist = fm_sqrt(dx * dx + dy * dy), step = DOG_SPEED * dt;
        if (dist <= step) {
            d->x = gx; d->y = gy;
            if (near_interest(d, m, decals) && rng_float(rng) < DOG_SNIFF_CHANCE) {
                d->state = DOG_SNIFF; d->t = DOG_SNIFF_MIN + rng_float(rng) * (DOG_SNIFF_MAX - DOG_SNIFF_MIN);
            } else {
                choose_dir(d, m, rng);
                if (map_is_floor(m, d->tx + DX[d->dir], d->ty + DY[d->dir])) { d->tx += DX[d->dir]; d->ty += DY[d->dir]; }
            }
        } else {
            d->x += dx / dist * step; d->y += dy / dist * step;
            d->anim += dt;
        }
    }
    if (d->spr) {
        d->spr->x = d->x; d->spr->y = d->y;
        d->spr->kind = d->state == DOG_SNIFF ? SPR_DOG_SNIFF : (((int)(d->anim * 4.0f)) & 1 ? SPR_DOG_B : SPR_DOG_A);
    }
}
```

`sim/actors/trash.h`:

```c
#ifndef ACTORS_TRASH_H_HEADER
#define ACTORS_TRASH_H_HEADER
#include "map.h"
#include "sprites.h"
#include "rng.h"

#define TRASH_GUST_MIN 4.0f
#define TRASH_GUST_MAX 10.0f
#define TRASH_GUST_LEN 1.2f
#define TRASH_GUST_SPEED 2.0f
#define TRASH_FRICTION 3.0f

enum { LOOSE_PAPER = 0, LOOSE_NEWS = 1 };

typedef struct { float until_next; float left; int dir_sign; } Wind;
typedef struct { float x, y, vx, vy; int kind; float anim; Sprite *spr; } Loose;

void wind_init(Wind *w, Rng *rng);
void wind_step(Wind *w, Rng *rng, float dt);
int  wind_gusting(const Wind *w);
void loose_init(Loose *l, int kind, int tile_x, int tile_y, Sprite *spr);
void loose_step(Loose *l, const Map *m, const Wind *w, float dt);

#endif
```

`sim/actors/trash.c`:

```c
#include "actors/trash.h"
#include "fmath.h"

static float schedule(Rng *rng) { return TRASH_GUST_MIN + rng_float(rng) * (TRASH_GUST_MAX - TRASH_GUST_MIN); }

void wind_init(Wind *w, Rng *rng) { w->until_next = schedule(rng); w->left = 0.0f; w->dir_sign = 1; }

void wind_step(Wind *w, Rng *rng, float dt) {
    if (w->left > 0.0f) { w->left -= dt; if (w->left <= 0.0f) { w->left = 0.0f; w->until_next = schedule(rng); } return; }
    w->until_next -= dt;
    if (w->until_next <= 0.0f) { w->left = TRASH_GUST_LEN; w->dir_sign = rng_range(rng, 0, 1) ? 1 : -1; }
}

int wind_gusting(const Wind *w) { return w->left > 0.0f; }

void loose_init(Loose *l, int kind, int tile_x, int tile_y, Sprite *spr) {
    l->x = (float)tile_x + 0.5f; l->y = (float)tile_y + 0.5f; l->vx = 0.0f; l->vy = 0.0f;
    l->kind = kind; l->anim = 0.0f; l->spr = spr;
    if (spr) { spr->x = l->x; spr->y = l->y; }
}

static void move_axis(float *pos, float *vel, float other, int horizontal, const Map *m, float dt) {
    float next = *pos + *vel * dt;
    float margin = 0.2f;
    float probe = next + (*vel > 0 ? margin : -margin);
    int ok = horizontal ? map_is_floor(m, (int)probe, (int)other) : map_is_floor(m, (int)other, (int)probe);
    if (ok) *pos = next; else *vel = 0.0f;
}

void loose_step(Loose *l, const Map *m, const Wind *w, float dt) {
    if (wind_gusting(w)) {
        int tx = (int)l->x, ty = (int)l->y;
        int open_x = map_is_floor(m, tx + w->dir_sign, ty);
        int open_y = map_is_floor(m, tx, ty + w->dir_sign);
        float target = (float)w->dir_sign * TRASH_GUST_SPEED, k = fm_min(1.0f, 6.0f * dt);
        if (open_x) l->vx += (target - l->vx) * k;
        else if (open_y) l->vy += (target - l->vy) * k;
        else if (open_x || map_is_floor(m, tx - w->dir_sign, ty)) l->vx += (target - l->vx) * k;   /* blocked ahead: push into the wall, move_axis pins it */
    } else {
        float f = 1.0f - TRASH_FRICTION * dt; if (f < 0.0f) f = 0.0f;
        l->vx *= f; l->vy *= f;
        if (fm_abs(l->vx) < 0.01f) l->vx = 0.0f;
        if (fm_abs(l->vy) < 0.01f) l->vy = 0.0f;
    }
    move_axis(&l->x, &l->vx, l->y, 1, m, dt);
    move_axis(&l->y, &l->vy, l->x, 0, m, dt);
    int moving = fm_abs(l->vx) > 0.05f || fm_abs(l->vy) > 0.05f;
    if (moving) l->anim += dt;
    if (l->spr) {
        l->spr->x = l->x; l->spr->y = l->y;
        int b = moving && (((int)(l->anim * 6.0f)) & 1);
        l->spr->kind = l->kind == LOOSE_PAPER ? (b ? SPR_PAPER_B : SPR_PAPER_A) : (b ? SPR_NEWS_B : SPR_NEWS_A);
    }
}
```

`sim/actors/bug.h`:

```c
#ifndef ACTORS_BUG_H_HEADER
#define ACTORS_BUG_H_HEADER
#include "sprites.h"

enum { BUGSTATE_HIDDEN = 0, BUGSTATE_PEEK = 1, BUGSTATE_EXPOSED = 2, BUGSTATE_DEAD = 3 };
#define BUG_SPRITE_SIZE 0.45f

typedef struct { int state; float x, y; Sprite *spr; } HiddenBug;

void bug_place(HiddenBug *b, Sprites *sp, float x, float y);
void bug_set_state(HiddenBug *b, Sprites *sp, int state);

#endif
```

`sim/actors/bug.c`:

```c
#include "actors/bug.h"

void bug_place(HiddenBug *b, Sprites *sp, float x, float y) {
    b->state = BUGSTATE_HIDDEN; b->x = x; b->y = y;
    b->spr = sprites_add(sp, SPR_BUG_HIDDEN, x, y, BUG_SPRITE_SIZE);
}

void bug_set_state(HiddenBug *b, Sprites *sp, int state) {
    b->state = state;
    if (!b->spr) return;
    if (state == BUGSTATE_DEAD) { sprites_remove(sp, b->spr); b->spr = 0; return; }
    b->spr->kind = state == BUGSTATE_HIDDEN ? SPR_BUG_HIDDEN : state == BUGSTATE_PEEK ? SPR_BUG_PEEK : SPR_BUG_EXPOSED;
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`. The gust-count assertion depends on seed `1u` through `rng_range`/`rng_float`; if it lands outside 5..15 gusts in 60 s, the schedule math is wrong (expected mean interval 7 s plus 1.2 s gust → about 7 gusts), not the seed.

- [ ] **Step 5: Commit**

```bash
git add sim/actors test/test_actors.c test/test_main.c
git commit -m "Add dogs, wind-blown trash, and the hidden bug"
```

---

### Task 12: The hunt state machine and the World

**Files:**
- Create: `sim/hunt.h`, `sim/hunt.c`, `sim/world.h`, `sim/world.c`, `test/test_hunt.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces in world: `MAX_DOGS 4`, `MAX_LOOSE 16`, `WORLD_MAX_W 640`, `WORLD_MAX_H 320`, `WORLD_MIN_W 200`, `WORLD_MIN_H 160`; `World {w, h, map, tex, pal, cam, decals, sprites, hud, hunt, dogs[], n_dogs, loose[], n_loose, wind, bug, dust, dust_t, rng, events, time, squashed_total, depth[]}`; `int world_init(world, seed, w, h, events, neon_text)` (0 or -1), `void world_step(world, dt)` (wind, trash, dogs, hunt, camera), `int world_horizon(world)` (`h/2 + pitch + bob`, clamped to `[h/4, 3h/4]`), `void world_render(world, fb)` (sky, walls, floor, sprites, HUD).
- Produces in hunt: `HUNT_LOOK 0, HUNT_FOLLOW, HUNT_APPROACH, HUNT_SMASH, HUNT_CELEBRATE, HUNT_NEW_TRAIL, HUNT_COUNT`; constants `HUNT_LOOK_TIME 1.5f`, `HUNT_LOOK_SWEEP 0.35f`, `HUNT_INSPECT_MEAN 8.0f`, `HUNT_INSPECT_SPREAD 3.0f`, `HUNT_INSPECT_TIME 0.6f`, `HUNT_INSPECT_DIP 12`, `HUNT_PEEK_DIST 2.5f`, `HUNT_EXPOSE_DIST 1.5f`, `HUNT_SMASH_DIST 1.0f`, `HUNT_FACE_TOL 0.2f`, `HUNT_CELEBRATE_TIME 1.0f`, `HUNT_HOP_PX 8`, `HUNT_MIN_STEPS 12`, `HUNT_CULL_BEHIND 6.0f`, `HUNT_DUST_TIME 0.5f`; `Hunt {state, t, path, waypoint, walking_to, hiding, bug_x, bug_y, inspect_timer, inspect_t, inspecting, look_base, cycles}`; `hunt_init(world)`, `hunt_step(world, dt)` (also steps the HUD and reacts to the hit), `hunt_state(world)`, `hunt_cycles(world)`.
- Step order inside `world_step`: wind, loose items, dogs, hunt (which drives the camera's targets, the HUD, the bug, and events), then `camera_step`, then `time += dt`.
- Props (lamps, trash cans) are nudged 0.35 tiles toward an adjacent wall so the camera does not walk through them.

- [ ] **Step 1: Write the failing tests**

`test/test_hunt.c`:

```c
#include "test.h"
#include "world.h"
#include "palette.h"

#define DT (1.0f / 60.0f)
static uint8_t px[640 * 320 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int yellow(Color c) { return c.r > 180 && c.g > 130 && c.b < 110; }

static int run_until_state(World *w, int state, int max_steps) {
    for (int i = 0; i < max_steps; i++) { world_step(w, DT); if (hunt_state(w) == state) return i + 1; }
    return -1;
}

void test_hunt(void) {
    static World w, w2;
    EventQueue ev; events_init(&ev);
    CHECK_EQ(world_init(&w, 5u, 320, 200, &ev, "ROHIT"), 0);
    CHECK_EQ(world_init(&w2, 5u, 100, 200, &ev, "ROHIT"), -1);
    CHECK_EQ(world_init(&w2, 5u, 320, 400, &ev, "ROHIT"), -1);

    /* initial state: a trail is laid to a hiding spot, the bug is hidden there, the camera is at the start */
    CHECK_EQ(hunt_state(&w), HUNT_LOOK);
    CHECK_EQ(hunt_cycles(&w), 0);
    CHECK(w.hunt.path.n - 1 >= HUNT_MIN_STEPS);
    CHECK(w.decals.n > 0);
    CHECK(map_is_hiding_spot(&w.map, w.hunt.hiding.x, w.hunt.hiding.y));
    CHECK_EQ(w.bug.state, BUGSTATE_HIDDEN);
    CHECK(w.bug.spr != NULL);
    CHECK_NEAR(w.cam.x, w.map.start_x + 0.5f, 1e-5);
    CHECK_NEAR(w.cam.y, w.map.start_y + 0.5f, 1e-5);
    CHECK(w.n_dogs == 3);
    CHECK(w.n_loose >= 6);
    int lamps = 0; for (int i = 0; i < MAX_SPRITES; i++) if (w.sprites.s[i].active && w.sprites.s[i].kind == SPR_LAMP) lamps++;
    CHECK(lamps >= 12);

    /* LOOK pans, then FOLLOW walks the trail on floor tiles */
    int steps = run_until_state(&w, HUNT_FOLLOW, 3 * 60);
    CHECK(steps > 60 && steps <= 95);
    int walked = 0, inspected = 0;
    float x0 = w.cam.x, y0 = w.cam.y;
    for (int i = 0; i < 4 * 60 && hunt_state(&w) == HUNT_FOLLOW; i++) {
        world_step(&w, DT);
        CHECK(map_is_floor(&w.map, (int)w.cam.x, (int)w.cam.y));
        if (w.cam.x != x0 || w.cam.y != y0) walked = 1;
        if (w.cam.pitch_px != 0) inspected = 1;
    }
    CHECK(walked);

    /* the first cycle completes with exactly one squash event, within 10 to 120 seconds */
    int total = steps;
    while (hunt_cycles(&w) < 1 && total < 150 * 60) {
        world_step(&w, DT); total++;
        CHECK(map_is_floor(&w.map, (int)w.cam.x, (int)w.cam.y));
        if (w.cam.pitch_px != 0) inspected = 1;
    }
    CHECK_EQ(hunt_cycles(&w), 1);
    CHECK(total >= 10 * 60 && total <= 120 * 60);
    CHECK_EQ(events_count(&ev), 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);
    CHECK_EQ(w.squashed_total, 1);
    CHECK_EQ(hunt_state(&w), HUNT_LOOK);                     /* a fresh trail */
    CHECK(w.hunt.path.n - 1 >= HUNT_MIN_STEPS);
    CHECK_EQ(w.bug.state, BUGSTATE_HIDDEN);
    CHECK(w.bug.spr != NULL);
    CHECK(w.dust != NULL || w.dust_t == 0.0f);

    /* two more cycles: still one event each, an inspect dip happened at some point */
    int t2 = 0;
    while (hunt_cycles(&w) < 3 && t2 < 300 * 60) { world_step(&w, DT); t2++; if (w.cam.pitch_px != 0) inspected = 1; }
    CHECK_EQ(hunt_cycles(&w), 3);
    CHECK_EQ(events_count(&ev), 2);
    CHECK(inspected);
    while (events_pop(&ev)) {}

    /* deterministic: same seed, same first squash step */
    EventQueue ev2; events_init(&ev2);
    static World a, b;
    world_init(&a, 21u, 320, 200, &ev, "ROHIT");
    world_init(&b, 21u, 320, 200, &ev2, "ROHIT");
    int sa = -1, sb = -1;
    for (int i = 0; i < 150 * 60 && (sa < 0 || sb < 0); i++) {
        if (sa < 0) { world_step(&a, DT); if (events_count(&ev)) sa = i; }
        if (sb < 0) { world_step(&b, DT); if (events_count(&ev2)) sb = i; }
    }
    CHECK(sa > 0);
    CHECK_EQ(sa, sb);
    while (events_pop(&ev)) {} while (events_pop(&ev2)) {}

    /* rendering: every quadrant painted, sky at the top, hammer at the bottom centre, horizon in range */
    Framebuffer fb; fb_init(&fb, px, 320, 200);
    draw_clear(&fb, COLOR(0, 0, 0));
    world_init(&w, 5u, 320, 200, &ev, "ROHIT");
    for (int i = 0; i < 120; i++) world_step(&w, DT);
    int hz = world_horizon(&w);
    CHECK(hz >= 50 && hz <= 150);
    world_render(&w, &fb);
    int q[4] = { 0, 0, 0, 0 };
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++)
        if (!same(fb_get(&fb, x, y), COLOR(0, 0, 0))) q[(y >= 100) * 2 + (x >= 160)]++;
    for (int k = 0; k < 4; k++) CHECK(q[k] > 10000);
    int hammer = 0;
    for (int y = 130; y < 200; y++) for (int x = 120; x < 200; x++) if (same(fb_get(&fb, x, y), PALETTE_NIGHT[COL_HAMMER])) hammer++;
    CHECK(hammer > 100);
    int yellow_floor = 0;
    for (int y = hz + 1; y < 200; y++) for (int x = 0; x < 320; x++) if (yellow(fb_get(&fb, x, y)) && !same(fb_get(&fb, x, y), PALETTE_NIGHT[COL_HAMMER])) yellow_floor++;
    CHECK(yellow_floor > 20);                                      /* footprints ahead */

    /* the largest framebuffer works too */
    Framebuffer big; fb_init(&big, px, 640, 320);
    CHECK_EQ(world_init(&w, 5u, 640, 320, &ev, "ROHIT"), 0);
    world_step(&w, DT);
    world_render(&w, &big);
    CHECK(!same(fb_get(&big, 320, 160), COLOR(0, 0, 0)));
}
```

Add `void test_hunt(void);` and `RUN(test_hunt);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `world.h` not found.

- [ ] **Step 3: Implement**

`sim/hunt.h`:

```c
#ifndef HUNT_H_HEADER
#define HUNT_H_HEADER
#include "map.h"

enum { HUNT_LOOK = 0, HUNT_FOLLOW, HUNT_APPROACH, HUNT_SMASH, HUNT_CELEBRATE, HUNT_NEW_TRAIL, HUNT_COUNT };

#define HUNT_LOOK_TIME 1.5f
#define HUNT_LOOK_SWEEP 0.35f
#define HUNT_INSPECT_MEAN 8.0f
#define HUNT_INSPECT_SPREAD 3.0f
#define HUNT_INSPECT_TIME 0.6f
#define HUNT_INSPECT_DIP 12
#define HUNT_PEEK_DIST 2.5f
#define HUNT_EXPOSE_DIST 1.5f
#define HUNT_SMASH_DIST 1.0f
#define HUNT_FACE_TOL 0.2f
#define HUNT_CELEBRATE_TIME 1.0f
#define HUNT_HOP_PX 8
#define HUNT_MIN_STEPS 12
#define HUNT_CULL_BEHIND 6.0f
#define HUNT_DUST_TIME 0.5f

typedef struct {
    int   state;
    float t;
    Path  path;
    int   waypoint;       /* next waypoint to walk to */
    int   walking_to;     /* waypoint currently being walked to, -1 none */
    Tile  hiding;
    float bug_x, bug_y;
    float inspect_timer, inspect_t;
    int   inspecting;
    float look_base;
    int   cycles;
} Hunt;

struct World;
void hunt_init(struct World *w);
void hunt_step(struct World *w, float dt);
int  hunt_state(const struct World *w);
int  hunt_cycles(const struct World *w);

#endif
```

`sim/world.h`:

```c
#ifndef WORLD_H_HEADER
#define WORLD_H_HEADER
#include <stdint.h>
#include "rng.h"
#include "events.h"
#include "map.h"
#include "textures.h"
#include "camera.h"
#include "decals.h"
#include "sprites.h"
#include "hud.h"
#include "hunt.h"
#include "actors/dog.h"
#include "actors/trash.h"
#include "actors/bug.h"

#define MAX_DOGS 4
#define MAX_LOOSE 16
#define WORLD_MAX_W 640
#define WORLD_MAX_H 320
#define WORLD_MIN_W 200
#define WORLD_MIN_H 160

typedef struct World {
    int w, h;
    Map map;
    Textures tex;
    const Color *pal;
    Camera cam;
    Decals decals;
    Sprites sprites;
    Hud hud;
    Hunt hunt;
    Dog dogs[MAX_DOGS]; int n_dogs;
    Loose loose[MAX_LOOSE]; int n_loose;
    Wind wind;
    HiddenBug bug;
    Sprite *dust; float dust_t;
    Rng rng;
    EventQueue *events;
    float time;
    int squashed_total;
    float depth[WORLD_MAX_W];
} World;

int  world_init(World *w, uint32_t seed, int width, int height, EventQueue *events, const char *neon_text);
void world_step(World *w, float dt);
int  world_horizon(const World *w);
void world_render(World *w, Framebuffer *fb);

#endif
```

`sim/hunt.c`:

```c
#include "hunt.h"
#include "world.h"
#include "trig.h"
#include "fmath.h"

static float wp_x(const Hunt *h, int i) { return (float)h->path.t[i].x + 0.5f; }
static float wp_y(const Hunt *h, int i) { return (float)h->path.t[i].y + 0.5f; }

static void begin(Hunt *h, int state) { h->state = state; h->t = 0.0f; }

static float next_inspect(World *w) {
    return HUNT_INSPECT_MEAN + (rng_float(&w->rng) * 2.0f - 1.0f) * HUNT_INSPECT_SPREAD;
}

static void new_trail(World *w) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    Tile from = { (int)c->x, (int)c->y };
    if (!map_pick_hiding_spot(&w->map, &w->rng, from, HUNT_MIN_STEPS, &h->hiding)) h->hiding = from;
    if (!map_bfs(&w->map, from, h->hiding, &h->path) || h->path.n < 1) { h->path.n = 1; h->path.t[0] = from; }
    decals_lay_trail(&w->decals, &h->path);
    decals_set_head_waypoint(&w->decals, 0);
    h->waypoint = 1; h->walking_to = -1;

    float bx = (float)h->hiding.x + 0.5f, by = (float)h->hiding.y + 0.5f;
    static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++) {
        int nx = h->hiding.x + DX[k], ny = h->hiding.y + DY[k];
        if (nx < 0 || ny < 0 || nx >= w->map.w || ny >= w->map.h) continue;
        int dumpster = w->map.cell[ny][nx] == CELL_DUMPSTER;
        int trash = w->map.cell[ny][nx] == CELL_FLOOR && w->map.prop[ny][nx] == PROP_TRASH;
        if (dumpster || trash) { bx += 0.3f * (float)DX[k]; by += 0.3f * (float)DY[k]; break; }
    }
    h->bug_x = bx; h->bug_y = by;
    if (w->bug.spr) bug_set_state(&w->bug, &w->sprites, BUGSTATE_DEAD);
    bug_place(&w->bug, &w->sprites, bx, by);

    h->inspect_timer = next_inspect(w); h->inspecting = 0; h->inspect_t = 0.0f;
    h->look_base = c->angle;
    camera_stop(c);
    begin(h, HUNT_LOOK);
}

void hunt_init(World *w) { w->hunt.cycles = 0; w->dust = 0; w->dust_t = 0.0f; new_trail(w); }

int hunt_state(const World *w) { return w->hunt.state; }
int hunt_cycles(const World *w) { return w->hunt.cycles; }

/* Advance along the path: register arrival, then start the next leg. */
static void follow_path(World *w) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    if (c->walking) return;
    if (h->walking_to >= 0) {
        h->waypoint = h->walking_to + 1;
        decals_set_head_waypoint(&w->decals, h->walking_to);
        h->walking_to = -1;
    }
    if (h->waypoint < h->path.n) { camera_walk_to(c, wp_x(h, h->waypoint), wp_y(h, h->waypoint)); h->walking_to = h->waypoint; }
}

static void smash_hit(World *w) {
    Hunt *h = &w->hunt;
    if (w->dust) sprites_remove(&w->sprites, w->dust);
    w->dust = sprites_add(&w->sprites, SPR_DUST_A, h->bug_x, h->bug_y, 0.6f);
    w->dust_t = 0.0f;
    bug_set_state(&w->bug, &w->sprites, BUGSTATE_DEAD);
    hud_flash_plus(&w->hud);
    w->squashed_total++;
    if (w->events) events_push(w->events, event_pack(STAGE_BUGS, EV_BUG_SQUASHED, 0, 0));
}

void hunt_step(World *w, float dt) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    h->t += dt;
    int hit = hud_step(&w->hud, dt);

    switch (h->state) {
    case HUNT_LOOK:
        if (h->t < 0.5f) camera_turn_to(c, h->look_base - HUNT_LOOK_SWEEP);
        else if (h->t < 1.0f) camera_turn_to(c, h->look_base + HUNT_LOOK_SWEEP);
        else if (h->path.n > 1) camera_face(c, wp_x(h, 1), wp_y(h, 1));
        if (h->t >= HUNT_LOOK_TIME) begin(h, HUNT_FOLLOW);
        break;

    case HUNT_FOLLOW:
        if (h->inspecting) {
            h->inspect_t += dt;
            c->pitch_px = (int)((float)HUNT_INSPECT_DIP * trig_sin(TRIG_PI * h->inspect_t / HUNT_INSPECT_TIME));
            if (h->inspect_t >= HUNT_INSPECT_TIME) { h->inspecting = 0; c->pitch_px = 0; c->walking = 1; }
            break;
        }
        follow_path(w);
        h->inspect_timer -= dt;
        if (h->inspect_timer <= 0.0f && c->walking) {
            h->inspecting = 1; h->inspect_t = 0.0f; c->walking = 0;
            h->inspect_timer = next_inspect(w);
        }
        if (camera_dist(c, h->bug_x, h->bug_y) < HUNT_PEEK_DIST) {
            bug_set_state(&w->bug, &w->sprites, BUGSTATE_PEEK);
            begin(h, HUNT_APPROACH);
        }
        break;

    case HUNT_APPROACH: {
        follow_path(w);
        if (h->waypoint >= h->path.n && !c->walking && camera_dist(c, h->bug_x, h->bug_y) >= HUNT_SMASH_DIST) {
            camera_walk_to(c, (float)h->hiding.x + 0.5f, (float)h->hiding.y + 0.5f);
            h->walking_to = h->path.n - 1;
        }
        camera_face(c, h->bug_x, h->bug_y);
        float dist = camera_dist(c, h->bug_x, h->bug_y);
        if (dist < HUNT_EXPOSE_DIST && w->bug.state == BUGSTATE_PEEK) bug_set_state(&w->bug, &w->sprites, BUGSTATE_EXPOSED);
        if (dist < HUNT_SMASH_DIST && camera_facing(c, HUNT_FACE_TOL)) {
            camera_stop(c); h->walking_to = -1;
            hud_start_swing(&w->hud);
            begin(h, HUNT_SMASH);
        }
        break;
    }

    case HUNT_SMASH:
        if (hit) smash_hit(w);
        if (h->t >= HUD_SWING_TIME) begin(h, HUNT_CELEBRATE);
        break;

    case HUNT_CELEBRATE:
        c->pitch_px = h->t < 0.5f ? -(int)((float)HUNT_HOP_PX * trig_sin(TRIG_PI * h->t / 0.5f)) : 0;
        if (h->t >= HUNT_CELEBRATE_TIME) { c->pitch_px = 0; h->cycles++; begin(h, HUNT_NEW_TRAIL); }
        break;

    case HUNT_NEW_TRAIL:
    default:
        new_trail(w);
        break;
    }

    decals_cull_behind(&w->decals, c->x, c->y, HUNT_CULL_BEHIND);
    if (w->dust) {
        w->dust_t += dt;
        w->dust->kind = w->dust_t < 0.25f ? SPR_DUST_A : SPR_DUST_B;
        if (w->dust_t >= HUNT_DUST_TIME) { sprites_remove(&w->sprites, w->dust); w->dust = 0; }
    }
}
```

`sim/world.c`:

```c
#include "world.h"
#include "palette.h"
#include "raycast.h"

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };

/* Nudge a prop from the tile centre toward an adjacent wall so the camera does not walk through it. */
static void prop_position(const Map *m, int x, int y, float *px, float *py) {
    *px = (float)x + 0.5f; *py = (float)y + 0.5f;
    for (int k = 0; k < 4; k++) {
        if (!map_is_floor(m, x + DX[k], y + DY[k])) { *px += 0.35f * (float)DX[k]; *py += 0.35f * (float)DY[k]; return; }
    }
}

int world_init(World *w, uint32_t seed, int width, int height, EventQueue *events, const char *neon_text) {
    if (width < WORLD_MIN_W || width > WORLD_MAX_W || height < WORLD_MIN_H || height > WORLD_MAX_H) return -1;
    w->w = width; w->h = height; w->events = events; w->time = 0.0f; w->squashed_total = 0;
    w->pal = PALETTE_NIGHT;
    rng_seed(&w->rng, seed);
    if (map_parse(&w->map, CITY_MAP, MAP_W, MAP_H) != 0) return -1;
    textures_generate(&w->tex, &w->rng, w->pal, neon_text);
    camera_init(&w->cam, (float)w->map.start_x + 0.5f, (float)w->map.start_y + 0.5f, 0.0f);
    sprites_clear(&w->sprites);
    decals_clear(&w->decals);
    hud_init(&w->hud);
    wind_init(&w->wind, &w->rng);
    w->n_dogs = 0; w->n_loose = 0; w->bug.spr = 0; w->bug.state = BUGSTATE_DEAD; w->dust = 0; w->dust_t = 0.0f;

    for (int i = 0; i < w->map.n_spawns; i++) {
        const Spawn *s = &w->map.spawns[i];
        float px, py;
        switch (s->kind) {
        case PROP_LAMP:  prop_position(&w->map, s->x, s->y, &px, &py); sprites_add(&w->sprites, SPR_LAMP, px, py, 2.2f); break;
        case PROP_TRASH: prop_position(&w->map, s->x, s->y, &px, &py); sprites_add(&w->sprites, SPR_TRASHCAN, px, py, 0.8f); break;
        case PROP_PAPER:
        case PROP_NEWSPAPER:
            if (w->n_loose < MAX_LOOSE) {
                int kind = s->kind == PROP_PAPER ? LOOSE_PAPER : LOOSE_NEWS;
                Sprite *sp = sprites_add(&w->sprites, kind == LOOSE_PAPER ? SPR_PAPER_A : SPR_NEWS_A, (float)s->x + 0.5f, (float)s->y + 0.5f, kind == LOOSE_PAPER ? 0.3f : 0.4f);
                loose_init(&w->loose[w->n_loose++], kind, s->x, s->y, sp);
            }
            break;
        case PROP_DOG:
            if (w->n_dogs < MAX_DOGS) {
                Sprite *sp = sprites_add(&w->sprites, SPR_DOG_A, (float)s->x + 0.5f, (float)s->y + 0.5f, 0.6f);
                dog_init(&w->dogs[w->n_dogs++], s->x, s->y, sp);
            }
            break;
        default: break;
        }
    }
    hunt_init(w);
    return 0;
}

void world_step(World *w, float dt) {
    wind_step(&w->wind, &w->rng, dt);
    for (int i = 0; i < w->n_loose; i++) loose_step(&w->loose[i], &w->map, &w->wind, dt);
    for (int i = 0; i < w->n_dogs; i++) dog_step(&w->dogs[i], &w->map, &w->decals, &w->rng, dt);
    hunt_step(w, dt);
    camera_step(&w->cam, dt);
    w->time += dt;
}

int world_horizon(const World *w) {
    int hz = w->h / 2 + w->cam.pitch_px + camera_bob_px(&w->cam);
    int lo = w->h / 4, hi = 3 * w->h / 4;
    return hz < lo ? lo : hz > hi ? hi : hz;
}

void world_render(World *w, Framebuffer *fb) {
    int horizon = world_horizon(w);
    float proj = camera_proj(fb->w);
    raycast_sky(&w->tex, &w->cam, proj, fb, horizon);
    raycast_walls(&w->map, &w->tex, w->pal, &w->cam, proj, fb, horizon, w->depth);
    raycast_floor(&w->map, &w->tex, w->pal, &w->decals, &w->cam, proj, fb, horizon, w->depth);
    sprites_draw(&w->sprites, &w->cam, &w->map, &w->tex, w->pal, fb, w->depth, horizon, proj);
    hud_draw(&w->hud, &w->tex, w->pal, fb, camera_bob_px(&w->cam));
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`. If the first cycle never reaches SMASH, print `hunt_state`, `waypoint`, `path.n`, and the camera position every second in a scratch test to see where it stalls; the usual culprits are `walking_to` bookkeeping after an inspect pause and the bug sitting behind a wall corner outside `HUNT_SMASH_DIST`.

- [ ] **Step 5: Commit**

```bash
git add sim/hunt.h sim/hunt.c sim/world.h sim/world.c test/test_hunt.c test/test_main.c
git commit -m "Add the hunt state machine and the world that runs it"
```

---

### Task 13: Exports, native export tests, wasm build, smoke test

**Files:**
- Create: `sim/sim.h`, `sim/sim.c`, `test/test_sim.c`
- Rewrite: `test/wasm_smoke.mjs`
- Modify: `test/test_main.c`
- Rebuild and commit: `sim.wasm`

**Interfaces:**
- Produces the eight exports: `int sim_init(uint32_t seed, int w, int h)` (0 or -1), `void sim_update(int elapsed_ms)`, `void sim_render(void)`, `uint8_t *sim_framebuffer(void)`, `int sim_framebuffer_len(void)` (`w*h*4`, 0 before init), `uint32_t sim_poll_event(void)`, `void sim_render_static(void)` (re-inits with seed 7, runs 2.5 s, sets the bug to peek, drains events, renders once), plus `memory`.
- The neon text is fixed in C (`ROHIT`); no strings cross the boundary.

- [ ] **Step 1: Write the failing tests**

`test/test_sim.c`:

```c
#include "test.h"
#include "sim.h"
#include "palette.h"
#include "world.h"

static int same(Color a, const Color *b) { return a.r == b->r && a.g == b->g && a.b == b->b; }
static int yellow(const uint8_t *p) { return p[0] > 180 && p[1] > 130 && p[2] < 110; }

static int painted(const uint8_t *px, int w, int h) {
    int n = 0;
    for (int i = 0; i < w * h; i++) if (px[i * 4] || px[i * 4 + 1] || px[i * 4 + 2]) n++;
    return n;
}

static int has_color(const uint8_t *px, int w, int h, const Color *c) {
    for (int i = 0; i < w * h; i++) if (px[i * 4] == c->r && px[i * 4 + 1] == c->g && px[i * 4 + 2] == c->b) return 1;
    return 0;
}

void test_sim(void) {
    CHECK_EQ(sim_framebuffer_len(), 0);
    CHECK_EQ(sim_init(1u, 100, 200), -1);
    CHECK_EQ(sim_init(1u, 700, 200), -1);
    CHECK_EQ(sim_init(1u, 356, 100), -1);
    CHECK_EQ(sim_init(1u, 356, 400), -1);
    CHECK_EQ(sim_init(1234u, 356, 200), 0);
    CHECK_EQ(sim_framebuffer_len(), 356 * 200 * 4);
    CHECK(sim_framebuffer() != NULL);
    CHECK_EQ(sim_poll_event(), 0u);

    /* a first frame paints everything */
    sim_update(17);
    sim_render();
    const uint8_t *px = sim_framebuffer();
    CHECK(painted(px, 356, 200) > 356 * 200 * 9 / 10);
    CHECK(has_color(px, 356, 200, &PALETTE_NIGHT[COL_HAMMER]));

    /* the hunt produces a squash within two minutes of simulated time */
    int squashed = 0, frames = 0;
    while (!squashed && frames < 120 * 1000 / 17) {
        sim_update(17); frames++;
        for (uint32_t e = sim_poll_event(); e != 0; e = sim_poll_event())
            if (event_type(e) == EV_BUG_SQUASHED && event_stage(e) == STAGE_BUGS) squashed++;
    }
    CHECK_EQ(squashed, 1);
    CHECK(frames * 17 >= 10 * 1000);

    /* the update clamps: a minute of elapsed time is at most 250 ms of simulation */
    CHECK_EQ(sim_init(9u, 356, 200), 0);
    sim_update(60000);
    int events_after_jump = 0;
    for (uint32_t e = sim_poll_event(); e != 0; e = sim_poll_event()) events_after_jump++;
    CHECK_EQ(events_after_jump, 0);

    /* static frame: footprints and the hammer, deterministic */
    sim_render_static();
    px = sim_framebuffer();
    int yellow_px = 0;
    for (int i = 0; i < 356 * 200; i++) if (yellow(px + i * 4) && !same((Color){ px[i*4], px[i*4+1], px[i*4+2], 255 }, &PALETTE_NIGHT[COL_HAMMER])) yellow_px++;
    CHECK(yellow_px > 20);
    CHECK(has_color(px, 356, 200, &PALETTE_NIGHT[COL_HAMMER]));
    CHECK_EQ(sim_poll_event(), 0u);
    static uint8_t copy[640 * 320 * 4];
    for (int i = 0; i < 356 * 200 * 4; i++) copy[i] = px[i];
    sim_render_static();
    int diff = 0;
    for (int i = 0; i < 356 * 200 * 4; i++) if (copy[i] != sim_framebuffer()[i]) diff++;
    CHECK_EQ(diff, 0);

    /* extremes */
    CHECK_EQ(sim_init(3u, 200, 160), 0);
    CHECK_EQ(sim_framebuffer_len(), 200 * 160 * 4);
    sim_render();
    CHECK(painted(sim_framebuffer(), 200, 160) > 200 * 160 * 9 / 10);
    CHECK_EQ(sim_init(3u, 640, 320), 0);
    CHECK_EQ(sim_framebuffer_len(), 640 * 320 * 4);
    sim_update(17); sim_render();
    CHECK(painted(sim_framebuffer(), 640, 320) > 640 * 320 * 9 / 10);
}
```

Add `void test_sim(void);` and `RUN(test_sim);` to `test/test_main.c`.

`test/wasm_smoke.mjs` (replace the whole file):

```js
import { readFile } from 'node:fs/promises';

const REQUIRED = ['memory', 'sim_init', 'sim_update', 'sim_render', 'sim_framebuffer', 'sim_framebuffer_len',
  'sim_poll_event', 'sim_render_static'];
const MAX_BYTES = 64 * 1024;

const bytes = await readFile(new URL('../sim.wasm', import.meta.url));
const mod = await WebAssembly.compile(bytes);
const imports = WebAssembly.Module.imports(mod);
if (imports.length) throw new Error('module has imports: ' + JSON.stringify(imports));
const names = WebAssembly.Module.exports(mod).map(e => e.name);
for (const n of REQUIRED) if (!names.includes(n)) throw new Error('missing export ' + n);
if (names.length !== REQUIRED.length) throw new Error('unexpected exports: ' + names.filter(n => !REQUIRED.includes(n)).join(', '));
if (bytes.length > MAX_BYTES) throw new Error(`sim.wasm is ${bytes.length} bytes, budget is ${MAX_BYTES}`);

const { exports: ex } = await WebAssembly.instantiate(mod, {});
if (ex.sim_init(1234, 356, 200) !== 0) throw new Error('sim_init rejected valid sizes');
if (ex.sim_init(1234, 356, 900) !== -1) throw new Error('sim_init accepted a bad height');
ex.sim_init(1234, 356, 200);
for (let i = 0; i < 60; i++) ex.sim_update(17);
ex.sim_render();
const len = ex.sim_framebuffer_len();
if (len !== 356 * 200 * 4) throw new Error('bad framebuffer length ' + len);
const px = new Uint8Array(ex.memory.buffer, ex.sim_framebuffer(), len);
let painted = 0;
for (let i = 0; i < len; i += 4) if ((px[i] | px[i + 1] | px[i + 2]) && px[i + 3] === 255) painted++;
if (painted < 356 * 200 * 0.9) throw new Error('framebuffer looks empty: ' + painted + ' painted pixels');

let squashes = 0, ms = 0;
while (!squashes && ms < 120000) {
  ex.sim_update(17); ms += 17;
  for (let e = ex.sim_poll_event(); e !== 0; e = ex.sim_poll_event()) if ((e >>> 24) === 3) squashes++;
}
if (!squashes) throw new Error('no squash within 120 s of simulated time');

ex.sim_render_static();
console.log(`ok: ${bytes.length} bytes, ${names.length} exports, ${painted} painted pixels, first squash at ${(ms / 1000).toFixed(1)} s`);
```

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `sim.h` not found. Run: `make smoke` → fails (the module has no exports yet).

- [ ] **Step 3: Implement**

`sim/sim.h`:

```c
#ifndef SIM_H_HEADER
#define SIM_H_HEADER
#include <stdint.h>
#include "events.h"

int      sim_init(uint32_t seed, int w, int h);
void     sim_update(int elapsed_ms);
void     sim_render(void);
uint8_t *sim_framebuffer(void);
int      sim_framebuffer_len(void);
uint32_t sim_poll_event(void);
void     sim_render_static(void);

#endif
```

`sim/sim.c`:

```c
#include "export.h"
#include "sim.h"
#include "stage.h"
#include "world.h"
#include "palette.h"

#define STATIC_SEED 7u
#define STATIC_WARMUP_STEPS 150      /* 2.5 s: LOOK is over and the walk has begun */

static uint8_t g_fb_px[WORLD_MAX_W * WORLD_MAX_H * 4];
static World g_world;
static EventQueue g_events;
static Stage g_stage;
static int g_ready;
static const char NEON_TEXT[] = "ROHIT";

static void hunt_stage_step(Stage *s, float dt) { world_step((World *)s->state, dt); }
static void hunt_stage_render(Stage *s) { world_render((World *)s->state, &s->fb); }

SIM_EXPORT("sim_init")
int sim_init(uint32_t seed, int w, int h) {
    if (w < WORLD_MIN_W || w > WORLD_MAX_W || h < WORLD_MIN_H || h > WORLD_MAX_H) return -1;
    g_ready = 0;
    events_init(&g_events);
    if (world_init(&g_world, seed, w, h, &g_events, NEON_TEXT) != 0) return -1;
    stage_init(&g_stage, g_fb_px, w, h, PALETTE_NIGHT, hunt_stage_step, hunt_stage_render, &g_world);
    g_ready = 1;
    return 0;
}

SIM_EXPORT("sim_update")
void sim_update(int elapsed_ms) { if (g_ready) stage_update(&g_stage, elapsed_ms); }

SIM_EXPORT("sim_render")
void sim_render(void) { if (g_ready) stage_render(&g_stage); }

SIM_EXPORT("sim_framebuffer")
uint8_t *sim_framebuffer(void) { return g_fb_px; }

SIM_EXPORT("sim_framebuffer_len")
int sim_framebuffer_len(void) { return g_ready ? g_stage.fb.w * g_stage.fb.h * 4 : 0; }

SIM_EXPORT("sim_poll_event")
uint32_t sim_poll_event(void) { return events_pop(&g_events); }

SIM_EXPORT("sim_render_static")
void sim_render_static(void) {
    if (!g_ready) return;
    int w = g_stage.fb.w, h = g_stage.fb.h;
    events_init(&g_events);
    world_init(&g_world, STATIC_SEED, w, h, &g_events, NEON_TEXT);
    for (int i = 0; i < STATIC_WARMUP_STEPS; i++) world_step(&g_world, STAGE_STEP);
    bug_set_state(&g_world.bug, &g_world.sprites, BUGSTATE_PEEK);
    events_init(&g_events);
    stage_render(&g_stage);
}
```

- [ ] **Step 4: Run tests, build, smoke**

Run: `make test` → `0 failures`, zero warnings.
Run: `make clean && make sim.wasm && make smoke` → `ok: <bytes> bytes, 8 exports, ...` with bytes under 65536. Run the build twice and confirm `shasum sim.wasm` is identical.

- [ ] **Step 5: Commit**

```bash
git add sim/sim.h sim/sim.c test/test_sim.c test/test_main.c test/wasm_smoke.mjs sim.wasm
git commit -m "Add the hunt-stage exports and rebuild sim.wasm with the smoke test"
```

---

### Task 14: Page and shim rework

**Files:**
- Rewrite: `index.html`, `styles.css`, `shim.js`

**Interfaces:**
- DOM contract: `canvas#scene` (fixed, full viewport, behind everything, `aria-hidden="true" role="presentation"`), `section#hero.hero` inside `main.stage`, three `article.case-file` (plain text, no `data-vignette`, no `tabindex`), `span#squashed`, `<link rel="icon" href="data:,">`, `<script type="module" src="shim.js">`. CSS class `no-sim` on `body` when the simulation is unavailable.
- Shim behaviour per spec sections 2, 8, 11, 13, 14: measure the viewport, pick `scale = max(1, round(backingH / 200))`, logical `w = clamp(ceil(backingW/scale), 200, 640)` and `h = clamp(ceil(backingH/scale), 160, 320)`, call `sim_init(seed, w, h)` only when `w` or `h` change, blit the framebuffer scaled to cover, drain squash events into the counter, static mode for reduced motion, mobile, or `?motion=reduce`, `?seed=N` seeds the simulation, resize debounced 200 ms, every callback guarded into `fail()`.

- [ ] **Step 1: Write `index.html`**

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Rohit Kumar</title>
  <meta name="description" content="[One-line description for search results]">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" href="styles.css">
</head>
<body>
  <canvas id="scene" aria-hidden="true" role="presentation"></canvas>

  <main class="stage">
    <section id="hero" class="hero">
      <header>
        <h1>Rohit Kumar</h1>
        <p class="title">[Title line: role, years, the stack you want to be hired for]</p>
        <p class="pitch">[One sentence. Example tone: I fix the bugs that only happen in production, for one customer, on Fridays.]</p>
      </header>

      <ul class="case-files" aria-label="Case files">
        <li>
          <article class="case-file">
            <span class="tag">Race condition</span>
            <p class="symptom">[Symptom: what users saw, one line]</p>
            <p class="fix">[Fix: what you changed, one line]</p>
          </article>
        </li>
        <li>
          <article class="case-file">
            <span class="tag">Heisenbug</span>
            <p class="symptom">[Symptom: the bug that disappeared under observation]</p>
            <p class="fix">[Fix: how you tracked it down]</p>
          </article>
        </li>
        <li>
          <article class="case-file">
            <span class="tag">Regression</span>
            <p class="symptom">[Symptom: the bug that kept coming back]</p>
            <p class="fix">[Fix: the test or guard that made it stay fixed]</p>
          </article>
        </li>
      </ul>

      <nav class="contact" aria-label="Contact">
        <a href="mailto:[you@example.com]">Email</a>
        <a href="https://github.com/0x1DKFA" rel="me">GitHub</a>
        <a href="resume.pdf">Resume</a>
      </nav>

      <p class="counter">bugs squashed this visit: <span id="squashed">0</span></p>
      <p class="credit">Background: C compiled to WebAssembly, no framework, no art assets. <a href="https://github.com/0x1DKFA/portfolio">Source</a></p>
    </section>
  </main>

  <section class="below" id="more">
    <h2>More</h2>
    <p>[Experience and skills go here later.]</p>
  </section>

  <script type="module" src="shim.js"></script>
</body>
</html>
```

- [ ] **Step 2: Write `styles.css`**

```css
:root {
  --bg: #0a0c18;
  --card: rgba(27, 33, 64, 0.92);
  --line: #2a3160;
  --text: #dce1f0;
  --muted: #98a0c0;
  --accent: #fac83c;
}

* { box-sizing: border-box; }

html, body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  line-height: 1.5;
}

#scene {
  position: fixed;
  inset: 0;
  z-index: 0;
  display: block;
  width: 100vw;
  height: 100vh;
  background: var(--bg);
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.stage {
  position: relative;
  z-index: 1;
  display: flex;
  align-items: center;
  min-height: 100vh;
  padding: 6vh 5vw;
}

.hero {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  width: clamp(320px, 34vw, 520px);
  max-height: 88vh;
  overflow: auto;
  padding: 1.75rem 1.5rem;
  background: var(--card);
  border-radius: 8px;
  box-shadow: 0 0 0 1px var(--line), 0 20px 60px rgba(0, 0, 0, 0.55);
}

h1 { margin: 0; font-size: 1.75rem; line-height: 1.2; }
.title { margin: 0.25rem 0 0; color: var(--muted); }
.pitch { margin: 0.6rem 0 0; font-size: 1.05rem; }

.case-files { list-style: none; margin: 0; padding: 0; display: grid; gap: 0.6rem; }
.case-file { padding: 0.7rem 0.85rem; border: 1px solid var(--line); border-radius: 6px; }
.tag { display: inline-block; margin-bottom: 0.3rem; font-size: 0.7rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--accent); }
.case-file p { margin: 0.1rem 0; font-size: 0.95rem; }
.fix { color: var(--muted); }

.contact { display: flex; flex-wrap: wrap; gap: 1.1rem; }
.contact a { color: var(--text); }

.counter { margin: 0; font-size: 0.85rem; color: var(--muted); }
.credit { margin: 0; font-size: 0.72rem; color: var(--muted); }
.credit a { color: var(--muted); }

.below {
  position: relative;
  z-index: 1;
  max-width: 60rem;
  margin: 0 auto;
  padding: 3rem 2.5rem;
  background: var(--bg);
}

body.no-sim #scene { background: var(--bg); }   /* set by shim.js when the simulation cannot start */

@media (max-width: 1023.98px) {
  .stage { align-items: flex-start; padding: 4vh 4vw; }
  .hero { width: 100%; max-height: none; overflow: visible; }
}
```

- [ ] **Step 3: Write `shim.js`**

```js
// shim.js — the only code on the page that touches the DOM.
// The simulation lives in sim.wasm; everything crossing the boundary is an integer or a pointer.
// Debug query parameters: ?seed=N fixes the simulation seed; ?motion=reduce forces the static frame.

const LOGICAL_H = 200, MIN_W = 200, MAX_W = 640, MIN_H = 160, MAX_H = 320;
const EV_SQUASHED = 3;
const BG = '#0a0c18';

const params = new URLSearchParams(location.search);
const desktop = matchMedia('(min-width: 1024px)');
const reduced = matchMedia('(prefers-reduced-motion: reduce)');
const staticMode = () => reduced.matches || !desktop.matches || params.get('motion') === 'reduce';
const canvas = document.getElementById('scene');
const counterEl = document.getElementById('squashed');

let sim = null, geom = null, off = null, raf = 0, last = 0, squashed = 0, resizeTimer = 0;

function fail(err) {
  console.warn('alley hunt disabled:', err);
  stopLoop();
  document.body.classList.add('no-sim');
}
const guard = (fn) => (...args) => { try { return fn(...args); } catch (err) { fail(err); } };

async function loadWasm() {
  const url = new URL('sim.wasm', import.meta.url);
  try {
    return (await WebAssembly.instantiateStreaming(fetch(url), {})).instance.exports;
  } catch {
    const buf = await (await fetch(url)).arrayBuffer();           // wrong MIME type, older host
    return (await WebAssembly.instantiate(buf, {})).instance.exports;
  }
}

function seed() {
  const s = Number(params.get('seed'));
  if (Number.isFinite(s) && s > 0) return s >>> 0;
  return (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0;
}

function measure() {
  const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
  const bw = Math.max(1, Math.round(innerWidth * dpr)), bh = Math.max(1, Math.round(innerHeight * dpr));
  const scale = Math.max(1, Math.round(bh / LOGICAL_H));
  const w = Math.min(MAX_W, Math.max(MIN_W, Math.ceil(bw / scale)));
  const h = Math.min(MAX_H, Math.max(MIN_H, Math.ceil(bh / scale)));
  return { bw, bh, scale, w, h };
}

function init() {
  const g = measure();
  canvas.width = g.bw; canvas.height = g.bh;
  if (!geom || geom.w !== g.w || geom.h !== g.h) {        // re-init only when the logical size changes
    if (sim.sim_init(seed(), g.w, g.h) !== 0) throw new Error('sim_init rejected ' + JSON.stringify(g));
    off = document.createElement('canvas'); off.width = g.w; off.height = g.h;
  }
  geom = g;
}

function blit() {
  const px = new Uint8ClampedArray(sim.memory.buffer, sim.sim_framebuffer(), sim.sim_framebuffer_len());
  off.getContext('2d').putImageData(new ImageData(px, geom.w, geom.h), 0, 0);
  const ctx = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = BG;
  ctx.fillRect(0, 0, geom.bw, geom.bh);
  ctx.drawImage(off, 0, 0, geom.w, geom.h, 0, 0, geom.w * geom.scale, geom.h * geom.scale);   // covers; overflow is cropped
}

function drainEvents() {
  for (let e = sim.sim_poll_event(); e !== 0; e = sim.sim_poll_event())
    if ((e >>> 24) === EV_SQUASHED) counterEl.textContent = String(++squashed);
}

function frame(now) {
  raf = requestAnimationFrame(frame);
  try {
    const elapsed = last ? now - last : 16;
    last = now;
    sim.sim_update(Math.round(elapsed));
    sim.sim_render();
    blit();
    drainEvents();
  } catch (err) { fail(err); }
}

function startLoop() { if (!raf) { last = 0; raf = requestAnimationFrame(frame); } }
function stopLoop() { if (raf) cancelAnimationFrame(raf); raf = 0; }
function renderStatic() { sim.sim_render_static(); blit(); drainEvents(); }

function applyMode() {
  stopLoop();
  init();
  if (staticMode()) renderStatic(); else startLoop();
}

async function main() {
  sim = await loadWasm();
  const applyModeSafe = guard(applyMode);
  applyModeSafe();
  desktop.addEventListener('change', applyModeSafe);
  reduced.addEventListener('change', applyModeSafe);
  addEventListener('resize', guard(() => { clearTimeout(resizeTimer); resizeTimer = setTimeout(applyModeSafe, 200); }));
  document.addEventListener('visibilitychange', guard(() => {
    if (document.hidden) stopLoop();
    else if (!staticMode()) startLoop();
  }));
}

main().catch(fail);
```

- [ ] **Step 4: Check the size budget and verify in Chrome**

Run: `gzip -c shim.js | wc -c` → under 2560.

With `make serve` running in the background and Chrome DevTools MCP:

1. `navigate_page` to `http://localhost:8000/`, `resize_page` 1440×900, wait three seconds, `take_screenshot`. Expected: a dark alley rendered edge to edge, the compact card on the left third, the alley's centre and the hammer visible on the right, footprints glowing on the floor ahead. `list_console_messages` shows no errors.
2. Wait thirty more seconds and `take_screenshot` again: the view has moved along the trail; `evaluate_script` returning `Number(document.querySelector('#squashed').textContent)` is 0 or more (a squash may or may not have happened yet).
3. `navigate_page` to `http://localhost:8000/?seed=7`, wait two seconds, screenshot; reload the same URL and screenshot: the two frames show the same alley from the same spot (deterministic seed).
4. `navigate_page` to `http://localhost:8000/?motion=reduce`; two screenshots two seconds apart are identical and show footprints and the hammer.
5. `resize_page` 390×844, `navigate_page` to `http://localhost:8000/`: the card fills the width over a still city frame; no animation (two screenshots identical).
6. `list_network_requests`: exactly `index.html` (or `/`), `styles.css`, `shim.js`, `sim.wasm`; no favicon request.
7. Stop the server, `mv sim.wasm sim.wasm.bak`, restart, reload: the card renders over a plain dark background, the console shows the `alley hunt disabled` warning, `body` has class `no-sim`. Restore the file and confirm `git status --short` is clean apart from your edits.

Fix anything that fails before committing (shim, CSS, or C; a C fix means `make test`, `make sim.wasm`, `make smoke`, and committing `sim.wasm` too).

- [ ] **Step 5: Commit**

```bash
git add index.html styles.css shim.js
git commit -m "Rework the page: compact left card over a full-viewport first-person scene"
```

---

### Task 15: Final verification and follow-ups

**Files:**
- Create: `docs/superpowers/plans/2026-09-03-bug-smasher-phase1b-followups.md`
- Modify: `README.md` (describe the alley hunt instead of side panels), only if a check fails: anything else

- [ ] **Step 1: Run every automated check**

```bash
make clean && make test && make sim.wasm && make smoke
ls -l sim.wasm
gzip -c shim.js | wc -c
```

Expected: `0 failures`, zero warnings, smoke `ok`, `sim.wasm` under 65536 bytes, gzipped shim under 2560 bytes.

- [ ] **Step 2: Walk the spec's browser checklist (section 16)**

With `make serve` running: the desktop screenshot (card left, alley right, hammer bottom centre), the mobile screenshot (card full width over a still frame), `?motion=reduce`, console clean, the network panel showing exactly four files, and the renamed-module fallback. Record PASS or FAIL per item.

- [ ] **Step 3: Update the README and record follow-ups**

In `README.md`, replace the first paragraph and the layout block's first two lines so they describe one fixed full-viewport canvas rendering a first-person alley hunt (a raycaster) behind a compact card, and list the new `sim/` modules in one line: trig, map, textures, camera, raycast, sprites, decals, actors, hud, hunt, world. Replace the debug-parameter sentence with `?seed=N` and `?motion=reduce`. Update the Status paragraph: phase 1b complete, phase 2 (settlement) planned.

Write `docs/superpowers/plans/2026-09-03-bug-smasher-phase1b-followups.md` with any deferred findings from the reviews, plus these known polish items: variable wall heights, rain, visitor steering, and a softer lamp light falloff.

- [ ] **Step 4: Commit and tag**

```bash
git add README.md docs/superpowers/plans/2026-09-03-bug-smasher-phase1b-followups.md
git commit -m "Update README and record phase 1b follow-ups"
git tag phase-1b-complete
```

---

## Self-review notes (for the plan author)

- **Spec coverage.** Layout and card: Task 14. Visual style, palette, textures, sky, HUD look: Tasks 3, 5. Map and legend, light map, connectivity, hiding spots: Task 4. Camera motion, bob, pitch: Task 6. Hunt states and timings, trail glow and culling, bug states: Tasks 7, 11, 12. Dogs and wind: Task 11. Events and debug hooks: Tasks 13, 14. Architecture and exports: Tasks 12, 13. Renderer: Tasks 8, 9. Frame loop and static mode: Tasks 13, 14. Build: Task 13. Accessibility, resilience, budgets: Task 14, 15. Testing: every task plus 13 and 15.
- **Deviations from the spec text, on purpose.** Textures are stored as `Color` arrays and drawn with the existing framebuffer primitives at init, so no image data is embedded. The trail glow ahead fades from 1.0 to 0.5 beyond eight pairs rather than staying at 1.0, so the far end of a long trail reads as distant. The floor caster starts each column below the wall it hit rather than clipping against a wall-bottom table, which is the same result with less state. `HUNT_NEW_TRAIL` is a transient state resolved within the same step.
- **Hazards the implementer should expect.** Float accumulation at exact phase boundaries (use the `- 1e-4f` tolerance pattern where a test samples exactly at a boundary); the DDA ray must start from the camera's tile even when the camera stands on a tile edge; `Uint8ClampedArray` views must be recreated each frame in case wasm memory grows (it does not here, but the code does it anyway); `sim_render_static` re-initialises the world, so the shim must never call it mid-loop.
- **Phase 2 hooks.** `Stage`, `EventQueue` with a stage id, `SIM_EXPORT`, `draw.h`, `font.h`, `trig.h`, and `rng.h` are stage-agnostic. The settlement stage adds `sim/oasis/*`, its own framebuffer, and `oasis_*` exports; the shim adds a second canvas and the section wiring.
