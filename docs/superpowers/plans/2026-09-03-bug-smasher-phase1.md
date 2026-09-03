# Bug Smasher Profile — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the first screen of the profile: a 1:2:1 desktop layout with a hero card in the middle and two pixel-art side panels, driven by a C simulation compiled to a freestanding WebAssembly module, with case-file hover linking, a mobile fallback, and a native test suite.

**Architecture:** Two layers with one integer-only boundary. The page layer is `index.html`, `styles.css`, and `shim.js`, the only code that touches the DOM. The simulation layer is C under `sim/`, compiled once to `sim.wasm` with no imports and no libc, owning all state, timing, and pixel rendering into an RGBA framebuffer in its own memory. The same C compiles natively with sanitizers for tests. A `Stage` struct (framebuffer, fixed-step accumulator, palette, step/render hooks) hosts the single bugs stage now so phase 2 can add the settlement stage without restructuring.

**Tech Stack:** C11 (freestanding for wasm, libc only in tests), clang with the wasm32 target via `zig cc` (or Homebrew LLVM), GNU Make, Node 24 for a wasm smoke test, plain HTML/CSS/ES-module JavaScript, Chrome DevTools for browser verification.

**Spec:** `docs/superpowers/specs/2026-09-03-bug-smasher-profile-design.md` (revision 3). Phase 1 covers spec sections 2 to 6 and 8 to 15; section 7 (settlement) is phase 2 and is out of scope here except for the `Stage` abstraction in section 9.

## Global Constraints

- The wasm module has **no imports** and no libc. Only compiler-provided freestanding headers (`stdint.h`, `stddef.h`, `stdbool.h`) may be included from `sim/`. Never include `stdio.h`, `stdlib.h`, `string.h`, or `math.h` in `sim/`.
- No libm calls in `sim/` (`sqrt`, `sin`, `fabs`, `floor`, etc.). Floating point arithmetic itself is fine. Write small helpers instead.
- All exports cross the boundary as integers or pointers into wasm memory. No strings.
- Exports are marked `SIM_EXPORT("name")`, which expands to `__attribute__((export_name("name")))` under `__wasm__` and to nothing natively.
- Logical panel width is `96` px; logical panel height is at most `320`. Bug cap `20` per side. Character walk speed `24` logical px/s. Fixed logic step `1/60` s. Elapsed time clamped to `250` ms per update.
- Idle auto-play after `45` s. Crossover on a bug imbalance of `6` or after `120` s. Hover intent delay `150` ms.
- Vignette ids: `0` patrol, `1` race, `2` detective, `3` regression, `4` crossover. Only `1` to `3` are requestable.
- Event packing: `(type << 24) | (stage << 16) | (a << 8) | b`, so any real event is non-zero. Stage `0` is the bugs stage. Types: `1` VIGNETTE_START (a = id, b = auto), `2` VIGNETTE_END (a = id), `3` BUG_SQUASHED.
- `sim.wasm` under 40 KB uncompressed; `shim.js` under 3 KB gzipped (roughly 8 KB source). Zero dependencies.
- Canvases carry `aria-hidden="true"` and `role="presentation"`. Under `prefers-reduced-motion: reduce` no loop runs.
- Native tests build with `-fsanitize=address,undefined` and must pass clean.
- Commit after every task. Commit `sim.wasm` (the static host serves it; there is no CI build).
- All bugs are cute and cartoonish: round body, two antennae, stub legs. Never realistic.

---

## File Structure

```
index.html                  hero card markup, panel canvases, avatar canvas, below-fold placeholder
styles.css                  1:2:1 grid, hero card, case-file glow, mobile single column, reduced motion
shim.js                     wasm load, frame loop, blit, hover wiring, events → DOM, fallbacks
sim.wasm                    build output, committed
Makefile                    targets: sim.wasm, test, smoke, serve, clean
.gitignore                  build/
sim/
  export.h                  SIM_EXPORT macro
  rng.h / rng.c             mulberry32 seeded RNG
  events.h / events.c       packed event queue shared by all stages
  draw.h / draw.c           Framebuffer, Color, view/clip, rect, dim
  font.h / font.c           3x5 bitmap glyphs and draw_text
  palette.h / palette.c     bugs-stage palette
  bug.h / bug.c             bug entity
  hero.h / hero.c           character entity
  world.h / world.c         World struct, geometry, spawning, squashing, fx
  sprites.h / sprites.c     placeholder drawing of bug, hero, fx, props, glass
  stage.h / stage.c         Stage struct and fixed-step accumulator
  vignette.h                Vignette interface and ids
  vignettes/patrol.h/.c     default state
  vignettes/crossover.h/.c  walk behind the card
  vignettes/race.h/.c       race condition
  vignettes/detective.h/.c  heisenbug
  vignettes/regression.h/.c regression
  vignettes/table.h/.c      VIGNETTES[] lookup table
  scene.h / scene.c         state machine, requests, idle timer, crossover triggers
  sim.h / sim.c             exports: init, update, render, framebuffer, request, poll_event, static, avatar
  freestanding.c            memset/memcpy/memmove, compiled only for wasm
test/
  test.h                    CHECK macros
  test_main.c               runs every test_*() function
  test_harness.c            sanity
  test_rng.c test_events.c test_draw.c test_font.c test_bug.c test_hero.c test_world.c
  test_sprites.c test_stage.c test_patrol.c test_scene.c test_crossover.c
  test_race.c test_detective.c test_regression.c test_sim.c
  wasm_smoke.mjs            Node: instantiate sim.wasm, check imports/exports, render a frame
```

Every header uses an include guard named after the file (`RNG_H`, `EVENTS_H`, ...). Every `.c` in `sim/` includes only `sim/` headers and freestanding compiler headers.

---

### Task 1: Scaffold, test harness, Makefile `test` target

**Files:**
- Create: `Makefile`, `.gitignore`, `sim/export.h`, `test/test.h`, `test/test_main.c`, `test/test_harness.c`

**Interfaces:**
- Produces: `CHECK(cond)`, `CHECK_EQ(a,b)`, `CHECK_NEAR(a,b,eps)`, `RUN(fn)` macros; `int test_checks, test_failures` globals; `make test` builds `build/test` from `sim/*.c sim/vignettes/*.c test/*.c` with sanitizers and runs it; `SIM_EXPORT(name)` macro.

- [ ] **Step 1: Write the harness and a self-test**

`test/test.h`:

```c
#ifndef TEST_H
#define TEST_H
#include <stdio.h>
#include <string.h>
#include <math.h>

extern int test_checks, test_failures;

#define CHECK(cond) do { test_checks++; if (!(cond)) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); test_checks++; \
    if (_a != _b) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s == %s (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); test_checks++; \
    if (fabs(_a - _b) > (eps)) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s ~= %s (%f vs %f)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define RUN(fn) do { fprintf(stderr, "%s\n", #fn); fn(); } while (0)

#endif
```

`test/test_harness.c`:

```c
#include "test.h"

void test_harness(void) {
    CHECK(1 + 1 == 2);
    CHECK_EQ(2 * 3, 6);
    CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9);
}
```

`test/test_main.c`:

```c
#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);

int main(void) {
    RUN(test_harness);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
```

`sim/export.h`:

```c
#ifndef EXPORT_H
#define EXPORT_H
#ifdef __wasm__
#define SIM_EXPORT(name) __attribute__((export_name(name), visibility("default")))
#else
#define SIM_EXPORT(name)
#endif
#endif
```

- [ ] **Step 2: Write the Makefile and .gitignore**

`Makefile` (tabs, not spaces, before each recipe line):

```make
WASM_CC     ?= zig cc
WASM_TARGET ?= -target wasm32-freestanding
CC          ?= cc

SIM_SRC  := $(wildcard sim/*.c sim/vignettes/*.c)
SIM_HDR  := $(wildcard sim/*.h sim/vignettes/*.h)
TEST_SRC := $(wildcard test/*.c)
TEST_HDR := $(wildcard test/*.h)

# -Isim: sources under sim/vignettes/ include "world.h" and "vignettes/x.h" relative to sim/
WASM_FLAGS := $(WASM_TARGET) -std=c11 -nostdlib -ffreestanding -fno-builtin -fvisibility=hidden \
              -mbulk-memory -O2 -Wall -Wextra -Isim -Wl,--no-entry
TEST_FLAGS := -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined \
              -fno-omit-frame-pointer -Isim -Itest

all: sim.wasm

sim.wasm: $(SIM_SRC) $(SIM_HDR)
	$(WASM_CC) $(WASM_FLAGS) -o $@ $(SIM_SRC)
	@ls -l $@

build/test: $(SIM_SRC) $(SIM_HDR) $(TEST_SRC) $(TEST_HDR)
	@mkdir -p build
	$(CC) $(TEST_FLAGS) -o $@ $(SIM_SRC) $(TEST_SRC)

test: build/test
	./build/test

smoke: sim.wasm
	node test/wasm_smoke.mjs

serve:
	python3 -m http.server 8000

clean:
	rm -rf build sim.wasm

.PHONY: all test smoke serve clean
```

`.gitignore`:

```
build/
.DS_Store
```

- [ ] **Step 3: Run the tests**

Run: `make test`
Expected: output ends with `3 checks, 0 failures` and exit code 0. (`sim/` has no `.c` files yet; the wildcard is empty and that is fine.)

- [ ] **Step 4: Commit**

```bash
git add Makefile .gitignore sim/export.h test/
git commit -m "Scaffold test harness and Makefile

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: Seeded RNG

**Files:**
- Create: `sim/rng.h`, `sim/rng.c`, `test/test_rng.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `typedef struct { uint32_t state; } Rng;` `void rng_seed(Rng*, uint32_t)`, `uint32_t rng_next(Rng*)`, `int rng_range(Rng*, int lo, int hi)` inclusive, `float rng_float(Rng*)` in [0,1).

- [ ] **Step 1: Write the failing tests**

`test/test_rng.c`:

```c
#include "test.h"
#include "rng.h"

void test_rng(void) {
    Rng a, b;
    rng_seed(&a, 42u);
    rng_seed(&b, 42u);
    for (int i = 0; i < 100; i++) CHECK_EQ(rng_next(&a), rng_next(&b));

    rng_seed(&b, 43u);
    CHECK(rng_next(&a) != rng_next(&b));

    rng_seed(&a, 7u);
    for (int i = 0; i < 1000; i++) {
        int v = rng_range(&a, 3, 9);
        CHECK(v >= 3 && v <= 9);
        float f = rng_float(&a);
        CHECK(f >= 0.0f && f < 1.0f);
    }

    int seen[7] = {0};
    rng_seed(&a, 99u);
    for (int i = 0; i < 700; i++) seen[rng_range(&a, 0, 6)]++;
    for (int i = 0; i < 7; i++) CHECK(seen[i] > 50);

    rng_seed(&a, 0u);
    CHECK(rng_next(&a) != 0u);
}
```

Add to `test/test_main.c`: declaration `void test_rng(void);` and `RUN(test_rng);` after `RUN(test_harness);`.

- [ ] **Step 2: Run to verify failure**

Run: `make test`
Expected: compile error, `rng.h` not found.

- [ ] **Step 3: Implement mulberry32**

`sim/rng.h`:

```c
#ifndef RNG_H
#define RNG_H
#include <stdint.h>

typedef struct { uint32_t state; } Rng;

void     rng_seed(Rng *r, uint32_t seed);
uint32_t rng_next(Rng *r);
int      rng_range(Rng *r, int lo, int hi);   /* inclusive both ends */
float    rng_float(Rng *r);                   /* [0, 1) */

#endif
```

`sim/rng.c`:

```c
#include "rng.h"

void rng_seed(Rng *r, uint32_t seed) {
    r->state = seed ? seed : 0x9E3779B9u;
}

uint32_t rng_next(Rng *r) {
    uint32_t z = (r->state += 0x6D2B79F5u);
    z = (z ^ (z >> 15)) * (z | 1u);
    z ^= z + (z ^ (z >> 7)) * (z | 61u);
    return z ^ (z >> 14);
}

int rng_range(Rng *r, int lo, int hi) {
    uint32_t span = (uint32_t)(hi - lo + 1);
    return lo + (int)(rng_next(r) % span);
}

float rng_float(Rng *r) {
    return (float)(rng_next(r) >> 8) * (1.0f / 16777216.0f);
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test`
Expected: `... 0 failures`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add sim/rng.h sim/rng.c test/test_rng.c test/test_main.c
git commit -m "Add seeded mulberry32 RNG

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: Packed event queue

**Files:**
- Create: `sim/events.h`, `sim/events.c`, `test/test_events.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `EventQueue` (capacity 32, drops oldest on overflow), `events_init`, `event_pack(stage, type, a, b)`, `event_type/event_stage/event_a/event_b`, `events_push`, `events_pop` (0 when empty), `events_count`. Constants `STAGE_BUGS=0, STAGE_OASIS=1`, `EV_NONE=0, EV_VIGNETTE_START=1, EV_VIGNETTE_END=2, EV_BUG_SQUASHED=3`.

- [ ] **Step 1: Write the failing tests**

`test/test_events.c`:

```c
#include "test.h"
#include "events.h"

void test_events(void) {
    uint32_t e = event_pack(STAGE_BUGS, EV_VIGNETTE_START, 2, 1);
    CHECK(e != 0u);
    CHECK_EQ(event_type(e), EV_VIGNETTE_START);
    CHECK_EQ(event_stage(e), STAGE_BUGS);
    CHECK_EQ(event_a(e), 2);
    CHECK_EQ(event_b(e), 1);
    CHECK(event_pack(STAGE_OASIS, EV_BUG_SQUASHED, 0, 0) != 0u);

    EventQueue q;
    events_init(&q);
    CHECK_EQ(events_count(&q), 0);
    CHECK_EQ(events_pop(&q), 0u);

    events_push(&q, event_pack(0, EV_VIGNETTE_START, 1, 0));
    events_push(&q, event_pack(0, EV_BUG_SQUASHED, 0, 0));
    events_push(&q, event_pack(0, EV_VIGNETTE_END, 1, 0));
    CHECK_EQ(events_count(&q), 3);
    CHECK_EQ(event_type(events_pop(&q)), EV_VIGNETTE_START);
    CHECK_EQ(event_type(events_pop(&q)), EV_BUG_SQUASHED);
    CHECK_EQ(event_type(events_pop(&q)), EV_VIGNETTE_END);
    CHECK_EQ(events_pop(&q), 0u);

    /* overflow drops the oldest */
    for (int i = 0; i < EVENT_QUEUE_CAP + 5; i++)
        events_push(&q, event_pack(0, EV_BUG_SQUASHED, i & 0xFF, 0));
    CHECK_EQ(events_count(&q), EVENT_QUEUE_CAP);
    CHECK_EQ(event_a(events_pop(&q)), 5);
}
```

Add `void test_events(void);` and `RUN(test_events);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test`
Expected: compile error, `events.h` not found.

- [ ] **Step 3: Implement**

`sim/events.h`:

```c
#ifndef EVENTS_H
#define EVENTS_H
#include <stdint.h>

#define EVENT_QUEUE_CAP 32

enum { STAGE_BUGS = 0, STAGE_OASIS = 1 };
enum { EV_NONE = 0, EV_VIGNETTE_START = 1, EV_VIGNETTE_END = 2, EV_BUG_SQUASHED = 3 };

typedef struct {
    uint32_t items[EVENT_QUEUE_CAP];
    int head;
    int count;
} EventQueue;

void     events_init(EventQueue *q);
uint32_t event_pack(int stage, int type, int a, int b);
int      event_type(uint32_t e);
int      event_stage(uint32_t e);
int      event_a(uint32_t e);
int      event_b(uint32_t e);
void     events_push(EventQueue *q, uint32_t e);
uint32_t events_pop(EventQueue *q);
int      events_count(const EventQueue *q);

#endif
```

`sim/events.c`:

```c
#include "events.h"

void events_init(EventQueue *q) { q->head = 0; q->count = 0; }

uint32_t event_pack(int stage, int type, int a, int b) {
    return ((uint32_t)(type & 0xFF) << 24) | ((uint32_t)(stage & 0xFF) << 16)
         | ((uint32_t)(a & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}

int event_type(uint32_t e)  { return (int)((e >> 24) & 0xFF); }
int event_stage(uint32_t e) { return (int)((e >> 16) & 0xFF); }
int event_a(uint32_t e)     { return (int)((e >> 8) & 0xFF); }
int event_b(uint32_t e)     { return (int)(e & 0xFF); }

void events_push(EventQueue *q, uint32_t e) {
    if (q->count == EVENT_QUEUE_CAP) {           /* drop oldest */
        q->head = (q->head + 1) % EVENT_QUEUE_CAP;
        q->count--;
    }
    q->items[(q->head + q->count) % EVENT_QUEUE_CAP] = e;
    q->count++;
}

uint32_t events_pop(EventQueue *q) {
    if (q->count == 0) return 0u;
    uint32_t e = q->items[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_CAP;
    q->count--;
    return e;
}

int events_count(const EventQueue *q) { return q->count; }
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/events.h sim/events.c test/test_events.c test/test_main.c
git commit -m "Add packed event queue

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: Framebuffer and drawing primitives

**Files:**
- Create: `sim/draw.h`, `sim/draw.c`, `test/test_draw.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `Color {r,g,b,a}` and `COLOR(r,g,b)`; `Framebuffer {px,w,h,clip_x0,clip_y0,clip_x1,clip_y1,ox,oy}`; `fb_init`, `fb_set_view(fb, clip_x, clip_y, clip_w, clip_h, ox, oy)`, `fb_reset_view`, `fb_get(fb,x,y)` (raw), `draw_clear`, `draw_pixel`, `draw_rect`, `draw_dim(fb,x,y,w,h,amount)`. Drawing functions add `ox,oy` and clip to the view; `fb_get` and `draw_clear` ignore the view.

- [ ] **Step 1: Write the failing tests**

`test/test_draw.c`:

```c
#include "test.h"
#include "draw.h"

static uint8_t px[16 * 8 * 4];

static int is(Color c, int r, int g, int b) { return c.r == r && c.g == g && c.b == b && c.a == 255; }

void test_draw(void) {
    Framebuffer fb;
    fb_init(&fb, px, 16, 8);
    CHECK_EQ(fb.w, 16); CHECK_EQ(fb.h, 8);
    CHECK_EQ(fb.clip_x1, 16); CHECK_EQ(fb.clip_y1, 8);

    draw_clear(&fb, COLOR(1, 2, 3));
    CHECK(is(fb_get(&fb, 0, 0), 1, 2, 3));
    CHECK(is(fb_get(&fb, 15, 7), 1, 2, 3));

    /* rect clips at framebuffer edges without touching memory outside */
    draw_rect(&fb, 14, 6, 10, 10, COLOR(9, 9, 9));
    CHECK(is(fb_get(&fb, 14, 6), 9, 9, 9));
    CHECK(is(fb_get(&fb, 15, 7), 9, 9, 9));
    CHECK(is(fb_get(&fb, 13, 6), 1, 2, 3));
    draw_rect(&fb, -3, -3, 5, 5, COLOR(7, 7, 7));
    CHECK(is(fb_get(&fb, 0, 0), 7, 7, 7));
    CHECK(is(fb_get(&fb, 1, 1), 7, 7, 7));
    CHECK(is(fb_get(&fb, 2, 2), 1, 2, 3));

    /* view: right half of the buffer, drawing coords offset by +8 */
    draw_clear(&fb, COLOR(0, 0, 0));
    fb_set_view(&fb, 8, 0, 8, 8, 8, 0);
    draw_rect(&fb, 0, 0, 2, 2, COLOR(5, 5, 5));    /* lands at x=8..9 */
    CHECK(is(fb_get(&fb, 8, 0), 5, 5, 5));
    CHECK(is(fb_get(&fb, 9, 1), 5, 5, 5));
    CHECK(is(fb_get(&fb, 0, 0), 0, 0, 0));
    draw_rect(&fb, -4, 0, 6, 1, COLOR(6, 6, 6));   /* x=4..9 world, clipped to 8..9 */
    CHECK(is(fb_get(&fb, 7, 0), 0, 0, 0));
    CHECK(is(fb_get(&fb, 8, 0), 6, 6, 6));
    fb_reset_view(&fb);
    draw_pixel(&fb, 7, 0, COLOR(4, 4, 4));
    CHECK(is(fb_get(&fb, 7, 0), 4, 4, 4));

    /* dim reduces every channel; amount 255 gives black, 0 leaves unchanged */
    draw_clear(&fb, COLOR(200, 100, 50));
    draw_dim(&fb, 0, 0, 4, 4, 128);
    Color d = fb_get(&fb, 1, 1);
    CHECK(d.r < 200 && d.g < 100 && d.b < 50 && d.a == 255);
    CHECK(is(fb_get(&fb, 5, 5), 200, 100, 50));
    draw_dim(&fb, 0, 0, 4, 4, 255);
    CHECK(is(fb_get(&fb, 1, 1), 0, 0, 0));
    draw_dim(&fb, 5, 5, 1, 1, 0);
    CHECK(is(fb_get(&fb, 5, 5), 200, 100, 50));
}
```

Add `void test_draw(void);` and `RUN(test_draw);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `draw.h` not found.

- [ ] **Step 3: Implement**

`sim/draw.h`:

```c
#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } Color;
#define COLOR(r, g, b) ((Color){ (uint8_t)(r), (uint8_t)(g), (uint8_t)(b), 255 })

typedef struct {
    uint8_t *px;            /* RGBA, row-major, w*h*4 bytes */
    int w, h;
    int clip_x0, clip_y0;   /* inclusive */
    int clip_x1, clip_y1;   /* exclusive */
    int ox, oy;             /* translation added to draw coordinates */
} Framebuffer;

void  fb_init(Framebuffer *fb, uint8_t *px, int w, int h);
void  fb_set_view(Framebuffer *fb, int clip_x, int clip_y, int clip_w, int clip_h, int ox, int oy);
void  fb_reset_view(Framebuffer *fb);
Color fb_get(const Framebuffer *fb, int x, int y);        /* raw coords, no view */

void  draw_clear(Framebuffer *fb, Color c);                 /* whole buffer, no view */
void  draw_pixel(Framebuffer *fb, int x, int y, Color c);
void  draw_rect(Framebuffer *fb, int x, int y, int w, int h, Color c);
void  draw_dim(Framebuffer *fb, int x, int y, int w, int h, int amount); /* 0 none .. 255 black */

#endif
```

`sim/draw.c`:

```c
#include "draw.h"

void fb_init(Framebuffer *fb, uint8_t *px, int w, int h) {
    fb->px = px; fb->w = w; fb->h = h;
    fb_reset_view(fb);
}

void fb_set_view(Framebuffer *fb, int clip_x, int clip_y, int clip_w, int clip_h, int ox, int oy) {
    fb->clip_x0 = clip_x < 0 ? 0 : clip_x;
    fb->clip_y0 = clip_y < 0 ? 0 : clip_y;
    fb->clip_x1 = clip_x + clip_w > fb->w ? fb->w : clip_x + clip_w;
    fb->clip_y1 = clip_y + clip_h > fb->h ? fb->h : clip_y + clip_h;
    fb->ox = ox; fb->oy = oy;
}

void fb_reset_view(Framebuffer *fb) { fb_set_view(fb, 0, 0, fb->w, fb->h, 0, 0); }

Color fb_get(const Framebuffer *fb, int x, int y) {
    const uint8_t *p = fb->px + ((y * fb->w) + x) * 4;
    return (Color){ p[0], p[1], p[2], p[3] };
}

static void put(Framebuffer *fb, int x, int y, Color c) {
    uint8_t *p = fb->px + ((y * fb->w) + x) * 4;
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

void draw_clear(Framebuffer *fb, Color c) {
    for (int y = 0; y < fb->h; y++)
        for (int x = 0; x < fb->w; x++) put(fb, x, y, c);
}

/* Translate and clip a rectangle; returns 0 when nothing is visible. */
static int clip_rect(const Framebuffer *fb, int *x, int *y, int *w, int *h) {
    int x0 = *x + fb->ox, y0 = *y + fb->oy, x1 = x0 + *w, y1 = y0 + *h;
    if (x0 < fb->clip_x0) x0 = fb->clip_x0;
    if (y0 < fb->clip_y0) y0 = fb->clip_y0;
    if (x1 > fb->clip_x1) x1 = fb->clip_x1;
    if (y1 > fb->clip_y1) y1 = fb->clip_y1;
    if (x1 <= x0 || y1 <= y0) return 0;
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
    return 1;
}

void draw_pixel(Framebuffer *fb, int x, int y, Color c) { draw_rect(fb, x, y, 1, 1, c); }

void draw_rect(Framebuffer *fb, int x, int y, int w, int h, Color c) {
    if (!clip_rect(fb, &x, &y, &w, &h)) return;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) put(fb, xx, yy, c);
}

void draw_dim(Framebuffer *fb, int x, int y, int w, int h, int amount) {
    if (!clip_rect(fb, &x, &y, &w, &h)) return;
    int keep = 255 - (amount < 0 ? 0 : amount > 255 ? 255 : amount);
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) {
            uint8_t *p = fb->px + ((yy * fb->w) + xx) * 4;
            p[0] = (uint8_t)((p[0] * keep) / 255);
            p[1] = (uint8_t)((p[1] * keep) / 255);
            p[2] = (uint8_t)((p[2] * keep) / 255);
        }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/draw.h sim/draw.c test/test_draw.c test/test_main.c
git commit -m "Add framebuffer, view clipping, rect and dim primitives

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: 3×5 bitmap font

**Files:**
- Create: `sim/font.h`, `sim/font.c`, `test/test_font.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `const char *font_glyph(char c)` returning 15 chars (5 rows × 3 columns, `#` = lit) or NULL; `void draw_text(Framebuffer*, int x, int y, const char *s, Color)` with a 4 px advance (3 px glyph + 1 px gap), lowercase mapped to uppercase, unknown glyphs drawn as a hollow 3×5 box; `int draw_text_width(const char *s)`.

- [ ] **Step 1: Write the failing tests**

`test/test_font.c`:

```c
#include "test.h"
#include "draw.h"
#include "font.h"

static uint8_t px[32 * 8 * 4];
static int lit(const Framebuffer *fb, int x, int y) { return fb_get(fb, x, y).r == 255; }

void test_font(void) {
    CHECK(font_glyph('0') != NULL);
    CHECK(font_glyph('c') == font_glyph('C'));
    CHECK(font_glyph('~') == NULL);
    CHECK_EQ(strlen(font_glyph('1')), 15);

    CHECK_EQ(draw_text_width(""), 0);
    CHECK_EQ(draw_text_width("A"), 3);
    CHECK_EQ(draw_text_width("AB"), 7);
    CHECK_EQ(draw_text_width("COUNT"), 19);

    Framebuffer fb;
    fb_init(&fb, px, 32, 8);
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "1", COLOR(255, 255, 255));
    /* glyph '1' is " # ,## , # , # ,###" */
    CHECK(lit(&fb, 1, 0)); CHECK(!lit(&fb, 0, 0)); CHECK(!lit(&fb, 2, 0));
    CHECK(lit(&fb, 0, 1)); CHECK(lit(&fb, 1, 1)); CHECK(!lit(&fb, 2, 1));
    CHECK(lit(&fb, 0, 4)); CHECK(lit(&fb, 1, 4)); CHECK(lit(&fb, 2, 4));
    CHECK(!lit(&fb, 3, 0));   /* gap column */

    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "+1", COLOR(255, 255, 255));
    CHECK(lit(&fb, 1, 1)); CHECK(lit(&fb, 0, 2)); CHECK(lit(&fb, 2, 2)); CHECK(!lit(&fb, 0, 0));
    CHECK(lit(&fb, 5, 0));    /* '1' starts at x=4 */

    /* unknown glyph: hollow box has corners lit and centre dark */
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "~", COLOR(255, 255, 255));
    CHECK(lit(&fb, 0, 0)); CHECK(lit(&fb, 2, 4)); CHECK(!lit(&fb, 1, 2));

    /* clipping through the view */
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 30, 0, "8", COLOR(255, 255, 255));
    CHECK(lit(&fb, 31, 0));
}
```

Add `void test_font(void);` and `RUN(test_font);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `font.h` not found.

- [ ] **Step 3: Implement**

`sim/font.h`:

```c
#ifndef FONT_H
#define FONT_H
#include "draw.h"

#define FONT_W 3
#define FONT_H 5
#define FONT_ADVANCE 4

const char *font_glyph(char c);                  /* 15 chars or NULL */
void draw_text(Framebuffer *fb, int x, int y, const char *s, Color c);
int  draw_text_width(const char *s);

#endif
```

`sim/font.c` (each glyph is five 3-character rows concatenated):

```c
#include "font.h"

typedef struct { char ch; const char *rows; } Glyph;

static const Glyph GLYPHS[] = {
    { '0', "###" "# #" "# #" "# #" "###" },
    { '1', " # " "## " " # " " # " "###" },
    { '2', "###" "  #" "###" "#  " "###" },
    { '3', "###" "  #" "###" "  #" "###" },
    { '4', "# #" "# #" "###" "  #" "  #" },
    { '5', "###" "#  " "###" "  #" "###" },
    { '6', "###" "#  " "###" "# #" "###" },
    { '7', "###" "  #" "  #" "  #" "  #" },
    { '8', "###" "# #" "###" "# #" "###" },
    { '9', "###" "# #" "###" "  #" "###" },
    { '+', "   " " # " "###" " # " "   " },
    { '-', "   " "   " "###" "   " "   " },
    { '.', "   " "   " "   " "   " " # " },
    { '?', "###" "  #" " ##" "   " " # " },
    { ':', "   " " # " "   " " # " "   " },
    { ' ', "   " "   " "   " "   " "   " },
    { 'C', "###" "#  " "#  " "#  " "###" },
    { 'D', "## " "# #" "# #" "# #" "## " },
    { 'E', "###" "#  " "###" "#  " "###" },
    { 'K', "# #" "# #" "## " "# #" "# #" },
    { 'L', "#  " "#  " "#  " "#  " "###" },
    { 'M', "# #" "###" "###" "# #" "# #" },
    { 'N', "###" "# #" "# #" "# #" "# #" },
    { 'O', "###" "# #" "# #" "# #" "###" },
    { 'T', "###" " # " " # " " # " " # " },
    { 'U', "# #" "# #" "# #" "# #" "###" },
    { 'V', "# #" "# #" "# #" "# #" " # " },
    { 'X', "# #" "# #" " # " "# #" "# #" },
};

static const char BOX[] = "###" "# #" "# #" "# #" "###";

const char *font_glyph(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (unsigned i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++)
        if (GLYPHS[i].ch == c) return GLYPHS[i].rows;
    return 0;
}

int draw_text_width(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n ? n * FONT_ADVANCE - 1 : 0;
}

void draw_text(Framebuffer *fb, int x, int y, const char *s, Color c) {
    for (; *s; s++, x += FONT_ADVANCE) {
        const char *g = font_glyph(*s);
        if (!g) g = BOX;
        for (int row = 0; row < FONT_H; row++)
            for (int col = 0; col < FONT_W; col++)
                if (g[row * FONT_W + col] == '#') draw_pixel(fb, x + col, y + row, c);
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/font.h sim/font.c test/test_font.c test/test_main.c
git commit -m "Add 3x5 bitmap font and draw_text

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: Math helpers, palette, and the bug entity

**Files:**
- Create: `sim/fmath.h`, `sim/palette.h`, `sim/palette.c`, `sim/bug.h`, `sim/bug.c`, `sim/world.h` (struct only; functions come in Task 8), `test/test_bug.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `fm_abs`, `fm_sqrt`, `fm_clamp`, `fm_min`, `fm_max` inline helpers (no libm). `PALETTE_BUGS[COL_COUNT]` with the `COL_*` enum. `Bug` struct with states `BUG_DEAD, BUG_WANDER, BUG_RUSH, BUG_WAIT, BUG_QUEUE, BUG_SQUASHED, BUG_POOF`; `bug_spawn`, `bug_set_target`, `bug_update(Bug*, World*, float dt)`, `bug_alive`. Constants `BUG_W 10`, `BUG_H 8`, `BUG_WANDER_SPEED 6`, `BUG_RUSH_SPEED 30`, `BUG_SQUASH_TIME 0.4f`, `BUG_POOF_TIME 0.4f`.
- `World` struct (fields listed below) is declared here so `bug.c` can read panel geometry and the RNG; its functions arrive in Task 8.

- [ ] **Step 1: Write the failing tests**

`test/test_bug.c`:

```c
#include "test.h"
#include "fmath.h"
#include "world.h"

void test_bug(void) {
    CHECK_NEAR(fm_sqrt(16.0f), 4.0f, 1e-3);
    CHECK_NEAR(fm_sqrt(2.0f), 1.41421f, 1e-3);
    CHECK_NEAR(fm_sqrt(0.0f), 0.0f, 1e-6);
    CHECK_NEAR(fm_abs(-3.5f), 3.5f, 1e-6);
    CHECK_NEAR(fm_clamp(5.0f, 0.0f, 2.0f), 2.0f, 1e-6);
    CHECK_NEAR(fm_clamp(-1.0f, 0.0f, 2.0f), 0.0f, 1e-6);
    CHECK_EQ(PALETTE_BUGS[COL_BG].a, 255);
    CHECK(PALETTE_BUGS[COL_RED].r > PALETTE_BUGS[COL_RED].g);

    /* a hand-built world: 96x240 panels, 200px gap */
    World w;
    memset(&w, 0, sizeof w);
    w.panel_w = 96; w.panel_h = 240; w.gap_w = 200; w.world_w = 96 + 200 + 96;
    w.floor_top = 132; w.floor_bottom = 216;
    rng_seed(&w.rng, 5u);

    Bug b;
    bug_spawn(&b, SIDE_RIGHT, 300.0f, 150.0f, 1);
    CHECK_EQ(b.state, BUG_WANDER);
    CHECK_EQ(b.side, SIDE_RIGHT);
    CHECK_EQ(b.variant, 1);
    CHECK(bug_alive(&b));

    /* wander stays inside the right panel's floor band for 10 simulated seconds */
    int moved = 0;
    for (int i = 0; i < 600; i++) {
        float ox = b.x;
        bug_update(&b, &w, 1.0f / 60.0f);
        if (b.x != ox) moved = 1;
        CHECK(b.x >= 296.0f + BUG_W / 2 && b.x <= 392.0f - BUG_W / 2);
        CHECK(b.y >= 132.0f && b.y <= 216.0f);
    }
    CHECK(moved);

    /* rush reaches its target and waits there */
    bug_spawn(&b, SIDE_LEFT, 10.0f, 200.0f, 0);
    bug_set_target(&b, 40.0f, 200.0f);
    CHECK_EQ(b.state, BUG_RUSH);
    for (int i = 0; i < 90; i++) bug_update(&b, &w, 1.0f / 60.0f);   /* 30px at 30px/s = 1s */
    CHECK_EQ(b.state, BUG_WAIT);
    CHECK_NEAR(b.x, 40.0f, 0.01);

    /* squashed then dead after 0.4s */
    b.state = BUG_SQUASHED; b.timer = BUG_SQUASH_TIME;
    CHECK(!bug_alive(&b));
    for (int i = 0; i < 23; i++) bug_update(&b, &w, 1.0f / 60.0f);
    CHECK_EQ(b.state, BUG_SQUASHED);
    for (int i = 0; i < 3; i++) bug_update(&b, &w, 1.0f / 60.0f);
    CHECK_EQ(b.state, BUG_DEAD);
}
```

Add `void test_bug(void);` and `RUN(test_bug);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `fmath.h` not found.

- [ ] **Step 3: Implement**

`sim/fmath.h`:

```c
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
```

`sim/palette.h`:

```c
#ifndef PALETTE_H
#define PALETTE_H
#include "draw.h"

enum {
    COL_BG = 0, COL_GRID, COL_HORIZON, COL_FLOOR,
    COL_BUG_A, COL_BUG_B, COL_BUG_C, COL_BUG_DARK,
    COL_HERO_BODY, COL_HERO_SKIN, COL_HAMMER, COL_HANDLE, COL_HAT,
    COL_TEXT, COL_RED, COL_GREEN, COL_DUST, COL_PROP, COL_PROP_DARK, COL_GLOW,
    COL_COUNT
};

extern const Color PALETTE_BUGS[COL_COUNT];

#endif
```

`sim/palette.c`:

```c
#include "palette.h"

const Color PALETTE_BUGS[COL_COUNT] = {
    [COL_BG]        = COLOR(18, 22, 40),
    [COL_GRID]      = COLOR(26, 31, 54),
    [COL_HORIZON]   = COLOR(44, 52, 84),
    [COL_FLOOR]     = COLOR(24, 29, 50),
    [COL_BUG_A]     = COLOR(214, 93, 92),
    [COL_BUG_B]     = COLOR(220, 160, 70),
    [COL_BUG_C]     = COLOR(120, 180, 120),
    [COL_BUG_DARK]  = COLOR(40, 20, 30),
    [COL_HERO_BODY] = COLOR(88, 120, 200),
    [COL_HERO_SKIN] = COLOR(240, 200, 170),
    [COL_HAMMER]    = COLOR(250, 200, 60),
    [COL_HANDLE]    = COLOR(120, 80, 60),
    [COL_HAT]       = COLOR(110, 70, 50),
    [COL_TEXT]      = COLOR(220, 225, 240),
    [COL_RED]       = COLOR(230, 70, 70),
    [COL_GREEN]     = COLOR(80, 200, 120),
    [COL_DUST]      = COLOR(160, 165, 190),
    [COL_PROP]      = COLOR(130, 120, 150),
    [COL_PROP_DARK] = COLOR(70, 64, 90),
    [COL_GLOW]      = COLOR(255, 240, 150),
};
```

`sim/bug.h`:

```c
#ifndef BUG_H
#define BUG_H

#define BUG_W 10
#define BUG_H 8
#define BUG_WANDER_SPEED 6.0f
#define BUG_RUSH_SPEED 30.0f
#define BUG_SQUASH_TIME 0.4f
#define BUG_POOF_TIME 0.4f

enum { BUG_DEAD = 0, BUG_WANDER, BUG_RUSH, BUG_WAIT, BUG_QUEUE, BUG_SQUASHED, BUG_POOF };

typedef struct {
    int state, side, variant;
    float x, y;              /* feet position: bottom centre, world coords */
    float vx, vy;
    float timer;
    float target_x, target_y;
    int hidden;              /* drawn only while revealed */
    int revealed;
} Bug;

struct World;

void bug_spawn(Bug *b, int side, float x, float y, int variant);
void bug_set_target(Bug *b, float x, float y);          /* enters BUG_RUSH */
void bug_update(Bug *b, struct World *w, float dt);
int  bug_alive(const Bug *b);                            /* not DEAD, SQUASHED, or POOF */

#endif
```

`sim/world.h` (struct and constants now; function prototypes are added in Task 8):

```c
#ifndef WORLD_H
#define WORLD_H
#include <stdint.h>
#include "rng.h"
#include "events.h"
#include "bug.h"
#include "hero.h"

#define PANEL_W_MAX 96
#define PANEL_H_MAX 320
#define MAX_BUGS_PER_SIDE 20
#define MAX_BUGS (2 * MAX_BUGS_PER_SIDE)
#define MAX_FX 16
#define FLOOR_TOP_FRAC 0.55f
#define FLOOR_BOTTOM_FRAC 0.90f

enum { SIDE_LEFT = 0, SIDE_RIGHT = 1, SIDE_GAP = -1 };
enum { FX_DUST = 0, FX_PLUS1 = 1, FX_POOF = 2 };

typedef struct { int active, kind; float x, y, t; } Fx;

typedef struct World {
    int panel_w, panel_h, gap_w, world_w;
    int floor_top, floor_bottom;          /* y range for actor feet */
    Rng rng;
    Bug bugs[MAX_BUGS];
    Hero hero;
    Fx fx[MAX_FX];
    EventQueue *events;
    int squashed_total;
    float time;
} World;

#endif
```

`hero.h` does not exist yet; create a stub now so `world.h` compiles, and Task 7 fills it in:

`sim/hero.h` (temporary stub, replaced in Task 7):

```c
#ifndef HERO_H
#define HERO_H
typedef struct { float x, y; } Hero;
#endif
```

`sim/bug.c`:

```c
#include "bug.h"
#include "world.h"
#include "fmath.h"

static float panel_x0(const World *w, int side) { return side == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w) : 0.0f; }

void bug_spawn(Bug *b, int side, float x, float y, int variant) {
    b->state = BUG_WANDER; b->side = side; b->variant = variant;
    b->x = x; b->y = y; b->vx = 0; b->vy = 0; b->timer = 0;
    b->target_x = x; b->target_y = y; b->hidden = 0; b->revealed = 0;
}

void bug_set_target(Bug *b, float x, float y) {
    b->target_x = x; b->target_y = y; b->state = BUG_RUSH;
}

int bug_alive(const Bug *b) {
    return b->state != BUG_DEAD && b->state != BUG_SQUASHED && b->state != BUG_POOF;
}

static void wander(Bug *b, World *w, float dt) {
    b->timer -= dt;
    if (b->timer <= 0.0f) {
        b->vx = (float)rng_range(&w->rng, -6, 6);
        b->vy = (float)rng_range(&w->rng, -3, 3);
        b->timer = 1.0f + rng_float(&w->rng);
    }
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    float x0 = panel_x0(w, b->side) + BUG_W / 2, x1 = panel_x0(w, b->side) + w->panel_w - BUG_W / 2;
    if (b->x < x0) { b->x = x0; b->vx = -b->vx; }
    if (b->x > x1) { b->x = x1; b->vx = -b->vx; }
    if (b->y < (float)w->floor_top)    { b->y = (float)w->floor_top;    b->vy = -b->vy; }
    if (b->y > (float)w->floor_bottom) { b->y = (float)w->floor_bottom; b->vy = -b->vy; }
}

static void rush(Bug *b, float dt) {
    float dx = b->target_x - b->x, dy = b->target_y - b->y;
    float dist = fm_sqrt(dx * dx + dy * dy);
    float step = BUG_RUSH_SPEED * dt;
    if (dist <= step || dist < 0.001f) { b->x = b->target_x; b->y = b->target_y; b->state = BUG_WAIT; return; }
    b->x += dx / dist * step;
    b->y += dy / dist * step;
}

void bug_update(Bug *b, World *w, float dt) {
    switch (b->state) {
    case BUG_WANDER: wander(b, w, dt); break;
    case BUG_RUSH:   rush(b, dt); break;
    case BUG_SQUASHED:
    case BUG_POOF:
        b->timer -= dt;
        if (b->timer <= 0.0f) b->state = BUG_DEAD;
        break;
    default: break;                       /* DEAD, WAIT, QUEUE hold still */
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/fmath.h sim/palette.h sim/palette.c sim/bug.h sim/bug.c sim/world.h sim/hero.h test/test_bug.c test/test_main.c
git commit -m "Add math helpers, bugs palette, and bug entity

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: Hero entity

**Files:**
- Modify: `sim/hero.h` (replace the stub)
- Create: `sim/hero.c`, `test/test_hero.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `Hero` with actions `HERO_IDLE, HERO_WALK, HERO_WINDUP, HERO_BONK, HERO_HIDDEN, HERO_SIT, HERO_THINK`; constants `HERO_W 14`, `HERO_H 20`, `HERO_WALK_SPEED 24.0f`, `HERO_WINDUP_TIME 0.25f`, `HERO_BONK_TIME 0.15f`, `HERO_REACH 8.0f`; `hero_init`, `hero_walk_to`, `hero_arrived`, `hero_start_bonk`, `hero_busy`, `hero_hide`, `hero_show`, `hero_set_action`, `hero_update`. `hero.bonk_landed` is 1 for exactly the update step in which the swing lands. `hero.facing` is +1 or -1.

- [ ] **Step 1: Write the failing tests**

`test/test_hero.c`:

```c
#include "test.h"
#include "hero.h"

#define DT (1.0f / 60.0f)

void test_hero(void) {
    Hero h;
    hero_init(&h, 10.0f, 100.0f);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK(hero_arrived(&h));
    CHECK_EQ(h.facing, 1);

    /* walk 24px right at 24px/s: not arrived after 59 steps, arrived by 61 */
    hero_walk_to(&h, 34.0f, 100.0f);
    CHECK_EQ(h.action, HERO_WALK);
    for (int i = 0; i < 59; i++) hero_update(&h, DT);
    CHECK(!hero_arrived(&h));
    for (int i = 0; i < 2; i++) hero_update(&h, DT);
    CHECK(hero_arrived(&h));
    CHECK_NEAR(h.x, 34.0f, 0.01);
    CHECK_EQ(h.action, HERO_IDLE);

    /* walking left flips facing */
    hero_walk_to(&h, 0.0f, 100.0f);
    hero_update(&h, DT);
    CHECK_EQ(h.facing, -1);

    /* bonk: windup then a single landed step, then idle */
    hero_init(&h, 10.0f, 100.0f);
    hero_start_bonk(&h);
    CHECK_EQ(h.action, HERO_WINDUP);
    CHECK(hero_busy(&h));
    int landed = 0, landed_step = -1;
    for (int i = 0; i < 40; i++) {
        hero_update(&h, DT);
        if (h.bonk_landed) { landed++; landed_step = i; }
    }
    CHECK_EQ(landed, 1);
    CHECK(landed_step >= 13 && landed_step <= 16);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK(!hero_busy(&h));

    /* hide and show */
    hero_hide(&h);
    CHECK_EQ(h.action, HERO_HIDDEN);
    hero_show(&h, 300.0f, 100.0f);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK_NEAR(h.x, 300.0f, 1e-6);

    hero_set_action(&h, HERO_THINK);
    CHECK_EQ(h.action, HERO_THINK);
}
```

Add `void test_hero(void);` and `RUN(test_hero);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile errors about `hero_init` and friends.

- [ ] **Step 3: Implement**

`sim/hero.h` (full replacement of the stub):

```c
#ifndef HERO_H
#define HERO_H

#define HERO_W 14
#define HERO_H 20
#define HERO_WALK_SPEED 24.0f
#define HERO_WINDUP_TIME 0.25f
#define HERO_BONK_TIME 0.15f
#define HERO_REACH 8.0f

enum { HERO_IDLE = 0, HERO_WALK, HERO_WINDUP, HERO_BONK, HERO_HIDDEN, HERO_SIT, HERO_THINK };

typedef struct {
    float x, y;                 /* feet position, world coords */
    int facing;                 /* +1 right, -1 left */
    int action;
    float timer;
    float target_x, target_y;
    int has_target;
    int detective;              /* draws hat and glass */
    float glass_x, glass_y;     /* magnifying glass centre, world coords */
    int bonk_landed;            /* 1 only on the step the swing lands */
} Hero;

void hero_init(Hero *h, float x, float y);
void hero_walk_to(Hero *h, float x, float y);
int  hero_arrived(const Hero *h);
void hero_start_bonk(Hero *h);
int  hero_busy(const Hero *h);              /* WINDUP or BONK */
void hero_hide(Hero *h);
void hero_show(Hero *h, float x, float y);
void hero_set_action(Hero *h, int action);  /* IDLE, SIT, THINK */
void hero_update(Hero *h, float dt);

#endif
```

`sim/hero.c`:

```c
#include "hero.h"
#include "fmath.h"

void hero_init(Hero *h, float x, float y) {
    h->x = x; h->y = y; h->facing = 1; h->action = HERO_IDLE; h->timer = 0;
    h->target_x = x; h->target_y = y; h->has_target = 0;
    h->detective = 0; h->glass_x = x; h->glass_y = y; h->bonk_landed = 0;
}

void hero_walk_to(Hero *h, float x, float y) {
    h->target_x = x; h->target_y = y; h->has_target = 1; h->action = HERO_WALK;
    if (x < h->x) h->facing = -1; else if (x > h->x) h->facing = 1;
}

int hero_arrived(const Hero *h) { return !h->has_target && h->action != HERO_WALK; }

void hero_start_bonk(Hero *h) {
    h->has_target = 0; h->action = HERO_WINDUP; h->timer = 0;
}

int hero_busy(const Hero *h) { return h->action == HERO_WINDUP || h->action == HERO_BONK; }

void hero_hide(Hero *h) { h->action = HERO_HIDDEN; h->has_target = 0; }

void hero_show(Hero *h, float x, float y) { h->x = x; h->y = y; h->action = HERO_IDLE; h->has_target = 0; }

void hero_set_action(Hero *h, int action) { h->action = action; h->has_target = 0; h->timer = 0; }

void hero_update(Hero *h, float dt) {
    h->bonk_landed = 0;
    switch (h->action) {
    case HERO_WALK: {
        float dx = h->target_x - h->x, dy = h->target_y - h->y;
        float dist = fm_sqrt(dx * dx + dy * dy);
        float step = HERO_WALK_SPEED * dt;
        if (dist <= step || dist < 0.001f) {
            h->x = h->target_x; h->y = h->target_y; h->has_target = 0; h->action = HERO_IDLE;
        } else {
            h->x += dx / dist * step; h->y += dy / dist * step;
            if (dx < 0) h->facing = -1; else if (dx > 0) h->facing = 1;
        }
        break;
    }
    case HERO_WINDUP:
        h->timer += dt;
        if (h->timer >= HERO_WINDUP_TIME - 1e-4f) { h->action = HERO_BONK; h->timer = 0; h->bonk_landed = 1; }
        break;
    case HERO_BONK:
        h->timer += dt;
        if (h->timer >= HERO_BONK_TIME - 1e-4f) { h->action = HERO_IDLE; h->timer = 0; }
        break;
    default: break;
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/hero.h sim/hero.c test/test_hero.c test/test_main.c
git commit -m "Add hero entity with walk, windup, and bonk

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 8: World geometry, spawning, squashing, and fx

**Files:**
- Modify: `sim/world.h` (add prototypes)
- Create: `sim/world.c`, `test/test_world.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `world_init(w, seed, panel_w, panel_h, gap_w, events)`, `world_side_of_x`, `world_panel_x0(side)`, `world_panel_center_x(side)`, `world_hero_side`, `world_bug_count(side)`, `world_spawn_bug(side)` (random edge position, NULL at cap), `world_spawn_bug_at(side, x, y)`, `world_nearest_bug(side, x, y)` (wandering bugs only), `world_squash_near(x, y, radius)` (emits `EV_BUG_SQUASHED`, spawns dust and +1 fx, returns count), `world_spawn_fx`, `world_clear_scripted_bugs` (kills RUSH/WAIT/QUEUE bugs), `world_update_actors(dt)`. Fx lifetimes `FX_DUST_TIME 0.4f`, `FX_PLUS1_TIME 0.8f`, `FX_POOF_TIME 0.4f`.

- [ ] **Step 1: Write the failing tests**

`test/test_world.c`:

```c
#include "test.h"
#include "world.h"

#define DT (1.0f / 60.0f)

void test_world(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 1u, 96, 240, 200, &ev);

    CHECK_EQ(w.world_w, 392);
    CHECK_EQ(w.floor_top, 132);
    CHECK_EQ(w.floor_bottom, 216);
    CHECK_EQ(world_side_of_x(&w, 10.0f), SIDE_LEFT);
    CHECK_EQ(world_side_of_x(&w, 150.0f), SIDE_GAP);
    CHECK_EQ(world_side_of_x(&w, 300.0f), SIDE_RIGHT);
    CHECK_NEAR(world_panel_x0(&w, SIDE_LEFT), 0.0f, 1e-6);
    CHECK_NEAR(world_panel_x0(&w, SIDE_RIGHT), 296.0f, 1e-6);
    CHECK_NEAR(world_panel_center_x(&w, SIDE_LEFT), 48.0f, 1e-6);
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK(w.hero.y >= w.floor_top && w.hero.y <= w.floor_bottom);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);

    /* spawn to the cap, then NULL */
    for (int i = 0; i < MAX_BUGS_PER_SIDE; i++) CHECK(world_spawn_bug(&w, SIDE_LEFT) != NULL);
    CHECK(world_spawn_bug(&w, SIDE_LEFT) == NULL);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), MAX_BUGS_PER_SIDE);
    CHECK_EQ(world_bug_count(&w, SIDE_RIGHT), 0);
    CHECK(world_spawn_bug(&w, SIDE_RIGHT) != NULL);

    /* every spawned bug is on its own side's floor band and at a panel edge */
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w.bugs[i];
        if (!bug_alive(b)) continue;
        CHECK_EQ(world_side_of_x(&w, b->x), b->side);
        CHECK(b->y >= w.floor_top && b->y <= w.floor_bottom);
        float rel = b->x - world_panel_x0(&w, b->side);
        CHECK(rel <= BUG_W || rel >= 96 - BUG_W);
    }

    /* nearest and squash */
    world_init(&w, 2u, 96, 240, 200, &ev);
    Bug *far = world_spawn_bug_at(&w, SIDE_LEFT, 80.0f, 200.0f);
    Bug *near = world_spawn_bug_at(&w, SIDE_LEFT, 20.0f, 200.0f);
    CHECK(world_nearest_bug(&w, SIDE_LEFT, 10.0f, 200.0f) == near);
    CHECK(world_nearest_bug(&w, SIDE_RIGHT, 10.0f, 200.0f) == NULL);
    CHECK_EQ(world_squash_near(&w, 22.0f, 200.0f, HERO_REACH), 1);
    CHECK_EQ(near->state, BUG_SQUASHED);
    CHECK_EQ(far->state, BUG_WANDER);
    CHECK_EQ(events_count(&ev), 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);
    CHECK_EQ(w.squashed_total, 1);
    int dust = 0, plus = 0;
    for (int i = 0; i < MAX_FX; i++) if (w.fx[i].active) { if (w.fx[i].kind == FX_DUST) dust++; if (w.fx[i].kind == FX_PLUS1) plus++; }
    CHECK_EQ(dust, 1); CHECK_EQ(plus, 1);
    CHECK(world_nearest_bug(&w, SIDE_LEFT, 10.0f, 200.0f) == far);   /* squashed bugs are not targets */

    /* actors update: fx expire, squashed bug dies, wanderers stay in bounds */
    for (int i = 0; i < 60; i++) world_update_actors(&w, DT);
    CHECK_EQ(near->state, BUG_DEAD);
    for (int i = 0; i < MAX_FX; i++) CHECK(!w.fx[i].active);
    CHECK(far->x >= BUG_W / 2 && far->x <= 96 - BUG_W / 2);

    /* scripted bugs are cleared, wanderers kept */
    Bug *s = world_spawn_bug_at(&w, SIDE_LEFT, 30.0f, 200.0f);
    bug_set_target(s, 50.0f, 200.0f);
    world_clear_scripted_bugs(&w);
    CHECK_EQ(s->state, BUG_DEAD);
    CHECK_EQ(far->state, BUG_WANDER);
}
```

Add `void test_world(void);` and `RUN(test_world);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile errors about `world_init` and friends.

- [ ] **Step 3: Implement**

Append to `sim/world.h`, before the final `#endif`:

```c
#define FX_DUST_TIME 0.4f
#define FX_PLUS1_TIME 0.8f
#define FX_POOF_TIME 0.4f

void  world_init(World *w, uint32_t seed, int panel_w, int panel_h, int gap_w, EventQueue *events);
int   world_side_of_x(const World *w, float x);
float world_panel_x0(const World *w, int side);
float world_panel_center_x(const World *w, int side);
int   world_hero_side(const World *w);
int   world_bug_count(const World *w, int side);
Bug  *world_spawn_bug(World *w, int side);
Bug  *world_spawn_bug_at(World *w, int side, float x, float y);
Bug  *world_nearest_bug(World *w, int side, float x, float y);
int   world_squash_near(World *w, float x, float y, float radius);
Fx   *world_spawn_fx(World *w, int kind, float x, float y);
void  world_clear_scripted_bugs(World *w);
void  world_update_actors(World *w, float dt);
```

`sim/world.c`:

```c
#include "world.h"
#include "fmath.h"

void world_init(World *w, uint32_t seed, int panel_w, int panel_h, int gap_w, EventQueue *events) {
    w->panel_w = panel_w; w->panel_h = panel_h; w->gap_w = gap_w;
    w->world_w = 2 * panel_w + gap_w;
    w->floor_top = (int)((float)panel_h * FLOOR_TOP_FRAC);
    w->floor_bottom = (int)((float)panel_h * FLOOR_BOTTOM_FRAC);
    rng_seed(&w->rng, seed);
    for (int i = 0; i < MAX_BUGS; i++) w->bugs[i].state = BUG_DEAD;
    for (int i = 0; i < MAX_FX; i++) w->fx[i].active = 0;
    hero_init(&w->hero, world_panel_center_x(w, SIDE_LEFT), (float)(w->floor_bottom - 6));
    w->events = events; w->squashed_total = 0; w->time = 0.0f;
}

int world_side_of_x(const World *w, float x) {
    if (x < (float)w->panel_w) return SIDE_LEFT;
    if (x >= (float)(w->panel_w + w->gap_w)) return SIDE_RIGHT;
    return SIDE_GAP;
}

float world_panel_x0(const World *w, int side) { return side == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w) : 0.0f; }

float world_panel_center_x(const World *w, int side) { return world_panel_x0(w, side) + (float)w->panel_w * 0.5f; }

int world_hero_side(const World *w) {
    if (w->hero.action == HERO_HIDDEN) return SIDE_GAP;
    return world_side_of_x(w, w->hero.x);
}

int world_bug_count(const World *w, int side) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) if (bug_alive(&w->bugs[i]) && w->bugs[i].side == side) n++;
    return n;
}

static Bug *free_slot(World *w) {
    for (int i = 0; i < MAX_BUGS; i++) if (w->bugs[i].state == BUG_DEAD) return &w->bugs[i];
    return 0;
}

Bug *world_spawn_bug_at(World *w, int side, float x, float y) {
    if (world_bug_count(w, side) >= MAX_BUGS_PER_SIDE) return 0;
    Bug *b = free_slot(w);
    if (!b) return 0;
    bug_spawn(b, side, x, y, rng_range(&w->rng, 0, 2));
    return b;
}

Bug *world_spawn_bug(World *w, int side) {
    float x0 = world_panel_x0(w, side);
    float x = rng_range(&w->rng, 0, 1) ? x0 + BUG_W / 2 : x0 + (float)w->panel_w - BUG_W / 2;
    float y = (float)rng_range(&w->rng, w->floor_top, w->floor_bottom);
    return world_spawn_bug_at(w, side, x, y);
}

Bug *world_nearest_bug(World *w, int side, float x, float y) {
    Bug *best = 0; float best_d = 0;
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w->bugs[i];
        if (b->state != BUG_WANDER || b->side != side) continue;
        float dx = b->x - x, dy = b->y - y, d = dx * dx + dy * dy;
        if (!best || d < best_d) { best = b; best_d = d; }
    }
    return best;
}

Fx *world_spawn_fx(World *w, int kind, float x, float y) {
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &w->fx[i];
        if (f->active) continue;
        f->active = 1; f->kind = kind; f->x = x; f->y = y; f->t = 0;
        return f;
    }
    return 0;
}

int world_squash_near(World *w, float x, float y, float radius) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w->bugs[i];
        if (!bug_alive(b)) continue;
        float dx = b->x - x, dy = b->y - y;
        if (dx * dx + dy * dy > radius * radius) continue;
        b->state = BUG_SQUASHED; b->timer = BUG_SQUASH_TIME;
        world_spawn_fx(w, FX_DUST, b->x, b->y);
        world_spawn_fx(w, FX_PLUS1, b->x, b->y - BUG_H);
        if (w->events) events_push(w->events, event_pack(STAGE_BUGS, EV_BUG_SQUASHED, 0, 0));
        w->squashed_total++;
        n++;
    }
    return n;
}

void world_clear_scripted_bugs(World *w) {
    for (int i = 0; i < MAX_BUGS; i++) {
        int s = w->bugs[i].state;
        if (s == BUG_RUSH || s == BUG_WAIT || s == BUG_QUEUE) w->bugs[i].state = BUG_DEAD;
    }
}

static float fx_lifetime(int kind) {
    return kind == FX_PLUS1 ? FX_PLUS1_TIME : kind == FX_POOF ? FX_POOF_TIME : FX_DUST_TIME;
}

void world_update_actors(World *w, float dt) {
    w->time += dt;
    for (int i = 0; i < MAX_BUGS; i++) if (w->bugs[i].state != BUG_DEAD) bug_update(&w->bugs[i], w, dt);
    hero_update(&w->hero, dt);
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &w->fx[i];
        if (!f->active) continue;
        f->t += dt;
        if (f->t >= fx_lifetime(f->kind)) f->active = 0;
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/world.h sim/world.c test/test_world.c test/test_main.c
git commit -m "Add world geometry, spawning, squashing, and fx

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 9: Placeholder sprites

**Files:**
- Create: `sim/sprites.h`, `sim/sprites.c`, `test/test_sprites.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `sprite_bug(fb, pal, bug)`, `sprite_hero(fb, pal, hero)`, `sprite_fx(fb, pal, fx)`, `sprite_prop(fb, pal, kind, x, y, state)`, `sprite_glass(fb, pal, cx, cy)`. Prop kinds `PROP_BOX, PROP_PADLOCK, PROP_FOOTPRINT, PROP_HIDING_SPOT, PROP_SHIELD`. Box states `BOX_NORMAL 0, BOX_GLITCH 1, BOX_LOCKED 2, BOX_GREEN 3`. Footprint state `1` glows. All positions are feet / bottom-centre in the framebuffer's draw coordinates (the caller sets the view). This is the only file that knows what anything looks like; phase 1 uses rectangles.

- [ ] **Step 1: Write the failing tests**

`test/test_sprites.c`:

```c
#include "test.h"
#include "sprites.h"
#include "palette.h"

static uint8_t px[96 * 96 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

/* count pixels inside a rect that are not the background colour */
static int painted(const Framebuffer *fb, int x, int y, int w, int h) {
    int n = 0;
    for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++)
        if (!same(fb_get(fb, xx, yy), PALETTE_BUGS[COL_BG])) n++;
    return n;
}

void test_sprites(void) {
    Framebuffer fb;
    fb_init(&fb, px, 96, 96);
    const Color *pal = PALETTE_BUGS;

    Bug b;
    bug_spawn(&b, SIDE_LEFT, 40.0f, 60.0f, 1);
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 51, 12, 10) > 20);          /* body lands inside its box */
    CHECK_EQ(painted(&fb, 0, 0, 96, 50), 0);            /* nothing above the antennae */
    CHECK_EQ(painted(&fb, 0, 62, 96, 34), 0);           /* nothing below the feet */
    CHECK(same(fb_get(&fb, 40, 57), pal[COL_BUG_B]));   /* variant 1 body colour */

    b.hidden = 1; b.revealed = 0;
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK_EQ(painted(&fb, 0, 0, 96, 96), 0);
    b.revealed = 1;
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 51, 12, 10) > 20);

    b.hidden = 0; b.state = BUG_SQUASHED;
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 58, 12, 3) > 10);            /* flat splat */
    CHECK_EQ(painted(&fb, 0, 0, 96, 55), 0);

    Hero h;
    hero_init(&h, 48.0f, 80.0f);
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(same(fb_get(&fb, 48, 62), pal[COL_HERO_SKIN]));   /* head */
    CHECK(same(fb_get(&fb, 48, 70), pal[COL_HERO_BODY]));   /* torso */
    CHECK(painted(&fb, 40, 58, 20, 23) > 60);
    CHECK_EQ(painted(&fb, 0, 82, 96, 14), 0);               /* nothing below the feet */

    h.action = HERO_HIDDEN;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK_EQ(painted(&fb, 0, 0, 96, 96), 0);

    h.action = HERO_WINDUP;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(painted(&fb, 40, 48, 20, 10) > 0);                /* hammer raised above the head */

    h.action = HERO_IDLE; h.detective = 1; h.glass_x = 70.0f; h.glass_y = 80.0f;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(same(fb_get(&fb, 48, 58), pal[COL_HAT]));
    CHECK(painted(&fb, 64, 74, 13, 13) > 10);               /* glass ring */

    Fx f = { 1, FX_PLUS1, 40.0f, 60.0f, 0.0f };
    draw_clear(&fb, pal[COL_BG]);
    sprite_fx(&fb, pal, &f);
    CHECK(painted(&fb, 30, 40, 24, 20) > 5);
    f.kind = FX_DUST; f.t = 0.2f;
    draw_clear(&fb, pal[COL_BG]);
    sprite_fx(&fb, pal, &f);
    CHECK(painted(&fb, 30, 45, 24, 18) >= 4);

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_GLITCH);
    CHECK(same(fb_get(&fb, 42, 55), pal[COL_RED]));
    CHECK(painted(&fb, 38, 42, 20, 6) > 10);                /* COUNT label above */
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_GREEN);
    CHECK(same(fb_get(&fb, 42, 55), pal[COL_GREEN]));
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_LOCKED);
    CHECK(painted(&fb, 45, 52, 7, 6) > 0);                  /* padlock on the box front */

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_FOOTPRINT, 20, 70, 0);
    CHECK(same(fb_get(&fb, 20, 70), pal[COL_PROP_DARK]));
    sprite_prop(&fb, pal, PROP_FOOTPRINT, 30, 70, 1);
    CHECK(same(fb_get(&fb, 30, 70), pal[COL_GLOW]));

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_SHIELD, 48, 60, 0);
    CHECK(painted(&fb, 44, 50, 9, 11) > 20);
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_HIDING_SPOT, 48, 60, 0);
    CHECK(painted(&fb, 42, 51, 12, 10) > 20);
}
```

Add `void test_sprites(void);` and `RUN(test_sprites);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `sprites.h` not found.

- [ ] **Step 3: Implement**

`sim/sprites.h`:

```c
#ifndef SPRITES_H
#define SPRITES_H
#include "draw.h"
#include "world.h"

enum { PROP_BOX = 0, PROP_PADLOCK, PROP_FOOTPRINT, PROP_HIDING_SPOT, PROP_SHIELD };
enum { BOX_NORMAL = 0, BOX_GLITCH = 1, BOX_LOCKED = 2, BOX_GREEN = 3 };

void sprite_bug(Framebuffer *fb, const Color *pal, const Bug *b);
void sprite_hero(Framebuffer *fb, const Color *pal, const Hero *h);
void sprite_fx(Framebuffer *fb, const Color *pal, const Fx *f);
void sprite_prop(Framebuffer *fb, const Color *pal, int kind, int x, int y, int state);
void sprite_glass(Framebuffer *fb, const Color *pal, int cx, int cy);

#endif
```

`sim/sprites.c`:

```c
#include "sprites.h"
#include "palette.h"
#include "font.h"

static int fi(float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); }

void sprite_bug(Framebuffer *fb, const Color *pal, const Bug *b) {
    if (b->state == BUG_DEAD || b->state == BUG_POOF) return;
    if (b->hidden && !b->revealed) return;
    int x = fi(b->x), y = fi(b->y);
    if (b->state == BUG_SQUASHED) { draw_rect(fb, x - 6, y - 2, 12, 2, pal[COL_BUG_DARK]); return; }
    Color body = pal[b->variant == 0 ? COL_BUG_A : b->variant == 1 ? COL_BUG_B : COL_BUG_C];
    draw_rect(fb, x - 5, y - 7, 10, 6, body);                 /* round-ish body */
    draw_pixel(fb, x - 5, y - 7, pal[COL_BG]); draw_pixel(fb, x + 4, y - 7, pal[COL_BG]);
    draw_pixel(fb, x - 5, y - 2, pal[COL_BG]); draw_pixel(fb, x + 4, y - 2, pal[COL_BG]);
    draw_pixel(fb, x - 3, y - 6, pal[COL_BUG_DARK]);          /* eyes */
    draw_pixel(fb, x + 2, y - 6, pal[COL_BUG_DARK]);
    draw_pixel(fb, x - 3, y - 8, body); draw_pixel(fb, x - 4, y - 9, body);   /* antennae */
    draw_pixel(fb, x + 2, y - 8, body); draw_pixel(fb, x + 3, y - 9, body);
    draw_pixel(fb, x - 4, y - 1, pal[COL_BUG_DARK]);          /* stub legs */
    draw_pixel(fb, x - 1, y - 1, pal[COL_BUG_DARK]);
    draw_pixel(fb, x + 2, y - 1, pal[COL_BUG_DARK]);
}

static void hammer(Framebuffer *fb, const Color *pal, int x, int y, int facing, int action) {
    if (action == HERO_WINDUP) {                               /* raised above the head */
        draw_rect(fb, x + facing * 3, y - 26, 1, 8, pal[COL_HANDLE]);
        draw_rect(fb, x + facing * 3 - 2, y - 30, 5, 4, pal[COL_HAMMER]);
    } else if (action == HERO_BONK) {                          /* down in front */
        int hx = facing > 0 ? x + 5 : x - 11;
        draw_rect(fb, hx, y - 8, 7, 1, pal[COL_HANDLE]);
        draw_rect(fb, facing > 0 ? x + 11 : x - 15, y - 9, 4, 5, pal[COL_HAMMER]);
    } else {                                                   /* resting at the side */
        draw_rect(fb, x + facing * 5, y - 14, 1, 8, pal[COL_HANDLE]);
        draw_rect(fb, x + facing * 5 - 2, y - 18, 5, 4, pal[COL_HAMMER]);
    }
}

void sprite_hero(Framebuffer *fb, const Color *pal, const Hero *h) {
    if (h->action == HERO_HIDDEN) return;
    int x = fi(h->x), y = fi(h->y);
    int sit = h->action == HERO_SIT ? 4 : 0;
    if (sit) draw_rect(fb, x - 3, y - 2, 8, 2, pal[COL_PROP_DARK]);          /* legs out */
    else { draw_rect(fb, x - 3, y - 6, 2, 6, pal[COL_PROP_DARK]); draw_rect(fb, x + 1, y - 6, 2, 6, pal[COL_PROP_DARK]); }
    draw_rect(fb, x - 4, y - 14 + sit, 8, 8, pal[COL_HERO_BODY]);           /* torso */
    draw_rect(fb, x - 3, y - 20 + sit, 6, 6, pal[COL_HERO_SKIN]);           /* head */
    draw_pixel(fb, x + h->facing * 2, y - 18 + sit, pal[COL_BUG_DARK]);      /* eye */
    if (h->detective) {
        draw_rect(fb, x - 4, y - 22 + sit, 8, 2, pal[COL_HAT]);
        draw_rect(fb, x - 2, y - 24 + sit, 4, 2, pal[COL_HAT]);
        sprite_glass(fb, pal, fi(h->glass_x), fi(h->glass_y));
    }
    if (h->action == HERO_THINK) draw_text(fb, x - 1, y - 28, "?", pal[COL_TEXT]);
    if (h->action != HERO_SIT) hammer(fb, pal, x, y, h->facing, h->action);
}

void sprite_glass(Framebuffer *fb, const Color *pal, int cx, int cy) {
    Color c = pal[COL_TEXT];
    draw_rect(fb, cx - 3, cy - 5, 7, 1, c); draw_rect(fb, cx - 3, cy + 5, 7, 1, c);
    draw_rect(fb, cx - 5, cy - 3, 1, 7, c); draw_rect(fb, cx + 5, cy - 3, 1, 7, c);
    draw_pixel(fb, cx - 4, cy - 4, c); draw_pixel(fb, cx + 4, cy - 4, c);
    draw_pixel(fb, cx - 4, cy + 4, c); draw_pixel(fb, cx + 4, cy + 4, c);
    draw_pixel(fb, cx + 5, cy + 5, pal[COL_HANDLE]); draw_pixel(fb, cx + 6, cy + 6, pal[COL_HANDLE]);
    draw_pixel(fb, cx + 7, cy + 7, pal[COL_HANDLE]);
}

void sprite_fx(Framebuffer *fb, const Color *pal, const Fx *f) {
    if (!f->active) return;
    int x = fi(f->x), y = fi(f->y);
    if (f->kind == FX_PLUS1) {
        draw_text(fb, x - 3, y - 6 - fi(f->t * 10.0f), "+1", pal[COL_GREEN]);
    } else if (f->kind == FX_DUST) {
        int r = 2 + fi(f->t * 10.0f);
        draw_pixel(fb, x - r, y - 1 - r / 2, pal[COL_DUST]); draw_pixel(fb, x + r, y - 1 - r / 2, pal[COL_DUST]);
        draw_pixel(fb, x - r / 2, y - r, pal[COL_DUST]);     draw_pixel(fb, x + r / 2, y - r, pal[COL_DUST]);
    } else {                                                   /* FX_POOF: expanding hollow square */
        int s = 2 + fi(f->t * 8.0f), x0 = x - s / 2, y0 = y - 4 - s / 2;
        draw_rect(fb, x0, y0, s, 1, pal[COL_DUST]); draw_rect(fb, x0, y0 + s - 1, s, 1, pal[COL_DUST]);
        draw_rect(fb, x0, y0, 1, s, pal[COL_DUST]); draw_rect(fb, x0 + s - 1, y0, 1, s, pal[COL_DUST]);
    }
}

static void padlock(Framebuffer *fb, const Color *pal, int x, int y) {   /* y = bottom of the lock */
    draw_rect(fb, x - 2, y - 4, 5, 4, pal[COL_HAMMER]);
    draw_rect(fb, x - 1, y - 6, 1, 2, pal[COL_HANDLE]); draw_rect(fb, x + 1, y - 6, 1, 2, pal[COL_HANDLE]);
    draw_pixel(fb, x, y - 6, pal[COL_HANDLE]);
}

void sprite_prop(Framebuffer *fb, const Color *pal, int kind, int x, int y, int state) {
    switch (kind) {
    case PROP_BOX: {
        Color c = pal[state == BOX_GLITCH ? COL_RED : state == BOX_GREEN ? COL_GREEN : COL_PROP];
        draw_rect(fb, x - 7, y - 10, 14, 10, c);
        draw_rect(fb, x - 5, y - 8, 10, 6, pal[COL_BG]);
        draw_text(fb, x - 9, y - 17, "COUNT", pal[COL_TEXT]);
        if (state == BOX_LOCKED) padlock(fb, pal, x, y - 2);
        break;
    }
    case PROP_PADLOCK: padlock(fb, pal, x, y); break;
    case PROP_FOOTPRINT:
        draw_rect(fb, x, y, 2, 1, pal[state ? COL_GLOW : COL_PROP_DARK]);
        break;
    case PROP_HIDING_SPOT:
        draw_rect(fb, x - 5, y - 6, 10, 6, pal[COL_PROP_DARK]);
        draw_rect(fb, x - 3, y - 8, 6, 2, pal[COL_PROP_DARK]);
        draw_pixel(fb, x - 3, y - 7, pal[COL_PROP]);
        break;
    case PROP_SHIELD:
        draw_rect(fb, x - 3, y - 9, 7, 7, pal[COL_PROP]);
        draw_rect(fb, x - 2, y - 2, 5, 2, pal[COL_PROP]);
        draw_pixel(fb, x - 2, y - 5, pal[COL_GREEN]); draw_pixel(fb, x - 1, y - 4, pal[COL_GREEN]);
        draw_pixel(fb, x, y - 5, pal[COL_GREEN]);     draw_pixel(fb, x + 1, y - 6, pal[COL_GREEN]);
        draw_pixel(fb, x + 2, y - 7, pal[COL_GREEN]);
        break;
    default: break;
    }
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`. If a geometry assertion fails, adjust the sprite, not the test, unless the test's box is plainly wrong; the tests describe the silhouette the rest of the plan assumes.

- [ ] **Step 5: Commit**

```bash
git add sim/sprites.h sim/sprites.c test/test_sprites.c test/test_main.c
git commit -m "Add placeholder sprites for bug, hero, fx, and props

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 10: Stage struct and fixed-step accumulator

**Files:**
- Create: `sim/stage.h`, `sim/stage.c`, `test/test_stage.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `Stage {fb, accum, palette, step, render, state}`, `STAGE_STEP (1.0f/60.0f)`, `STAGE_MAX_DT 0.25f`, `stage_init(stage, px, w, h, palette, step_fn, render_fn, state)`, `int stage_update(stage, elapsed_ms)` returning the number of fixed steps run, `stage_render(stage)`.

- [ ] **Step 1: Write the failing tests**

`test/test_stage.c`:

```c
#include "test.h"
#include "stage.h"
#include "palette.h"

static int steps, renders;
static float last_dt;
static void step_fn(Stage *s, float dt) { (void)s; steps++; last_dt = dt; }
static void render_fn(Stage *s) { (void)s; renders++; }
static uint8_t px[8 * 8 * 4];

void test_stage(void) {
    Stage st;
    int marker = 7;
    stage_init(&st, px, 8, 8, PALETTE_BUGS, step_fn, render_fn, &marker);
    CHECK_EQ(st.fb.w, 8);
    CHECK(st.palette == PALETTE_BUGS);
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
```

Add `void test_stage(void);` and `RUN(test_stage);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `stage.h` not found.

- [ ] **Step 3: Implement**

`sim/stage.h`:

```c
#ifndef STAGE_H
#define STAGE_H
#include <stdint.h>
#include "draw.h"

#define STAGE_STEP (1.0f / 60.0f)
#define STAGE_MAX_DT 0.25f

typedef struct Stage Stage;
struct Stage {
    Framebuffer fb;
    float accum;
    const Color *palette;
    void (*step)(Stage *s, float dt);
    void (*render)(Stage *s);
    void *state;
};

void stage_init(Stage *s, uint8_t *px, int w, int h, const Color *palette,
                void (*step)(Stage *, float), void (*render)(Stage *), void *state);
int  stage_update(Stage *s, int elapsed_ms);
void stage_render(Stage *s);

#endif
```

`sim/stage.c`:

```c
#include "stage.h"

void stage_init(Stage *s, uint8_t *px, int w, int h, const Color *palette,
                void (*step)(Stage *, float), void (*render)(Stage *), void *state) {
    fb_init(&s->fb, px, w, h);
    s->accum = 0.0f; s->palette = palette; s->step = step; s->render = render; s->state = state;
}

int stage_update(Stage *s, int elapsed_ms) {
    if (elapsed_ms <= 0) return 0;
    float dt = (float)elapsed_ms / 1000.0f;
    if (dt > STAGE_MAX_DT) dt = STAGE_MAX_DT;
    s->accum += dt;
    int n = 0;
    while (s->accum >= STAGE_STEP) {
        s->step(s, STAGE_STEP);
        s->accum -= STAGE_STEP;
        n++;
    }
    return n;
}

void stage_render(Stage *s) { s->render(s); }
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/stage.h sim/stage.c test/test_stage.c test/test_main.c
git commit -m "Add Stage struct with fixed-step accumulator

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 11: Vignette interface and the Patrol vignette

**Files:**
- Create: `sim/vignette.h`, `sim/vignettes/patrol.h`, `sim/vignettes/patrol.c`, `test/test_patrol.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `Vignette {id, enter, update, draw_back, draw_front, at_beat_boundary, is_done, exit}`; ids `VIG_PATROL 0, VIG_RACE 1, VIG_DETECTIVE 2, VIG_REGRESSION 3, VIG_CROSSOVER 4, VIG_COUNT 5`; `vignette_is_linked(id)`. `extern const Vignette vignette_patrol;` Patrol spawns a bug per side every 4 to 8 s up to the cap, walks the hero to the nearest wandering bug on its side, bonks it, and rests 2.5 to 4.5 s. `at_beat_boundary` is true whenever the hero is not mid-swing. `is_done` is always false.
- Convention for every vignette: `update` runs **after** `world_update_actors` in the same step, so `hero.bonk_landed` from this step is visible.

- [ ] **Step 1: Write the failing tests**

`test/test_patrol.c`:

```c
#include "test.h"
#include "vignettes/patrol.h"

#define DT (1.0f / 60.0f)

static void run(World *w, const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) { world_update_actors(w, DT); v->update(w, DT); }
}

void test_patrol(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 11u, 96, 240, 200, &ev);
    for (int i = 0; i < 4; i++) { world_spawn_bug(&w, SIDE_LEFT); world_spawn_bug(&w, SIDE_RIGHT); }

    const Vignette *v = &vignette_patrol;
    CHECK_EQ(v->id, VIG_PATROL);
    CHECK(!vignette_is_linked(VIG_PATROL));
    CHECK(vignette_is_linked(VIG_RACE));
    CHECK(vignette_is_linked(VIG_REGRESSION));
    CHECK(!vignette_is_linked(VIG_CROSSOVER));

    v->enter(&w);
    CHECK(!v->is_done(&w));
    CHECK(v->at_beat_boundary(&w));

    /* within 10 seconds the hero has squashed something and was mid-swing at some point */
    int saw_busy_boundary_false = 0, saw_busy = 0;
    for (int i = 0; i < 600; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (hero_busy(&w.hero)) { saw_busy = 1; if (!v->at_beat_boundary(&w)) saw_busy_boundary_false = 1; }
    }
    CHECK(saw_busy);
    CHECK(saw_busy_boundary_false);
    CHECK(w.squashed_total >= 1);
    CHECK(events_count(&ev) >= 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);

    /* cadence: at least 4 squashes in the first 30 seconds, never more than cap alive */
    run(&w, v, 1200);
    CHECK(w.squashed_total >= 4);
    CHECK(world_bug_count(&w, SIDE_LEFT) <= MAX_BUGS_PER_SIDE);
    CHECK(world_bug_count(&w, SIDE_RIGHT) <= MAX_BUGS_PER_SIDE);

    /* spawning: the unattended right side grows over time */
    int right_before = world_bug_count(&w, SIDE_RIGHT);
    run(&w, v, 1800);
    CHECK(world_bug_count(&w, SIDE_RIGHT) > right_before || world_bug_count(&w, SIDE_RIGHT) == MAX_BUGS_PER_SIDE);

    /* the hero stays on the left; patrol never crosses */
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK(!v->is_done(&w));
    v->exit(&w);
}
```

Add `void test_patrol(void);` and `RUN(test_patrol);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `vignettes/patrol.h` not found.

- [ ] **Step 3: Implement**

`sim/vignette.h`:

```c
#ifndef VIGNETTE_H
#define VIGNETTE_H
#include "world.h"
#include "draw.h"

enum { VIG_PATROL = 0, VIG_RACE = 1, VIG_DETECTIVE = 2, VIG_REGRESSION = 3, VIG_CROSSOVER = 4, VIG_COUNT = 5 };

typedef struct {
    int id;
    void (*enter)(World *w);
    void (*update)(World *w, float dt);                                  /* runs after world_update_actors */
    void (*draw_back)(World *w, Framebuffer *fb, const Color *pal);      /* before actors; may be NULL */
    void (*draw_front)(World *w, Framebuffer *fb, const Color *pal);     /* after actors; may be NULL */
    int  (*at_beat_boundary)(const World *w);
    int  (*is_done)(const World *w);
    void (*exit)(World *w);
} Vignette;

static inline int vignette_is_linked(int id) { return id >= VIG_RACE && id <= VIG_REGRESSION; }

#endif
```

`sim/vignettes/patrol.h`:

```c
#ifndef PATROL_H
#define PATROL_H
#include "vignette.h"
extern const Vignette vignette_patrol;
#endif
```

`sim/vignettes/patrol.c`:

```c
#include "vignettes/patrol.h"
#include "fmath.h"

#define PATROL_SPAWN_MIN 4.0f
#define PATROL_SPAWN_SPAN 4.0f
#define PATROL_REST_MIN 2.5f
#define PATROL_REST_SPAN 2.0f

static struct {
    float next_spawn[2];
    float rest;
    Bug *target;
} S;

static void enter(World *w) {
    for (int s = 0; s < 2; s++) S.next_spawn[s] = PATROL_SPAWN_MIN + rng_float(&w->rng) * PATROL_SPAWN_SPAN;
    S.rest = 0.5f;
    S.target = 0;
}

static void update(World *w, float dt) {
    for (int s = 0; s < 2; s++) {
        S.next_spawn[s] -= dt;
        if (S.next_spawn[s] <= 0.0f) {
            world_spawn_bug(w, s);
            S.next_spawn[s] = PATROL_SPAWN_MIN + rng_float(&w->rng) * PATROL_SPAWN_SPAN;
        }
    }
    Hero *h = &w->hero;
    if (h->action == HERO_HIDDEN || h->action == HERO_SIT) return;
    if (hero_busy(h)) {
        if (h->bonk_landed) {
            world_squash_near(w, h->x + (float)h->facing * 6.0f, h->y, HERO_REACH);
            S.rest = PATROL_REST_MIN + rng_float(&w->rng) * PATROL_REST_SPAN;
            S.target = 0;
        }
        return;
    }
    if (S.rest > 0.0f) { S.rest -= dt; return; }
    int side = world_hero_side(w);
    if (side == SIDE_GAP) return;
    if (!S.target || S.target->state != BUG_WANDER) S.target = world_nearest_bug(w, side, h->x, h->y);
    if (!S.target) return;
    float dx = S.target->x - h->x, dy = S.target->y - h->y;
    if (fm_abs(dx) <= HERO_REACH - 1.0f && fm_abs(dy) <= 3.0f) {
        h->facing = dx < 0 ? -1 : 1;
        hero_start_bonk(h);
    } else {
        hero_walk_to(h, S.target->x - (dx < 0 ? -6.0f : 6.0f), S.target->y);
    }
}

static int at_beat_boundary(const World *w) { return !hero_busy(&w->hero); }
static int is_done(const World *w) { (void)w; return 0; }
static void exit_(World *w) { (void)w; S.target = 0; }

const Vignette vignette_patrol = {
    VIG_PATROL, enter, update, 0, 0, at_beat_boundary, is_done, exit_
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/vignette.h sim/vignettes/patrol.h sim/vignettes/patrol.c test/test_patrol.c test/test_main.c
git commit -m "Add vignette interface and patrol vignette

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 12: Scene state machine

**Files:**
- Create: `sim/scene.h`, `sim/scene.c`, `test/test_scene.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `Scene` with `table`, `world`, `current`, `current_auto`, `pending` (-1 none), `pending_auto`, `idle_timer`, `last_auto`, `since_crossover`; constants `SCENE_IDLE_AUTOPLAY 45.0f`, `SCENE_CROSSOVER_INTERVAL 120.0f`, `SCENE_CROSSOVER_IMBALANCE 6`; `scene_init(scene, world, table)` enters patrol; `scene_request(scene, id)` accepts only linked ids and resets the idle timer; `scene_step(scene, dt)` called after `world_update_actors`; `scene_draw_back` / `scene_draw_front` forward to the current vignette.
- Consumes: `Vignette` interface from Task 11; `world_bug_count`, `world_hero_side`; `events_push`, `event_pack`.
- The table is injected so tests can use fakes; production passes `VIGNETTES` from Task 17.

- [ ] **Step 1: Write the failing tests**

`test/test_scene.c`:

```c
#include "test.h"
#include "scene.h"

#define DT (1.0f / 60.0f)

static int fake_boundary = 1, fake_done = 0;
static int enters[VIG_COUNT], exits[VIG_COUNT];

#define FAKE(n) \
    static void enter##n(World *w) { (void)w; enters[n]++; } \
    static void exit##n(World *w) { (void)w; exits[n]++; }
FAKE(0) FAKE(1) FAKE(2) FAKE(3) FAKE(4)

static void upd(World *w, float dt) { (void)w; (void)dt; }
static int boundary(const World *w) { (void)w; return fake_boundary; }
static int never_done(const World *w) { (void)w; return 0; }
static int done(const World *w) { (void)w; return fake_done; }

static const Vignette F0 = { VIG_PATROL, enter0, upd, 0, 0, boundary, never_done, exit0 };
static const Vignette F1 = { VIG_RACE, enter1, upd, 0, 0, boundary, done, exit1 };
static const Vignette F2 = { VIG_DETECTIVE, enter2, upd, 0, 0, boundary, done, exit2 };
static const Vignette F3 = { VIG_REGRESSION, enter3, upd, 0, 0, boundary, done, exit3 };
static const Vignette F4 = { VIG_CROSSOVER, enter4, upd, 0, 0, boundary, done, exit4 };
static const Vignette *const TABLE[VIG_COUNT] = { &F0, &F1, &F2, &F3, &F4 };

static World w;
static EventQueue ev;
static Scene sc;

static void fresh(uint32_t seed) {
    events_init(&ev);
    world_init(&w, seed, 96, 240, 200, &ev);
    memset(enters, 0, sizeof enters); memset(exits, 0, sizeof exits);
    fake_boundary = 1; fake_done = 0;
    scene_init(&sc, &w, TABLE);
}

static void steps(int n) { for (int i = 0; i < n; i++) { world_update_actors(&w, DT); scene_step(&sc, DT); } }

/* run until a START event or the step budget runs out; returns packed event or 0 */
static uint32_t until_start(int budget) {
    for (int i = 0; i < budget; i++) {
        steps(1);
        uint32_t e;
        while ((e = events_pop(&ev)) != 0) if (event_type(e) == EV_VIGNETTE_START) return e;
    }
    return 0;
}

void test_scene(void) {
    fresh(1u);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(enters[0], 1);

    /* non-linked ids are ignored */
    scene_request(&sc, VIG_PATROL); scene_request(&sc, VIG_CROSSOVER); scene_request(&sc, 9); scene_request(&sc, -1);
    CHECK_EQ(sc.pending, -1);

    /* a request mid-beat waits; at a boundary it swaps and emits START */
    fake_boundary = 0;
    scene_request(&sc, VIG_RACE);
    steps(5);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(sc.pending, VIG_RACE);
    CHECK_EQ(events_count(&ev), 0);
    fake_boundary = 1;
    steps(1);
    CHECK_EQ(sc.current, VIG_RACE);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(exits[0], 1); CHECK_EQ(enters[1], 1);
    uint32_t e = events_pop(&ev);
    CHECK_EQ(event_type(e), EV_VIGNETTE_START); CHECK_EQ(event_a(e), VIG_RACE); CHECK_EQ(event_b(e), 0);
    CHECK_EQ(events_count(&ev), 0);

    /* a request for the current vignette is ignored */
    scene_request(&sc, VIG_RACE);
    steps(1);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(events_count(&ev), 0);

    /* a newer pending request replaces an older one; the interrupted vignette emits END */
    fake_boundary = 0;
    scene_request(&sc, VIG_DETECTIVE);
    scene_request(&sc, VIG_REGRESSION);
    CHECK_EQ(sc.pending, VIG_REGRESSION);
    fake_boundary = 1;
    steps(1);
    CHECK_EQ(sc.current, VIG_REGRESSION);
    CHECK_EQ(enters[2], 0); CHECK_EQ(enters[3], 1); CHECK_EQ(exits[1], 1);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), VIG_RACE);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_START); CHECK_EQ(event_a(e), VIG_REGRESSION);

    /* a finished vignette returns to patrol with END, and patrol emits nothing */
    fake_done = 1;
    steps(1);
    fake_done = 0;
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(exits[3], 1); CHECK_EQ(enters[0], 2);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), VIG_REGRESSION);
    CHECK_EQ(events_count(&ev), 0);

    /* idle auto-play: fires after 45s of no requests, flagged auto, never the same twice in a row */
    fresh(2u);
    int prev = -1;
    for (int round = 0; round < 6; round++) {
        steps(44 * 60);
        CHECK_EQ(sc.current, VIG_PATROL);
        uint32_t s = until_start(3 * 60);
        CHECK(s != 0u);
        CHECK_EQ(event_b(s), 1);
        CHECK(vignette_is_linked(event_a(s)));
        CHECK(event_a(s) != prev);
        prev = event_a(s);
        fake_done = 1; steps(1); fake_done = 0;
        while (events_pop(&ev)) {}
        sc.since_crossover = 0.0f;              /* keep the crossover timer out of this test */
    }

    /* an external request resets the idle timer */
    fresh(3u);
    steps(40 * 60);
    scene_request(&sc, VIG_RACE);
    steps(1);
    fake_done = 1; steps(1); fake_done = 0;
    while (events_pop(&ev)) {}
    steps(40 * 60);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(events_count(&ev), 0);

    /* crossover after 120s in patrol, silently; a request during it waits until it is done */
    fresh(4u);
    sc.idle_timer = -1e6f;                      /* keep idle auto-play out of this test */
    steps(121 * 60);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    CHECK_EQ(enters[4], 1);
    CHECK_EQ(events_count(&ev), 0);
    scene_request(&sc, VIG_RACE);
    steps(10);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    fake_done = 1; steps(1); fake_done = 0;
    CHECK_EQ(sc.current, VIG_PATROL);
    steps(1);
    CHECK_EQ(sc.current, VIG_RACE);

    /* crossover on a bug imbalance of 6 */
    fresh(5u);
    sc.idle_timer = -1e6f;
    for (int i = 0; i < 6; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(1);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    fresh(6u);
    sc.idle_timer = -1e6f;
    for (int i = 0; i < 5; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(60);
    CHECK_EQ(sc.current, VIG_PATROL);

    /* never crossover during a linked vignette */
    fresh(7u);
    scene_request(&sc, VIG_RACE);
    steps(1);
    sc.since_crossover = 500.0f;
    for (int i = 0; i < 6; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(60);
    CHECK_EQ(sc.current, VIG_RACE);
}
```

Add `void test_scene(void);` and `RUN(test_scene);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `scene.h` not found.

- [ ] **Step 3: Implement**

`sim/scene.h`:

```c
#ifndef SCENE_H
#define SCENE_H
#include "vignette.h"

#define SCENE_IDLE_AUTOPLAY 45.0f
#define SCENE_CROSSOVER_INTERVAL 120.0f
#define SCENE_CROSSOVER_IMBALANCE 6

typedef struct {
    const Vignette *const *table;   /* VIG_COUNT entries indexed by id */
    World *world;
    int current, current_auto;
    int pending, pending_auto;      /* pending == -1 when none */
    float idle_timer;               /* seconds since the last external request */
    int last_auto;                  /* last auto-played id, -1 none */
    float since_crossover;
} Scene;

void scene_init(Scene *sc, World *w, const Vignette *const *table);
void scene_request(Scene *sc, int id);
void scene_step(Scene *sc, float dt);
void scene_draw_back(Scene *sc, Framebuffer *fb, const Color *pal);
void scene_draw_front(Scene *sc, Framebuffer *fb, const Color *pal);

#endif
```

`sim/scene.c`:

```c
#include "scene.h"

static void emit(Scene *sc, int type, int a, int b) {
    if (sc->world->events) events_push(sc->world->events, event_pack(STAGE_BUGS, type, a, b));
}

static void enter(Scene *sc, int id, int auto_flag) {
    sc->current = id; sc->current_auto = auto_flag;
    sc->table[id]->enter(sc->world);
}

static void leave(Scene *sc) {
    if (vignette_is_linked(sc->current)) emit(sc, EV_VIGNETTE_END, sc->current, 0);
    sc->table[sc->current]->exit(sc->world);
}

void scene_init(Scene *sc, World *w, const Vignette *const *table) {
    sc->table = table; sc->world = w;
    sc->pending = -1; sc->pending_auto = 0;
    sc->idle_timer = 0.0f; sc->last_auto = -1; sc->since_crossover = 0.0f;
    enter(sc, VIG_PATROL, 0);
}

void scene_request(Scene *sc, int id) {
    if (!vignette_is_linked(id)) return;
    sc->idle_timer = 0.0f;
    sc->pending = id; sc->pending_auto = 0;
}

static int pick_auto(Scene *sc) {
    int pick = VIG_RACE + rng_range(&sc->world->rng, 0, 2);
    if (pick == sc->last_auto) pick = VIG_RACE + ((pick - VIG_RACE + 1) % 3);
    return pick;
}

static int imbalance(const Scene *sc) {
    int side = world_hero_side(sc->world);
    if (side == SIDE_GAP) return 0;
    int near = world_bug_count(sc->world, side), far = world_bug_count(sc->world, 1 - side);
    return far - near >= SCENE_CROSSOVER_IMBALANCE;
}

void scene_step(Scene *sc, float dt) {
    World *w = sc->world;
    const Vignette *cur = sc->table[sc->current];
    cur->update(w, dt);
    sc->idle_timer += dt;
    sc->since_crossover += dt;

    if (cur->is_done(w)) {
        leave(sc);
        enter(sc, VIG_PATROL, 0);
        return;
    }
    if (sc->pending >= 0) {
        if (sc->pending == sc->current) { sc->pending = -1; }
        else if (sc->current != VIG_CROSSOVER && cur->at_beat_boundary(w)) {
            leave(sc);
            enter(sc, sc->pending, sc->pending_auto);
            emit(sc, EV_VIGNETTE_START, sc->current, sc->current_auto);
            sc->pending = -1;
            return;
        }
    }
    if (sc->current == VIG_PATROL && sc->pending < 0) {
        if (sc->idle_timer >= SCENE_IDLE_AUTOPLAY) {
            int pick = pick_auto(sc);
            sc->pending = pick; sc->pending_auto = 1; sc->last_auto = pick;
            sc->idle_timer = 0.0f;
        } else if (sc->since_crossover >= SCENE_CROSSOVER_INTERVAL || imbalance(sc)) {
            leave(sc);
            enter(sc, VIG_CROSSOVER, 0);
            sc->since_crossover = 0.0f;
        }
    }
}

void scene_draw_back(Scene *sc, Framebuffer *fb, const Color *pal) {
    const Vignette *cur = sc->table[sc->current];
    if (cur->draw_back) cur->draw_back(sc->world, fb, pal);
}

void scene_draw_front(Scene *sc, Framebuffer *fb, const Color *pal) {
    const Vignette *cur = sc->table[sc->current];
    if (cur->draw_front) cur->draw_front(sc->world, fb, pal);
}
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/scene.h sim/scene.c test/test_scene.c test/test_main.c
git commit -m "Add scene state machine with requests, idle auto-play, and crossover triggers

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 13: Crossover vignette

**Files:**
- Create: `sim/vignettes/crossover.h`, `sim/vignettes/crossover.c`, `test/test_crossover.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `extern const Vignette vignette_crossover;` phases `XO_WALK 0, XO_HIDDEN 1, XO_EMERGE 2, XO_DONE 3`; `int crossover_phase(void)`. The hero walks to just past the inner edge of its panel (so it is fully clipped), hides for `(gap_w - HERO_W) / HERO_WALK_SPEED` seconds, reappears just past the other panel's inner edge, and walks 14 px inward. `at_beat_boundary` and `is_done` are both true only in `XO_DONE`.

- [ ] **Step 1: Write the failing tests**

`test/test_crossover.c`:

```c
#include "test.h"
#include "vignettes/crossover.h"

#define DT (1.0f / 60.0f)

static void run(World *w, const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) { world_update_actors(w, DT); v->update(w, DT); }
}

void test_crossover(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 3u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_crossover;
    CHECK_EQ(v->id, VIG_CROSSOVER);
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);

    v->enter(&w);
    CHECK_EQ(crossover_phase(), XO_WALK);
    CHECK(!v->is_done(&w));
    CHECK(!v->at_beat_boundary(&w));

    /* hero starts at x=48, exit point is 96 + 7 = 103: 55px at 24px/s = 2.3s */
    run(&w, v, 2 * 60);
    CHECK_EQ(crossover_phase(), XO_WALK);
    run(&w, v, 1 * 60);
    CHECK_EQ(crossover_phase(), XO_HIDDEN);
    CHECK_EQ(w.hero.action, HERO_HIDDEN);
    CHECK_EQ(world_hero_side(&w), SIDE_GAP);

    /* hidden for (200 - 14) / 24 = 7.75s: count the hidden steps */
    int hidden_steps = 0;
    while (crossover_phase() == XO_HIDDEN && hidden_steps < 20 * 60) { run(&w, v, 1); hidden_steps++; }
    CHECK_EQ(crossover_phase(), XO_EMERGE);
    /* we entered HIDDEN somewhere in the last second, so allow that slack plus rounding */
    CHECK(hidden_steps >= 465 - 62 && hidden_steps <= 465 + 2);
    CHECK_EQ(world_hero_side(&w), SIDE_RIGHT);
    CHECK(w.hero.action != HERO_HIDDEN);

    /* emerge: from 289 to 310 is 21px, under a second */
    run(&w, v, 2 * 60);
    CHECK_EQ(crossover_phase(), XO_DONE);
    CHECK(v->is_done(&w));
    CHECK(v->at_beat_boundary(&w));
    CHECK_NEAR(w.hero.x, 96.0f + 200.0f + 14.0f, 0.5);
    v->exit(&w);

    /* from the right it goes back left */
    v->enter(&w);
    run(&w, v, 15 * 60);
    CHECK(v->is_done(&w));
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK_NEAR(w.hero.x, 96.0f - 14.0f, 0.5);
    v->exit(&w);
}
```

Add `void test_crossover(void);` and `RUN(test_crossover);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `vignettes/crossover.h` not found.

- [ ] **Step 3: Implement**

`sim/vignettes/crossover.h`:

```c
#ifndef CROSSOVER_H
#define CROSSOVER_H
#include "vignette.h"
enum { XO_WALK = 0, XO_HIDDEN = 1, XO_EMERGE = 2, XO_DONE = 3 };
extern const Vignette vignette_crossover;
int crossover_phase(void);
#endif
```

`sim/vignettes/crossover.c`:

```c
#include "vignettes/crossover.h"

static struct { int phase, from, to; float hidden_left; } S;

int crossover_phase(void) { return S.phase; }

static float exit_x(const World *w, int from) {
    return from == SIDE_LEFT ? (float)(w->panel_w + HERO_W / 2) : (float)(w->panel_w + w->gap_w - HERO_W / 2);
}
static float enter_x(const World *w, int to) {
    return to == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w - HERO_W / 2) : (float)(w->panel_w + HERO_W / 2);
}
static float final_x(const World *w, int to) {
    return to == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w + 14) : (float)(w->panel_w - 14);
}

static void enter(World *w) {
    S.from = world_hero_side(w);
    if (S.from == SIDE_GAP) S.from = SIDE_LEFT;
    S.to = 1 - S.from;
    S.phase = XO_WALK;
    S.hidden_left = 0.0f;
    hero_walk_to(&w->hero, exit_x(w, S.from), w->hero.y);
}

static void update(World *w, float dt) {
    Hero *h = &w->hero;
    switch (S.phase) {
    case XO_WALK:
        if (hero_arrived(h)) {
            hero_hide(h);
            float span = (float)(w->gap_w - HERO_W);
            S.hidden_left = span > 0 ? span / HERO_WALK_SPEED : 0.0f;
            S.phase = XO_HIDDEN;
        }
        break;
    case XO_HIDDEN:
        S.hidden_left -= dt;
        if (S.hidden_left <= 0.0f) {
            hero_show(h, enter_x(w, S.to), h->y);
            hero_walk_to(h, final_x(w, S.to), h->y);
            S.phase = XO_EMERGE;
        }
        break;
    case XO_EMERGE:
        if (hero_arrived(h)) S.phase = XO_DONE;
        break;
    default: break;
    }
}

static int at_beat_boundary(const World *w) { (void)w; return S.phase == XO_DONE; }
static int is_done(const World *w) { (void)w; return S.phase == XO_DONE; }
static void exit_(World *w) { (void)w; }

const Vignette vignette_crossover = {
    VIG_CROSSOVER, enter, update, 0, 0, at_beat_boundary, is_done, exit_
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/vignettes/crossover.h sim/vignettes/crossover.c test/test_crossover.c test/test_main.c
git commit -m "Add crossover vignette

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 14: Race condition vignette

**Files:**
- Create: `sim/vignettes/race.h`, `sim/vignettes/race.c`, `test/test_race.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `extern const Vignette vignette_race;` phases `RACE_SETUP 0, RACE_RUSH 1, RACE_COLLIDE 2, RACE_LOCK 3, RACE_QUEUE 4, RACE_RESOLVED 5, RACE_DONE 6` with durations 1.0, 1.5, 2.0, 2.0, 3.0, 0.5 s; `int race_phase(void)`, `int race_box_state(void)`, `int race_value(void)`. `at_beat_boundary` is true only on the step a phase changes.
- Consumes: `sprite_prop(PROP_BOX, ...)`, `draw_text`, `world_spawn_bug_at`, `bug_set_target`, `world_clear_scripted_bugs`, `world_spawn_fx(FX_POOF)`.

- [ ] **Step 1: Write the failing tests**

`test/test_race.c`:

```c
#include "test.h"
#include "vignettes/race.h"
#include "sprites.h"
#include "palette.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

void test_race(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 8u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_race;
    CHECK_EQ(v->id, VIG_RACE);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(race_phase(), RACE_SETUP);
    CHECK_EQ(race_box_state(), BOX_NORMAL);
    CHECK(!v->is_done(&w));

    run(v, 30);  CHECK_EQ(race_phase(), RACE_SETUP);        /* t = 0.5 */
    run(v, 42);  CHECK_EQ(race_phase(), RACE_RUSH);         /* t = 1.2 */
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 2);
    run(v, 108); CHECK_EQ(race_phase(), RACE_COLLIDE);      /* t = 3.0 */
    CHECK_EQ(race_box_state(), BOX_GLITCH);
    run(v, 120); CHECK_EQ(race_phase(), RACE_LOCK);         /* t = 5.0 */
    run(v, 120); CHECK_EQ(race_phase(), RACE_QUEUE);        /* t = 7.0 */
    CHECK_EQ(race_box_state(), BOX_LOCKED);
    CHECK_EQ(race_value(), 0);
    run(v, 162); CHECK_EQ(race_phase(), RACE_RESOLVED);     /* t = 9.7 */
    CHECK_EQ(race_box_state(), BOX_GREEN);
    CHECK_EQ(race_value(), 3);
    CHECK(!v->is_done(&w));
    run(v, 30);  CHECK_EQ(race_phase(), RACE_DONE);         /* t = 10.2 */
    CHECK(v->is_done(&w));
    CHECK_EQ(boundaries, 6);                                 /* one boundary per phase change */

    /* the box is drawn into the panel */
    static uint8_t px[192 * 240 * 4];
    Framebuffer fb;
    fb_init(&fb, px, 192, 240);
    draw_clear(&fb, PALETTE_BUGS[COL_BG]);
    fb_set_view(&fb, 0, 0, 96, 240, 0, 0);
    v->exit(&w);
    v->enter(&w);
    run(v, 200);                                             /* mid-COLLIDE: red box */
    v->draw_back(&w, &fb, PALETTE_BUGS);
    int red = 0;
    for (int y = 0; y < 240; y++) for (int x = 0; x < 96; x++) {
        Color c = fb_get(&fb, x, y);
        if (c.r == PALETTE_BUGS[COL_RED].r && c.g == PALETTE_BUGS[COL_RED].g && c.b == PALETTE_BUGS[COL_RED].b) red++;
    }
    CHECK(red > 30);

    v->exit(&w);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);              /* scripted bugs are cleared */
}
```

Add `void test_race(void);` and `RUN(test_race);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `vignettes/race.h` not found.

- [ ] **Step 3: Implement**

`sim/vignettes/race.h`:

```c
#ifndef RACE_H
#define RACE_H
#include "vignette.h"
enum { RACE_SETUP = 0, RACE_RUSH, RACE_COLLIDE, RACE_LOCK, RACE_QUEUE, RACE_RESOLVED, RACE_DONE };
extern const Vignette vignette_race;
int race_phase(void);
int race_box_state(void);
int race_value(void);
#endif
```

`sim/vignettes/race.c`:

```c
#include "vignettes/race.h"
#include "sprites.h"
#include "font.h"
#include "fmath.h"

static const float DURATION[] = { 1.0f, 1.5f, 2.0f, 2.0f, 3.0f, 0.5f };

static struct {
    int phase, boundary, side;
    float t, glitch_t;
    float box_x, box_y;
    int box_state, value, jitter;
    Bug *q[3];
    int q_started[3], q_counted[3];
} S;

int race_phase(void) { return S.phase; }
int race_box_state(void) { return S.box_state; }
int race_value(void) { return S.value; }

static float x0(const World *w) { return world_panel_x0(w, S.side); }

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f; S.boundary = 1;
    switch (phase) {
    case RACE_RUSH:
        S.q[0] = world_spawn_bug_at(w, S.side, x0(w) + 6.0f, S.box_y);
        S.q[1] = world_spawn_bug_at(w, S.side, x0(w) + (float)w->panel_w - 6.0f, S.box_y);
        if (S.q[0]) bug_set_target(S.q[0], S.box_x - 8.0f, S.box_y);
        if (S.q[1]) bug_set_target(S.q[1], S.box_x + 8.0f, S.box_y);
        break;
    case RACE_COLLIDE:
        S.box_state = BOX_GLITCH; S.glitch_t = 0.0f;
        break;
    case RACE_LOCK:
        hero_walk_to(&w->hero, S.box_x - 14.0f, S.box_y + 4.0f);
        /* the two racers back off into a queue line; a third joins */
        if (S.q[0]) bug_set_target(S.q[0], S.box_x - 20.0f, S.box_y);
        if (S.q[1]) bug_set_target(S.q[1], S.box_x - 30.0f, S.box_y);
        S.q[2] = world_spawn_bug_at(w, S.side, x0(w) + 6.0f, S.box_y);
        if (S.q[2]) bug_set_target(S.q[2], S.box_x - 40.0f, S.box_y);
        break;
    case RACE_QUEUE:
        S.box_state = BOX_LOCKED; S.value = 0; S.jitter = 0;
        for (int i = 0; i < 3; i++) { S.q_started[i] = 0; S.q_counted[i] = 0; }
        break;
    case RACE_RESOLVED:
        S.box_state = BOX_GREEN;
        break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    S.box_x = world_panel_center_x(w, S.side);
    S.box_y = (float)((w->floor_top + w->floor_bottom) / 2);
    S.box_state = BOX_NORMAL; S.value = 0; S.jitter = 0;
    for (int i = 0; i < 3; i++) S.q[i] = 0;
    hero_walk_to(&w->hero, x0(w) + 16.0f, S.box_y + 12.0f);
    S.phase = RACE_SETUP; S.t = 0.0f; S.boundary = 0;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == RACE_DONE) return;
    S.t += dt;

    if (S.phase == RACE_COLLIDE) {
        S.glitch_t += dt;
        if (S.glitch_t >= 0.1f) { S.glitch_t = 0.0f; S.value = rng_range(&w->rng, 0, 9); S.jitter = rng_range(&w->rng, -1, 1); }
    }
    if (S.phase == RACE_QUEUE) {
        for (int i = 0; i < 3; i++) {
            Bug *b = S.q[i];
            if (!b) continue;
            /* one bug every 0.7s; each needs at most 31px / 30px/s, so all three finish inside the 3s phase */
            if (!S.q_started[i] && S.t >= (float)i * 0.7f) { S.q_started[i] = 1; bug_set_target(b, S.box_x - 9.0f, S.box_y); }
            if (S.q_started[i] && !S.q_counted[i] && b->state == BUG_WAIT && fm_abs(b->x - (S.box_x - 9.0f)) < 0.5f) {
                S.q_counted[i] = 1; S.value++;
                b->state = BUG_POOF; b->timer = BUG_POOF_TIME;
                world_spawn_fx(w, FX_POOF, b->x, b->y);
            }
        }
    }
    if (S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1);
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    (void)w;
    if (S.phase >= RACE_DONE) return;
    int bx = (int)S.box_x + (S.box_state == BOX_GLITCH ? S.jitter : 0), by = (int)S.box_y;
    sprite_prop(fb, pal, PROP_BOX, bx, by, S.box_state);
    char digit[2] = { (char)('0' + S.value % 10), 0 };
    draw_text(fb, bx - 1, by - 7, digit, pal[S.box_state == BOX_GLITCH ? COL_RED : COL_TEXT]);
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == RACE_DONE; }
static void exit_(World *w) {
    world_clear_scripted_bugs(w);
    for (int i = 0; i < 3; i++) S.q[i] = 0;
}

const Vignette vignette_race = {
    VIG_RACE, enter, update, draw_back, 0, at_beat_boundary, is_done, exit_
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/vignettes/race.h sim/vignettes/race.c test/test_race.c test/test_main.c
git commit -m "Add race condition vignette

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 15: Detective (Heisenbug) vignette

**Files:**
- Create: `sim/vignettes/detective.h`, `sim/vignettes/detective.c`, `test/test_detective.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `extern const Vignette vignette_detective;` phases `DET_DIM 0, DET_FOOTPRINTS 1, DET_GEAR 2, DET_FOLLOW 3, DET_REVEAL 4, DET_BONK 5, DET_UNDIM 6, DET_DONE 7` with durations 0.5, 3.0, 1.0, 3.0, 1.5, 0.5, 0.5 s; `int detective_phase(void)`, `int detective_dim(void)` (0 to 140), `int detective_footprints(void)` (placed so far, 0 to 5). Beat boundaries: on each footprint placement, at the end of FOLLOW, at the end of BONK, and at DONE.
- The hidden bug is spawned with `hidden = 1` in `BUG_WAIT`; it becomes `revealed` only while the glass is within 7 px. Footprints glow only within 7 px of the glass. The dim overlay is drawn in `draw_front`, and the glass ring, glowing prints, and revealed bug are redrawn on top of it so the inside of the glass reads bright.

- [ ] **Step 1: Write the failing tests**

`test/test_detective.c`:

```c
#include "test.h"
#include "vignettes/detective.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

static Bug *hidden_bug(void) {
    for (int i = 0; i < MAX_BUGS; i++) if (w.bugs[i].state != BUG_DEAD && w.bugs[i].hidden) return &w.bugs[i];
    return 0;
}

void test_detective(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 21u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_detective;
    CHECK_EQ(v->id, VIG_DETECTIVE);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(detective_phase(), DET_DIM);
    CHECK(hidden_bug() != 0);
    CHECK_EQ(hidden_bug()->revealed, 0);
    CHECK_EQ(detective_footprints(), 0);

    run(v, 18);  CHECK_EQ(detective_phase(), DET_DIM);          /* t = 0.3 */
    CHECK(detective_dim() > 0 && detective_dim() < 140);
    run(v, 42);  CHECK_EQ(detective_phase(), DET_FOOTPRINTS);   /* t = 1.0 */
    CHECK_EQ(detective_dim(), 140);
    run(v, 60);  CHECK_EQ(detective_footprints(), 2);          /* t = 2.0: prints at 1.1 and 1.7 */
    CHECK_EQ(w.hero.detective, 0);
    run(v, 120); CHECK_EQ(detective_phase(), DET_GEAR);         /* t = 4.0 */
    CHECK_EQ(detective_footprints(), 5);
    CHECK_EQ(w.hero.detective, 1);
    run(v, 60);  CHECK_EQ(detective_phase(), DET_FOLLOW);       /* t = 5.0 */
    run(v, 210); CHECK_EQ(detective_phase(), DET_REVEAL);       /* t = 8.5 */
    CHECK_EQ(hidden_bug()->revealed, 1);
    CHECK_EQ(w.squashed_total, 0);
    run(v, 45);  CHECK_EQ(detective_phase(), DET_BONK);         /* t = 9.25 */
    run(v, 30);  CHECK_EQ(detective_phase(), DET_UNDIM);        /* t = 9.75 */
    CHECK_EQ(w.squashed_total, 1);
    CHECK(detective_dim() < 140);
    run(v, 30);  CHECK_EQ(detective_phase(), DET_DONE);         /* t = 10.25 */
    CHECK(v->is_done(&w));
    CHECK_EQ(detective_dim(), 0);
    CHECK_EQ(w.hero.detective, 0);
    CHECK(boundaries >= 7 && boundaries <= 8);                 /* 5 prints + follow end + bonk end (+ done) */

    v->exit(&w);
    CHECK(hidden_bug() == 0 || hidden_bug()->state == BUG_SQUASHED || hidden_bug()->state == BUG_DEAD);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);
}
```

Add `void test_detective(void);` and `RUN(test_detective);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `vignettes/detective.h` not found.

- [ ] **Step 3: Implement**

`sim/vignettes/detective.h`:

```c
#ifndef DETECTIVE_H
#define DETECTIVE_H
#include "vignette.h"
enum { DET_DIM = 0, DET_FOOTPRINTS, DET_GEAR, DET_FOLLOW, DET_REVEAL, DET_BONK, DET_UNDIM, DET_DONE };
extern const Vignette vignette_detective;
int detective_phase(void);
int detective_dim(void);
int detective_footprints(void);
#endif
```

`sim/vignettes/detective.c`:

```c
#include "vignettes/detective.h"
#include "sprites.h"
#include "fmath.h"

#define DET_MAX_DIM 140
#define DET_PRINTS 5
#define DET_GLASS_RADIUS 7.0f

static const float DURATION[] = { 0.5f, 3.0f, 1.0f, 3.0f, 1.5f, 0.5f, 0.5f };

static struct {
    int phase, boundary, side;
    float t;
    int dim;
    float print_x[DET_PRINTS], print_y[DET_PRINTS];
    int n_prints, follow_idx;
    float spot_x, spot_y;
    Bug *hidden;
} S;

int detective_phase(void) { return S.phase; }
int detective_dim(void) { return S.dim; }
int detective_footprints(void) { return S.n_prints; }

static void aim_glass(Hero *h) {
    h->glass_x = h->x + (float)h->facing * 8.0f;
    h->glass_y = h->y - 8.0f;
}

static int near_glass(const Hero *h, float x, float y) {
    float dx = h->glass_x - x, dy = h->glass_y - y;
    return dx * dx + dy * dy <= DET_GLASS_RADIUS * DET_GLASS_RADIUS;
}

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f;
    Hero *h = &w->hero;
    switch (phase) {
    case DET_GEAR:   h->detective = 1; aim_glass(h); break;
    case DET_FOLLOW: S.follow_idx = 0; hero_walk_to(h, S.print_x[0], S.print_y[0] + 4.0f); break;
    case DET_REVEAL: S.boundary = 1; hero_walk_to(h, S.spot_x - 10.0f, S.spot_y); break;
    case DET_BONK:   h->facing = 1; hero_start_bonk(h); break;
    case DET_UNDIM:  S.boundary = 1; break;
    case DET_DONE:   S.boundary = 1; h->detective = 0; break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    float x0 = world_panel_x0(w, S.side);
    S.spot_x = x0 + (float)w->panel_w - 16.0f;
    S.spot_y = (float)(w->floor_bottom - 4);
    S.hidden = world_spawn_bug_at(w, S.side, S.spot_x, S.spot_y);
    if (S.hidden) { S.hidden->hidden = 1; S.hidden->revealed = 0; S.hidden->state = BUG_WAIT; }
    float start_x = x0 + 16.0f, end_x = S.spot_x - 12.0f;
    for (int i = 0; i < DET_PRINTS; i++) {
        float f = (float)i / (float)(DET_PRINTS - 1);
        S.print_x[i] = start_x + (end_x - start_x) * f;
        S.print_y[i] = (float)(w->floor_bottom - 10) + 6.0f * f + (float)((i % 2) ? 3 : -3);
    }
    S.n_prints = 0; S.follow_idx = 0; S.dim = 0; S.boundary = 0;
    w->hero.detective = 0;
    hero_walk_to(&w->hero, x0 + 12.0f, (float)(w->floor_bottom - 6));
    S.phase = DET_DIM; S.t = 0.0f;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == DET_DONE) return;
    S.t += dt;
    Hero *h = &w->hero;

    switch (S.phase) {
    case DET_DIM:
        S.dim = (int)(DET_MAX_DIM * fm_clamp(S.t / DURATION[DET_DIM], 0.0f, 1.0f));
        break;
    case DET_FOOTPRINTS:
        if (S.n_prints < DET_PRINTS && S.t >= 0.6f * (float)(S.n_prints + 1)) { S.n_prints++; S.boundary = 1; }
        break;
    case DET_FOLLOW:
        aim_glass(h);
        if (hero_arrived(h) && S.follow_idx < DET_PRINTS - 1) {
            S.follow_idx++;
            hero_walk_to(h, S.print_x[S.follow_idx], S.print_y[S.follow_idx] + 4.0f);
        }
        break;
    case DET_REVEAL:
        if (hero_arrived(h)) { h->facing = 1; h->glass_x = S.spot_x; h->glass_y = S.spot_y - 4.0f; }
        else aim_glass(h);
        break;
    case DET_BONK:
        if (h->bonk_landed) world_squash_near(w, S.spot_x, S.spot_y, 10.0f);
        break;
    case DET_UNDIM:
        S.dim = (int)(DET_MAX_DIM * (1.0f - fm_clamp(S.t / DURATION[DET_UNDIM], 0.0f, 1.0f)));
        break;
    default: break;
    }
    if (S.hidden && S.hidden->state != BUG_DEAD)
        S.hidden->revealed = h->detective && near_glass(h, S.hidden->x, S.hidden->y - 4.0f);
    if (S.phase == DET_FOOTPRINTS) { if (S.n_prints == DET_PRINTS && S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1); }
    else if (S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1);
    if (S.phase == DET_DONE) S.dim = 0;
}

static void draw_prints(World *w, Framebuffer *fb, const Color *pal, int only_glowing) {
    for (int i = 0; i < S.n_prints; i++) {
        int glow = w->hero.detective && near_glass(&w->hero, S.print_x[i], S.print_y[i]);
        if (only_glowing && !glow) continue;
        sprite_prop(fb, pal, PROP_FOOTPRINT, (int)S.print_x[i], (int)S.print_y[i], glow);
    }
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    if (S.phase >= DET_DONE) return;
    sprite_prop(fb, pal, PROP_HIDING_SPOT, (int)S.spot_x + 6, (int)S.spot_y, 0);
    draw_prints(w, fb, pal, 0);
}

static void draw_front(World *w, Framebuffer *fb, const Color *pal) {
    if (S.dim <= 0) return;
    draw_dim(fb, -4096, -4096, 8192, 8192, S.dim);           /* whole view; clipping bounds it */
    if (w->hero.detective) {
        sprite_glass(fb, pal, (int)w->hero.glass_x, (int)w->hero.glass_y);
        draw_prints(w, fb, pal, 1);
        if (S.hidden && S.hidden->revealed) sprite_bug(fb, pal, S.hidden);
    }
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == DET_DONE; }
static void exit_(World *w) {
    w->hero.detective = 0;
    S.dim = 0;
    world_clear_scripted_bugs(w);
    S.hidden = 0;
}

const Vignette vignette_detective = {
    VIG_DETECTIVE, enter, update, draw_back, draw_front, at_beat_boundary, is_done, exit_
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/vignettes/detective.h sim/vignettes/detective.c test/test_detective.c test/test_main.c
git commit -m "Add detective (Heisenbug) vignette

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 16: Regression vignette

**Files:**
- Create: `sim/vignettes/regression.h`, `sim/vignettes/regression.c`, `test/test_regression.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `extern const Vignette vignette_regression;` phases `REG_BONK1 0, REG_BONK2 1, REG_THINK 2, REG_SHIELD 3, REG_BOUNCE 4, REG_CLEAR 5, REG_DONE 6` with durations 1.5, 1.5, 1.0, 1.0, 2.0, 1.0 s; `int regression_phase(void)`, `int regression_shield(void)` (1 once planted), `int regression_bounced(void)` (1 once the respawn dissipated on the shield). Beat boundaries: after each landed bonk, after the shield is planted, at DONE.
- Spawned bugs are `BUG_WAIT` so they hold still and are cleared on exit.

- [ ] **Step 1: Write the failing tests**

`test/test_regression.c`:

```c
#include "test.h"
#include "vignettes/regression.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

static int waiting(void) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) if (w.bugs[i].state == BUG_WAIT) n++;
    return n;
}

void test_regression(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 33u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_regression;
    CHECK_EQ(v->id, VIG_REGRESSION);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(regression_phase(), REG_BONK1);
    CHECK_EQ(waiting(), 1);                                  /* the first target */
    CHECK_EQ(regression_shield(), 0);

    run(v, 90);  CHECK_EQ(regression_phase(), REG_BONK2);    /* t = 1.5 */
    CHECK_EQ(w.squashed_total, 1);
    CHECK_EQ(waiting(), 2);                                  /* one bonk, two appear */
    run(v, 90);  CHECK_EQ(regression_phase(), REG_THINK);    /* t = 3.0 */
    CHECK(w.squashed_total >= 2);
    CHECK(waiting() >= 4);                                   /* bonk again, four appear */
    CHECK_EQ(w.hero.action, HERO_THINK);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_SHIELD);   /* t = 4.0 */
    CHECK(w.hero.action != HERO_THINK);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_BOUNCE);   /* t = 5.0 */
    CHECK_EQ(regression_shield(), 1);
    CHECK_EQ(regression_bounced(), 0);
    run(v, 120); CHECK_EQ(regression_phase(), REG_CLEAR);    /* t = 7.0 */
    CHECK_EQ(regression_bounced(), 1);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_DONE);     /* t = 8.0 */
    CHECK(v->is_done(&w));
    CHECK_EQ(waiting(), 0);                                  /* everything dissipated */
    CHECK(boundaries >= 3 && boundaries <= 4);               /* two bonks + shield (+ done) */

    v->exit(&w);
    CHECK_EQ(regression_shield(), 0);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);
}
```

Add `void test_regression(void);` and `RUN(test_regression);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `vignettes/regression.h` not found.

- [ ] **Step 3: Implement**

`sim/vignettes/regression.h`:

```c
#ifndef REGRESSION_H
#define REGRESSION_H
#include "vignette.h"
enum { REG_BONK1 = 0, REG_BONK2, REG_THINK, REG_SHIELD, REG_BOUNCE, REG_CLEAR, REG_DONE };
extern const Vignette vignette_regression;
int regression_phase(void);
int regression_shield(void);
int regression_bounced(void);
#endif
```

`sim/vignettes/regression.c`:

```c
#include "vignettes/regression.h"
#include "sprites.h"
#include "fmath.h"

#define REG_MAX_SPAWN 8

static const float DURATION[] = { 1.5f, 1.5f, 1.0f, 1.0f, 2.0f, 1.0f };

static struct {
    int phase, boundary, side;
    float t;
    Bug *spawned[REG_MAX_SPAWN];
    int n_spawned;
    int swung;                 /* bonk already started in this phase */
    float shield_x, shield_y;
    int shield, bounced;
    Bug *bouncer;
} S;

int regression_phase(void) { return S.phase; }
int regression_shield(void) { return S.shield; }
int regression_bounced(void) { return S.bounced; }

static float clamp_x(const World *w, float x) {
    float x0 = world_panel_x0(w, S.side);
    return fm_clamp(x, x0 + BUG_W / 2, x0 + (float)w->panel_w - BUG_W / 2);
}

static Bug *spawn_still(World *w, float x, float y) {
    if (S.n_spawned >= REG_MAX_SPAWN) return 0;
    Bug *b = world_spawn_bug_at(w, S.side, clamp_x(w, x), y);
    if (!b) return 0;
    b->state = BUG_WAIT;
    S.spawned[S.n_spawned++] = b;
    return b;
}

static Bug *nearest_waiting(const World *w) {
    Bug *best = 0; float best_d = 0;
    for (int i = 0; i < S.n_spawned; i++) {
        Bug *b = S.spawned[i];
        if (!b || b->state != BUG_WAIT) continue;
        float dx = b->x - w->hero.x, dy = b->y - w->hero.y, d = dx * dx + dy * dy;
        if (!best || d < best_d) { best = b; best_d = d; }
    }
    return best;
}

/* Walk toward the nearest waiting bug and swing when in reach. */
static void hunt(World *w) {
    Hero *h = &w->hero;
    if (hero_busy(h) || S.swung) return;
    Bug *b = nearest_waiting(w);
    if (!b) return;
    float dx = b->x - h->x, dy = b->y - h->y;
    if (fm_abs(dx) <= HERO_REACH - 1.0f && fm_abs(dy) <= 3.0f) {
        h->facing = dx < 0 ? -1 : 1;
        hero_start_bonk(h);
        S.swung = 1;
    } else if (hero_arrived(h)) {
        hero_walk_to(h, b->x - (dx < 0 ? -6.0f : 6.0f), b->y);
    }
}

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f; S.swung = 0;
    Hero *h = &w->hero;
    switch (phase) {
    case REG_THINK:  hero_set_action(h, HERO_THINK); break;
    case REG_SHIELD: hero_set_action(h, HERO_IDLE); break;
    case REG_BOUNCE: {
        float x0 = world_panel_x0(w, S.side);
        float from = h->facing > 0 ? x0 + (float)w->panel_w - 6.0f : x0 + 6.0f;
        S.bouncer = spawn_still(w, from, S.shield_y);
        if (S.bouncer) bug_set_target(S.bouncer, S.shield_x + (float)h->facing * 5.0f, S.shield_y);
        break;
    }
    case REG_CLEAR:
        for (int i = 0; i < S.n_spawned; i++) {
            Bug *b = S.spawned[i];
            if (b && b->state == BUG_WAIT) { b->state = BUG_POOF; b->timer = BUG_POOF_TIME; world_spawn_fx(w, FX_POOF, b->x, b->y); }
        }
        break;
    case REG_DONE: S.boundary = 1; break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    Hero *h = &w->hero;
    S.n_spawned = 0; S.shield = 0; S.bounced = 0; S.bouncer = 0; S.boundary = 0; S.swung = 0;
    hero_set_action(h, HERO_IDLE);
    spawn_still(w, h->x + (float)h->facing * 6.0f, h->y);
    S.phase = REG_BONK1; S.t = 0.0f;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == REG_DONE) return;
    S.t += dt;
    Hero *h = &w->hero;

    switch (S.phase) {
    case REG_BONK1:
        if (S.t >= 0.2f) hunt(w);
        if (h->bonk_landed) {
            float px = h->x + (float)h->facing * 6.0f;
            world_squash_near(w, px, h->y, HERO_REACH);
            spawn_still(w, px - 8.0f, h->y); spawn_still(w, px + 8.0f, h->y);
            S.boundary = 1;
        }
        break;
    case REG_BONK2:
        if (S.t >= 0.2f) hunt(w);
        if (h->bonk_landed) {
            float px = h->x + (float)h->facing * 6.0f;
            world_squash_near(w, px, h->y, HERO_REACH);
            spawn_still(w, px - 20.0f, h->y - 6.0f); spawn_still(w, px - 10.0f, h->y + 4.0f);
            spawn_still(w, px + 10.0f, h->y + 4.0f); spawn_still(w, px + 20.0f, h->y - 6.0f);
            S.boundary = 1;
        }
        break;
    case REG_SHIELD:
        if (!S.shield && S.t >= 0.5f) {
            S.shield_x = h->x + (float)h->facing * 12.0f; S.shield_y = h->y;
            S.shield = 1; S.boundary = 1;
        }
        break;
    case REG_BOUNCE:
        if (S.bouncer && !S.bounced && S.bouncer->state == BUG_WAIT) {
            S.bouncer->state = BUG_POOF; S.bouncer->timer = BUG_POOF_TIME;
            world_spawn_fx(w, FX_POOF, S.bouncer->x, S.bouncer->y);
            S.bounced = 1;
        }
        break;
    default: break;
    }
    if (S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1);
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    (void)w;
    if (S.shield && S.phase < REG_DONE) sprite_prop(fb, pal, PROP_SHIELD, (int)S.shield_x, (int)S.shield_y, 0);
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == REG_DONE; }
static void exit_(World *w) {
    if (w->hero.action == HERO_THINK) hero_set_action(&w->hero, HERO_IDLE);
    world_clear_scripted_bugs(w);
    S.shield = 0; S.n_spawned = 0; S.bouncer = 0;
}

const Vignette vignette_regression = {
    VIG_REGRESSION, enter, update, draw_back, 0, at_beat_boundary, is_done, exit_
};
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add sim/vignettes/regression.h sim/vignettes/regression.c test/test_regression.c test/test_main.c
git commit -m "Add regression vignette

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 17: Vignette table and the exported simulation API

**Files:**
- Create: `sim/vignettes/table.h`, `sim/vignettes/table.c`, `sim/sim.h`, `sim/sim.c`, `test/test_sim.c`
- Modify: `test/test_main.c`

**Interfaces:**
- Produces: `extern const Vignette *const VIGNETTES[VIG_COUNT];` and the exports the shim calls: `int sim_init(uint32_t seed, int panel_w, int panel_h, int gap_w)` (0 ok, -1 bad sizes; spawns 4 bugs per side), `void sim_update(int elapsed_ms)`, `void sim_render(void)`, `uint8_t *sim_framebuffer(void)`, `int sim_framebuffer_len(void)`, `void sim_request(int id)`, `uint32_t sim_poll_event(void)`, `void sim_render_static(void)`, `void sim_render_avatar(void)`, `uint8_t *sim_avatar_buffer(void)`, `int sim_avatar_len(void)` (24×24×4). The framebuffer is `2 * panel_w` wide: left panel at columns `0..panel_w-1`, right panel at `panel_w..2*panel_w-1`.
- Consumes: everything from Tasks 2 to 16. Per-panel render order: background, grid dots, floor band, horizon and ground lines, `scene_draw_back`, bugs, hero, fx, `scene_draw_front`.

- [ ] **Step 1: Write the failing tests**

`test/test_sim.c`:

```c
#include "test.h"
#include "sim.h"
#include "vignettes/table.h"
#include "palette.h"
#include "world.h"

static int same(Color a, const Color *b) { return a.r == b->r && a.g == b->g && a.b == b->b; }

/* count non-background pixels in a column range of the framebuffer */
static int painted_cols(const uint8_t *px, int w, int h, int x0, int x1) {
    int n = 0;
    for (int y = 0; y < h; y++) for (int x = x0; x < x1; x++) {
        const uint8_t *p = px + (y * w + x) * 4;
        Color c = { p[0], p[1], p[2], p[3] };
        if (!same(c, &PALETTE_BUGS[COL_BG]) && !same(c, &PALETTE_BUGS[COL_GRID]) &&
            !same(c, &PALETTE_BUGS[COL_FLOOR]) && !same(c, &PALETTE_BUGS[COL_HORIZON])) n++;
    }
    return n;
}

static int has_color(const uint8_t *px, int w, int h, int x0, int x1, const Color *c) {
    for (int y = 0; y < h; y++) for (int x = x0; x < x1; x++) {
        const uint8_t *p = px + (y * w + x) * 4;
        if (p[0] == c->r && p[1] == c->g && p[2] == c->b) return 1;
    }
    return 0;
}

/* advance in 17ms frames, collecting the first event of a given type and id; returns 1 if seen */
static int run_until_event(int frames, int type, int id) {
    for (int i = 0; i < frames; i++) {
        sim_update(17);
        uint32_t e;
        while ((e = sim_poll_event()) != 0)
            if (event_type(e) == type && (id < 0 || event_a(e) == id)) return 1;
    }
    return 0;
}

void test_sim(void) {
    for (int i = 0; i < VIG_COUNT; i++) CHECK_EQ(VIGNETTES[i]->id, i);

    CHECK_EQ(sim_init(1u, 96, 1000, 200), -1);
    CHECK_EQ(sim_init(1u, 200, 240, 200), -1);
    CHECK_EQ(sim_init(1u, 96, 240, -5), -1);
    CHECK_EQ(sim_init(1234u, 96, 240, 200), 0);
    CHECK_EQ(sim_framebuffer_len(), 2 * 96 * 240 * 4);
    CHECK(sim_framebuffer() != 0);

    /* first frame: both panels show bugs (4 per side spawned at init) */
    sim_update(17);
    sim_render();
    const uint8_t *px = sim_framebuffer();
    CHECK(painted_cols(px, 192, 240, 0, 96) > 40);
    CHECK(painted_cols(px, 192, 240, 96, 192) > 40);
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HERO_SKIN]));      /* hero starts left */
    CHECK(!has_color(px, 192, 240, 96, 192, &PALETTE_BUGS[COL_HERO_SKIN]));

    /* a request plays the race vignette: START then, about ten seconds later, END */
    while (sim_poll_event()) {}
    sim_request(1);
    CHECK(run_until_event(120, EV_VIGNETTE_START, 1));
    CHECK(run_until_event(800, EV_VIGNETTE_END, 1));

    /* non-linked ids never start anything */
    while (sim_poll_event()) {}
    sim_request(0); sim_request(4); sim_request(7);
    CHECK(!run_until_event(120, EV_VIGNETTE_START, -1));

    /* squashes arrive as events while patrolling */
    CHECK(run_until_event(1200, EV_BUG_SQUASHED, -1));

    /* update is clamped: a huge elapsed time does not fast-forward for minutes */
    sim_request(2);
    CHECK(run_until_event(120, EV_VIGNETTE_START, 2));
    sim_update(60000);
    uint32_t e; int ended = 0;
    while ((e = sim_poll_event()) != 0) if (event_type(e) == EV_VIGNETTE_END) ended = 1;
    CHECK(!ended);

    /* static frame: hero mid-bonk on the left, bugs on both sides */
    sim_render_static();
    px = sim_framebuffer();
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HERO_SKIN]));
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HAMMER]));
    CHECK(painted_cols(px, 192, 240, 96, 192) > 40);

    /* avatar: 24x24 with the hero and one bug */
    CHECK_EQ(sim_avatar_len(), 24 * 24 * 4);
    sim_render_avatar();
    const uint8_t *av = sim_avatar_buffer();
    CHECK(has_color(av, 24, 24, 0, 24, &PALETTE_BUGS[COL_HERO_SKIN]));
    CHECK(has_color(av, 24, 24, 0, 24, &PALETTE_BUGS[COL_BUG_A]));

    /* re-init resets the scene without leaking state */
    CHECK_EQ(sim_init(99u, 96, 200, 100), 0);
    CHECK_EQ(sim_framebuffer_len(), 2 * 96 * 200 * 4);
    CHECK_EQ(sim_poll_event(), 0u);
}
```

Add `void test_sim(void);` and `RUN(test_sim);` to `test/test_main.c`.

- [ ] **Step 2: Run to verify failure**

Run: `make test` → compile error, `sim.h` not found.

- [ ] **Step 3: Implement**

`sim/vignettes/table.h`:

```c
#ifndef VIGNETTE_TABLE_H
#define VIGNETTE_TABLE_H
#include "vignette.h"
extern const Vignette *const VIGNETTES[VIG_COUNT];
#endif
```

`sim/vignettes/table.c`:

```c
#include "vignettes/table.h"
#include "vignettes/patrol.h"
#include "vignettes/race.h"
#include "vignettes/detective.h"
#include "vignettes/regression.h"
#include "vignettes/crossover.h"

const Vignette *const VIGNETTES[VIG_COUNT] = {
    [VIG_PATROL]     = &vignette_patrol,
    [VIG_RACE]       = &vignette_race,
    [VIG_DETECTIVE]  = &vignette_detective,
    [VIG_REGRESSION] = &vignette_regression,
    [VIG_CROSSOVER]  = &vignette_crossover,
};
```

`sim/sim.h`:

```c
#ifndef SIM_H
#define SIM_H
#include <stdint.h>
#include "events.h"

#define AVATAR_W 24
#define AVATAR_H 24

int      sim_init(uint32_t seed, int panel_w, int panel_h, int gap_w);
void     sim_update(int elapsed_ms);
void     sim_render(void);
uint8_t *sim_framebuffer(void);
int      sim_framebuffer_len(void);
void     sim_request(int vignette_id);
uint32_t sim_poll_event(void);
void     sim_render_static(void);
void     sim_render_avatar(void);
uint8_t *sim_avatar_buffer(void);
int      sim_avatar_len(void);

#endif
```

`sim/sim.c`:

```c
#include "export.h"
#include "sim.h"
#include "stage.h"
#include "world.h"
#include "scene.h"
#include "sprites.h"
#include "palette.h"
#include "vignettes/table.h"

static uint8_t g_fb_px[2 * PANEL_W_MAX * PANEL_H_MAX * 4];
static uint8_t g_avatar_px[AVATAR_W * AVATAR_H * 4];
static World g_world;
static EventQueue g_events;
static Scene g_scene;
static Stage g_stage;
static int g_ready;

static void bugs_step(Stage *s, float dt) {
    World *w = (World *)s->state;
    world_update_actors(w, dt);
    scene_step(&g_scene, dt);
}

static void draw_panel(Stage *s, int side) {
    World *w = (World *)s->state;
    Framebuffer *fb = &s->fb;
    const Color *pal = s->palette;
    int x0 = (int)world_panel_x0(w, side);
    fb_set_view(fb, side * w->panel_w, 0, w->panel_w, w->panel_h, side * w->panel_w - x0, 0);

    draw_rect(fb, x0, 0, w->panel_w, w->panel_h, pal[COL_BG]);
    for (int y = 4; y < w->panel_h; y += 8)
        for (int x = 4; x < w->panel_w; x += 8) draw_pixel(fb, x0 + x, y, pal[COL_GRID]);
    draw_rect(fb, x0, w->floor_top, w->panel_w, w->floor_bottom - w->floor_top + 1, pal[COL_FLOOR]);
    draw_rect(fb, x0, w->floor_top, w->panel_w, 1, pal[COL_HORIZON]);
    draw_rect(fb, x0, w->floor_bottom + 1, w->panel_w, 1, pal[COL_HORIZON]);

    scene_draw_back(&g_scene, fb, pal);
    for (int i = 0; i < MAX_BUGS; i++)
        if (w->bugs[i].state != BUG_DEAD && w->bugs[i].side == side) sprite_bug(fb, pal, &w->bugs[i]);
    if (world_hero_side(w) == side) sprite_hero(fb, pal, &w->hero);
    for (int i = 0; i < MAX_FX; i++) if (w->fx[i].active) sprite_fx(fb, pal, &w->fx[i]);
    scene_draw_front(&g_scene, fb, pal);
    fb_reset_view(fb);
}

static void bugs_render(Stage *s) {
    draw_panel(s, SIDE_LEFT);
    draw_panel(s, SIDE_RIGHT);
}

SIM_EXPORT("sim_init")
int sim_init(uint32_t seed, int panel_w, int panel_h, int gap_w) {
    if (panel_w < 32 || panel_w > PANEL_W_MAX || panel_h < 64 || panel_h > PANEL_H_MAX || gap_w < 0 || gap_w > 4096)
        return -1;
    g_ready = 0;
    events_init(&g_events);
    world_init(&g_world, seed, panel_w, panel_h, gap_w, &g_events);
    for (int i = 0; i < 4; i++) { world_spawn_bug(&g_world, SIDE_LEFT); world_spawn_bug(&g_world, SIDE_RIGHT); }
    stage_init(&g_stage, g_fb_px, 2 * panel_w, panel_h, PALETTE_BUGS, bugs_step, bugs_render, &g_world);
    scene_init(&g_scene, &g_world, VIGNETTES);
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

SIM_EXPORT("sim_request")
void sim_request(int vignette_id) { if (g_ready) scene_request(&g_scene, vignette_id); }

SIM_EXPORT("sim_poll_event")
uint32_t sim_poll_event(void) { return events_pop(&g_events); }

SIM_EXPORT("sim_render_static")
void sim_render_static(void) {
    if (!g_ready) return;
    World *w = &g_world;
    for (int i = 0; i < MAX_BUGS; i++) w->bugs[i].state = BUG_DEAD;
    for (int i = 0; i < MAX_FX; i++) w->fx[i].active = 0;
    int mid = (w->floor_top + w->floor_bottom) / 2;
    for (int side = 0; side < 2; side++) {
        float x0 = world_panel_x0(w, side);
        world_spawn_bug_at(w, side, x0 + 20.0f, (float)(w->floor_top + 10));
        world_spawn_bug_at(w, side, x0 + 76.0f, (float)mid);
        world_spawn_bug_at(w, side, x0 + 30.0f, (float)(w->floor_bottom - 2));
    }
    hero_init(&w->hero, world_panel_x0(w, SIDE_LEFT) + 44.0f, (float)(w->floor_bottom - 6));
    w->hero.action = HERO_BONK; w->hero.facing = 1;
    Bug *victim = world_spawn_bug_at(w, SIDE_LEFT, w->hero.x + 12.0f, w->hero.y);
    if (victim) { victim->state = BUG_SQUASHED; victim->timer = BUG_SQUASH_TIME; }
    Fx *dust = world_spawn_fx(w, FX_DUST, w->hero.x + 12.0f, w->hero.y);
    if (dust) dust->t = 0.15f;
    stage_render(&g_stage);
}

SIM_EXPORT("sim_render_avatar")
void sim_render_avatar(void) {
    Framebuffer fb;
    fb_init(&fb, g_avatar_px, AVATAR_W, AVATAR_H);
    draw_clear(&fb, PALETTE_BUGS[COL_BG]);
    Bug b; bug_spawn(&b, SIDE_LEFT, 19.0f, 22.0f, 0);
    Hero h; hero_init(&h, 8.0f, 22.0f); h.action = HERO_BONK; h.facing = 1;
    sprite_bug(&fb, PALETTE_BUGS, &b);
    sprite_hero(&fb, PALETTE_BUGS, &h);
}

SIM_EXPORT("sim_avatar_buffer")
uint8_t *sim_avatar_buffer(void) { return g_avatar_px; }

SIM_EXPORT("sim_avatar_len")
int sim_avatar_len(void) { return AVATAR_W * AVATAR_H * 4; }
```

- [ ] **Step 4: Run to verify pass**

Run: `make test` → `0 failures`. Sanitizers must stay clean; a stray pointer into the `bugs` array after `world_clear_scripted_bugs` would show up here.

- [ ] **Step 5: Commit**

```bash
git add sim/vignettes/table.h sim/vignettes/table.c sim/sim.h sim/sim.c test/test_sim.c test/test_main.c
git commit -m "Add vignette table and exported simulation API

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 18: Freestanding build to `sim.wasm` and a Node smoke test

**Files:**
- Create: `sim/freestanding.c`, `test/wasm_smoke.mjs`
- Modify: `Makefile` (wasm flags)
- Produces: `sim.wasm` (committed)

**Interfaces:**
- Produces: a wasm module with **zero imports** and the exports `memory`, `sim_init`, `sim_update`, `sim_render`, `sim_framebuffer`, `sim_framebuffer_len`, `sim_request`, `sim_poll_event`, `sim_render_static`, `sim_render_avatar`, `sim_avatar_buffer`, `sim_avatar_len`. `make smoke` proves it instantiates in Node and renders a frame. Size under 40 KB.

- [ ] **Step 1: Install a wasm-capable clang (ask the user first)**

Apple's bundled clang has no wasm32 backend. Confirm with the user, then run one of:

```bash
brew install zig          # preferred: single binary, `zig cc` is clang with wasm32 support
# or
brew install llvm         # alternative; then use WASM_CC=/opt/homebrew/opt/llvm/bin/clang WASM_TARGET=--target=wasm32
```

Verify: `zig version` prints a version, or `/opt/homebrew/opt/llvm/bin/clang --print-targets | grep wasm32` prints a line.

- [ ] **Step 2: Write the smoke test (it fails until the module exists)**

`test/wasm_smoke.mjs`:

```js
import { readFile } from 'node:fs/promises';

const REQUIRED = ['memory', 'sim_init', 'sim_update', 'sim_render', 'sim_framebuffer', 'sim_framebuffer_len',
  'sim_request', 'sim_poll_event', 'sim_render_static', 'sim_render_avatar', 'sim_avatar_buffer', 'sim_avatar_len'];
const MAX_BYTES = 40 * 1024;

const bytes = await readFile(new URL('../sim.wasm', import.meta.url));
const mod = await WebAssembly.compile(bytes);

const imports = WebAssembly.Module.imports(mod);
if (imports.length) throw new Error('module has imports: ' + JSON.stringify(imports));
const names = WebAssembly.Module.exports(mod).map(e => e.name);
for (const n of REQUIRED) if (!names.includes(n)) throw new Error('missing export ' + n);
if (bytes.length > MAX_BYTES) throw new Error(`sim.wasm is ${bytes.length} bytes, budget is ${MAX_BYTES}`);

const { exports: ex } = await WebAssembly.instantiate(mod, {});
if (ex.sim_init(1234, 96, 240, 200) !== 0) throw new Error('sim_init rejected valid sizes');
if (ex.sim_init(1234, 96, 9999, 200) !== -1) throw new Error('sim_init accepted a bad height');
ex.sim_init(1234, 96, 240, 200);
for (let i = 0; i < 60; i++) ex.sim_update(17);
ex.sim_render();

const len = ex.sim_framebuffer_len();
if (len !== 2 * 96 * 240 * 4) throw new Error('bad framebuffer length ' + len);
const px = new Uint8Array(ex.memory.buffer, ex.sim_framebuffer(), len);
let painted = 0;
for (let i = 0; i < len; i += 4) if (!(px[i] === 18 && px[i + 1] === 22 && px[i + 2] === 40) && px[i + 3] === 255) painted++;
if (painted < 500) throw new Error('framebuffer looks empty: ' + painted + ' painted pixels');

ex.sim_request(1);
let started = false;
for (let i = 0; i < 120 && !started; i++) {
  ex.sim_update(17);
  for (let e = ex.sim_poll_event(); e !== 0; e = ex.sim_poll_event()) if ((e >>> 24) === 1 && ((e >>> 8) & 255) === 1) started = true;
}
if (!started) throw new Error('race vignette never started');

ex.sim_render_avatar();
const av = new Uint8Array(ex.memory.buffer, ex.sim_avatar_buffer(), ex.sim_avatar_len());
if (av.length !== 24 * 24 * 4) throw new Error('bad avatar length');

console.log(`ok: ${bytes.length} bytes, ${names.length} exports, ${painted} painted pixels`);
```

Run: `make smoke`
Expected: fails because `sim.wasm` cannot be built yet (no `freestanding.c`, and `zig cc` may not be installed).

- [ ] **Step 3: Add the freestanding memory helpers and finalise the wasm flags**

`sim/freestanding.c` (compiled into both builds, empty natively):

```c
#ifdef __wasm__
#include <stddef.h>

/* The compiler may lower struct copies and zero-initialisation to these calls.
 * Built with -fno-builtin so these loops are not themselves turned back into calls. */
void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}
#else
typedef int freestanding_unused;   /* keeps the translation unit non-empty natively */
#endif
```

Confirm the `Makefile` variables from Task 1 read exactly:

```make
WASM_CC     ?= zig cc
WASM_TARGET ?= -target wasm32-freestanding

WASM_FLAGS := $(WASM_TARGET) -std=c11 -nostdlib -ffreestanding -fno-builtin -fvisibility=hidden \
              -mbulk-memory -O2 -Wall -Wextra -Isim -Wl,--no-entry
```

`-fno-builtin` stops the compiler turning the loops in `freestanding.c` back into calls to themselves; `-mbulk-memory` lets it use the wasm `memory.copy`/`memory.fill` instructions instead where possible. For Homebrew LLVM instead of Zig, invoke as
`make WASM_CC=/opt/homebrew/opt/llvm/bin/clang WASM_TARGET=--target=wasm32`.

Contingencies, only if the build complains:
- `zig cc` rejects `-Wl,--no-entry`: replace it with `-fno-entry`.
- Exports missing in the smoke test: add `-Wl,--export-dynamic` to `WASM_FLAGS`, or list them with `-Wl,--export=sim_init -Wl,--export=sim_update ...`.
- An `undefined symbol: memset` or similar at link time means `freestanding.c` was not compiled; check the `SIM_SRC` wildcard picked it up.

- [ ] **Step 4: Build and run the smoke test**

Run: `make sim.wasm && make smoke`
Expected: `ls -l sim.wasm` shows a size under 40960 bytes and the smoke test prints `ok: ... bytes, 12 exports, ... painted pixels`.

Run: `make test`
Expected: still `0 failures` (the native build now includes `freestanding.c` as an empty unit).

- [ ] **Step 5: Commit**

```bash
git add Makefile sim/freestanding.c test/wasm_smoke.mjs sim.wasm
git commit -m "Build sim.wasm freestanding with a Node smoke test

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 19: Page markup and styles

**Files:**
- Create: `index.html`, `styles.css`

**Interfaces:**
- Produces the DOM the shim (Task 20) relies on: `canvas#panel-left.panel`, `canvas#panel-right.panel`, `section#hero.hero`, `canvas#avatar.avatar` (24×24), three `article.case-file[data-vignette]` with `tabindex="0"`, `span#squashed`, and `<script type="module" src="shim.js">`. CSS classes `is-playing` (card glow) and `no-sim` (on `body`, fallback).
- Copy is placeholder text in square brackets for the author to replace (spec section 17).

- [ ] **Step 1: Write `index.html`**

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Rohit Kumar</title>
  <meta name="description" content="Rohit Kumar, software engineer. Race conditions fixed, heisenbugs found, regressions prevented.">
  <link rel="stylesheet" href="styles.css">
</head>
<body>
  <main class="stage">
    <canvas id="panel-left" class="panel" aria-hidden="true" role="presentation"></canvas>

    <section id="hero" class="hero">
      <canvas id="avatar" class="avatar" width="24" height="24" aria-hidden="true" role="presentation"></canvas>
      <header>
        <h1>Rohit Kumar</h1>
        <p class="title">[Title line: role, years, the stack you want to be hired for]</p>
        <p class="pitch">[One sentence. Example tone: I fix the bugs that only happen in production, for one customer, on Fridays.]</p>
      </header>

      <ul class="case-files" aria-label="Case files">
        <li>
          <article class="case-file" data-vignette="race" tabindex="0">
            <span class="tag">Race condition</span>
            <p class="symptom">[Symptom: what users saw, one line]</p>
            <p class="fix">[Fix: what you changed, one line]</p>
          </article>
        </li>
        <li>
          <article class="case-file" data-vignette="detective" tabindex="0">
            <span class="tag">Heisenbug</span>
            <p class="symptom">[Symptom: the bug that disappeared under observation]</p>
            <p class="fix">[Fix: how you tracked it down]</p>
          </article>
        </li>
        <li>
          <article class="case-file" data-vignette="regression" tabindex="0">
            <span class="tag">Regression</span>
            <p class="symptom">[Symptom: the bug that kept coming back]</p>
            <p class="fix">[Fix: the test or guard that made it stay fixed]</p>
          </article>
        </li>
      </ul>

      <nav class="contact" aria-label="Contact">
        <a href="mailto:[you@example.com]">Email</a>
        <a href="https://github.com/[handle]" rel="me">GitHub</a>
        <a href="resume.pdf">Resume</a>
      </nav>

      <p class="counter">bugs squashed this visit: <span id="squashed">0</span></p>
      <p class="credit">Side panels: C compiled to WebAssembly, no framework. <a href="https://github.com/[handle]/[repo]">Source</a></p>
    </section>

    <canvas id="panel-right" class="panel" aria-hidden="true" role="presentation"></canvas>
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
  --bg: #12162a;
  --card: #1b2140;
  --line: #2a3160;
  --text: #dce1f0;
  --muted: #98a0c0;
  --accent: #fac83c;
  --glow: #fff096;
}

* { box-sizing: border-box; }

html, body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  line-height: 1.5;
}

.stage {
  display: grid;
  grid-template-columns: 1fr 2fr 1fr;
  min-height: 100vh;
}

.panel {
  display: block;
  width: 100%;
  height: 100%;
  min-height: 100vh;
  background: var(--bg);
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.hero {
  position: relative;
  z-index: 1;
  display: flex;
  flex-direction: column;
  gap: 1.25rem;
  padding: 3rem 2.5rem;
  background: var(--card);
  box-shadow: 0 0 0 1px var(--line), 0 20px 60px rgba(0, 0, 0, 0.45);
}

.avatar { display: none; width: 96px; height: 96px; image-rendering: pixelated; }

h1 { margin: 0; font-size: 2rem; line-height: 1.2; }
.title { margin: 0.25rem 0 0; color: var(--muted); }
.pitch { margin: 0.75rem 0 0; font-size: 1.125rem; }

.case-files { list-style: none; margin: 0; padding: 0; display: grid; gap: 0.75rem; }

.case-file {
  position: relative;
  padding: 0.9rem 1rem;
  border: 1px solid var(--line);
  border-radius: 6px;
  outline: none;
  transition: border-color 0.2s, box-shadow 0.2s;
}
.case-file:hover { border-color: #3a4380; }
.case-file:focus-visible { border-color: var(--accent); }
.case-file.is-playing {
  border-color: var(--glow);
  box-shadow: 0 0 0 2px rgba(255, 240, 150, 0.25), 0 0 18px rgba(255, 240, 150, 0.25);
}
.case-file.is-playing::after {
  content: "\25B6";
  position: absolute;
  top: 0.6rem;
  right: 0.75rem;
  font-size: 0.7rem;
  color: var(--glow);
}

.tag {
  display: inline-block;
  margin-bottom: 0.35rem;
  font-size: 0.72rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--accent);
}
.case-file p { margin: 0.15rem 0; }
.fix { color: var(--muted); }

.contact { display: flex; flex-wrap: wrap; gap: 1.25rem; }
.contact a { color: var(--text); }

.counter { margin: 0; font-size: 0.85rem; color: var(--muted); }
.credit { margin: auto 0 0; font-size: 0.75rem; color: var(--muted); }
.credit a { color: var(--muted); }

.below { max-width: 60rem; margin: 0 auto; padding: 3rem 2.5rem; }

@media (max-width: 1023.98px) {
  .stage { grid-template-columns: 1fr; min-height: auto; }
  .panel { display: none; }
  .avatar { display: block; }
  .hero { min-height: 100vh; padding: 2rem 1.25rem; }
}

@media (prefers-reduced-motion: reduce) {
  .case-file { transition: none; }
}
```

- [ ] **Step 3: Verify the layout in Chrome**

Run: `make serve` in the background (port 8000). With Chrome DevTools MCP: `new_page` at `http://localhost:8000/`, `resize_page` to 1440×900, `take_screenshot`. Expected: three columns, the hero card in the middle with the case files, contact row, counter, and credit line, both side columns plain navy (the shim does not exist yet, so a 404 for `shim.js` in the console is expected at this task).

`resize_page` to 390×844, `take_screenshot`. Expected: a single column with the hero card only, no side canvases, and a blank 96×96 avatar space above the name.

- [ ] **Step 4: Commit**

```bash
git add index.html styles.css
git commit -m "Add hero card markup and 1:2:1 layout styles

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 20: The browser shim

**Files:**
- Create: `shim.js`

**Interfaces:**
- Consumes: the wasm exports from Task 17/18 and the DOM from Task 19.
- Produces: the running page. Debug query parameters: `?vignette=race|detective|regression` requests a vignette right after init; `?motion=reduce` forces static mode so reduced motion can be checked without changing OS settings.
- Behaviour, per spec sections 8 to 13: instantiate with streaming and fall back to array-buffer instantiation; compute integer scale and logical height from the left panel's backing size; call `sim_init` once and again only when logical height or gap width change; one `requestAnimationFrame` loop calling `sim_update` then `sim_render` then blitting each framebuffer half into its canvas with smoothing off and letterboxing; drain events into card glow and the counter; hover intent 150 ms, `pointerleave` cancels only the timer, `focusin` requests immediately; stop the loop when the tab is hidden; static mode under reduced motion; avatar-only under 1024 px; any failure adds `no-sim` to `body` and leaves the hero card untouched.

- [ ] **Step 1: Write `shim.js`**

```js
// shim.js — the only code on the page that touches the DOM.
// The simulation lives in sim.wasm; everything crossing the boundary is an integer or a pointer.

const PANEL_W = 96, PANEL_H_MIN = 64, PANEL_H_MAX = 320, HOVER_MS = 150;
const VIG = { race: 1, detective: 2, regression: 3 };
const EV = { START: 1, END: 2, SQUASHED: 3 };
const BG = '#12162a';

const params = new URLSearchParams(location.search);
const desktop = matchMedia('(min-width: 1024px)');
const reduced = matchMedia('(prefers-reduced-motion: reduce)');
const staticMode = () => reduced.matches || params.get('motion') === 'reduce';
const $ = (sel) => document.querySelector(sel);
const cards = [...document.querySelectorAll('.case-file[data-vignette]')];

let sim = null;     // wasm exports
let panels = null;  // { left, right, off, backingW, backingH, scale, panelH, gapW }
let raf = 0, last = 0, squashed = 0;

async function loadWasm() {
  const url = new URL('sim.wasm', import.meta.url);
  try {
    return (await WebAssembly.instantiateStreaming(fetch(url), {})).instance.exports;
  } catch {
    const buf = await (await fetch(url)).arrayBuffer();           // wrong MIME type, older host
    return (await WebAssembly.instantiate(buf, {})).instance.exports;
  }
}

function fail(err) {
  console.warn('bug smasher disabled:', err);
  stopLoop();
  document.body.classList.add('no-sim');
}

function seed() { return (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0; }

// ---- geometry -------------------------------------------------------------
function measure() {
  const left = $('#panel-left'), hero = $('#hero');
  const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
  const backingW = Math.max(PANEL_W, Math.round(left.clientWidth * dpr));
  const backingH = Math.max(PANEL_H_MIN, Math.round(left.clientHeight * dpr));
  const scale = Math.max(1, Math.floor(backingW / PANEL_W));
  const panelH = Math.max(PANEL_H_MIN, Math.min(PANEL_H_MAX, Math.floor(backingH / scale)));
  const gapW = Math.min(4096, Math.round(hero.clientWidth * dpr / scale));
  return { backingW, backingH, scale, panelH, gapW };
}

function init(g) {
  if (sim.sim_init(seed(), PANEL_W, g.panelH, g.gapW) !== 0) throw new Error('sim_init rejected ' + JSON.stringify(g));
  const debug = VIG[params.get('vignette')];
  if (debug) sim.sim_request(debug);
}

function createPanels() {
  const g = measure();
  const left = $('#panel-left'), right = $('#panel-right');
  for (const c of [left, right]) { c.width = g.backingW; c.height = g.backingH; }
  const off = document.createElement('canvas');
  off.width = PANEL_W * 2; off.height = g.panelH;
  panels = { left, right, off, ...g };
  init(g);
}

function resizePanels() {
  const g = measure();
  const reinit = g.panelH !== panels.panelH || g.gapW !== panels.gapW;
  for (const c of [panels.left, panels.right]) { c.width = g.backingW; c.height = g.backingH; }
  Object.assign(panels, g);
  if (reinit) { panels.off.height = g.panelH; init(g); }   // the scene restarts; the counter is DOM-side and survives
  if (staticMode()) renderStatic();
}

// ---- drawing --------------------------------------------------------------
function blit() {
  const { left, right, off, scale, panelH, backingW, backingH } = panels;
  const px = new Uint8ClampedArray(sim.memory.buffer, sim.sim_framebuffer(), sim.sim_framebuffer_len());
  off.getContext('2d').putImageData(new ImageData(px, PANEL_W * 2, panelH), 0, 0);
  const ox = Math.floor((backingW - PANEL_W * scale) / 2);
  const oy = Math.floor((backingH - panelH * scale) / 2);
  [left, right].forEach((canvas, side) => {
    const ctx = canvas.getContext('2d');
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = BG;
    ctx.fillRect(0, 0, backingW, backingH);
    ctx.drawImage(off, side * PANEL_W, 0, PANEL_W, panelH, ox, oy, PANEL_W * scale, panelH * scale);
  });
}

function drainEvents() {
  for (let e = sim.sim_poll_event(); e !== 0; e = sim.sim_poll_event()) {
    const type = e >>> 24, id = (e >>> 8) & 255;
    if (type === EV.START) cards.forEach(c => c.classList.toggle('is-playing', VIG[c.dataset.vignette] === id));
    else if (type === EV.END) cards.forEach(c => { if (VIG[c.dataset.vignette] === id) c.classList.remove('is-playing'); });
    else if (type === EV.SQUASHED) $('#squashed').textContent = String(++squashed);
  }
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

function renderStatic() { sim.sim_render_static(); blit(); }

function renderAvatar() {
  sim.sim_render_avatar();
  const px = new Uint8ClampedArray(sim.memory.buffer, sim.sim_avatar_buffer(), sim.sim_avatar_len());
  $('#avatar').getContext('2d').putImageData(new ImageData(px, 24, 24), 0, 0);
}

// ---- hover linking ---------------------------------------------------------
function wireCards() {
  for (const card of cards) {
    const id = VIG[card.dataset.vignette];
    if (!id) continue;
    let timer = 0;
    card.addEventListener('pointerenter', () => { clearTimeout(timer); timer = setTimeout(() => sim.sim_request(id), HOVER_MS); });
    card.addEventListener('pointerleave', () => clearTimeout(timer));   // cancels the intent only, never the vignette
    card.addEventListener('focusin', () => sim.sim_request(id));
  }
}

// ---- mode switching --------------------------------------------------------
function applyMode() {
  stopLoop();
  if (!desktop.matches) { panels = null; renderAvatar(); return; }
  if (panels) resizePanels(); else createPanels();
  if (staticMode()) renderStatic(); else startLoop();
}

async function main() {
  sim = await loadWasm();
  wireCards();
  applyMode();
  desktop.addEventListener('change', applyMode);
  reduced.addEventListener('change', applyMode);
  new ResizeObserver(() => { if (panels && desktop.matches) resizePanels(); }).observe($('#panel-left'));
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) stopLoop();
    else if (panels && !staticMode()) startLoop();
  });
}

main().catch(fail);
```

- [ ] **Step 2: Check the size budget**

Run: `gzip -c shim.js | wc -c`
Expected: under 3072. If over, trim comments first; do not minify.

- [ ] **Step 3: Verify in Chrome**

With `make serve` running and Chrome DevTools MCP:

1. `navigate_page` to `http://localhost:8000/`, `resize_page` 1440×900, wait two seconds, `take_screenshot`. Expected: both side panels show a navy world with a floor band, cute pixel bugs on both sides, and the character on the left bonking bugs. `list_console_messages` shows no errors.
2. `take_snapshot`, then `hover` the "Race condition" case file and wait one second. `evaluate_script` returning `document.querySelector('[data-vignette=race]').classList.contains('is-playing')` is `true`. `take_screenshot`: a `COUNT` box in the panel where the character is, two bugs rushing it, then red and jittering.
3. Wait twelve seconds. `evaluate_script` for the same class is `false` (the vignette ended and patrol resumed).
4. `evaluate_script` returning `Number(document.querySelector('#squashed').textContent)` is greater than 0.
5. `navigate_page` to `http://localhost:8000/?vignette=detective`, wait three seconds, `take_screenshot`: the character's panel is dimmed with footprints appearing. Repeat with `?vignette=regression` and screenshot the shield after about five seconds.
6. `navigate_page` to `http://localhost:8000/?motion=reduce`, wait one second, take two screenshots two seconds apart. Expected: identical frames showing the character mid-bonk with a few bugs on each side; `evaluate_script` returning `performance.now()` differences are irrelevant, the frames must not change.
7. `resize_page` 390×844, `navigate_page` to `http://localhost:8000/`, `take_screenshot`. Expected: single column, a 96×96 pixel avatar of the character mid-bonk above the name, no side panels.
8. `list_network_requests`: `sim.wasm` is served once, under 40 KB, content type `application/wasm`.
9. Press Tab until a case file has focus (`press_key` Tab repeatedly) at 1440×900: the focused card's vignette starts within a second.

If any step fails, fix the cause (shim, CSS, or C) and re-run the affected steps before committing.

- [ ] **Step 4: Commit**

```bash
git add shim.js
git commit -m "Add browser shim: wasm loop, blit, hover linking, fallbacks

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 21: Final verification against the spec

**Files:**
- Modify only if a check fails.

- [ ] **Step 1: Run every automated check**

```bash
make clean && make test && make sim.wasm && make smoke
ls -l sim.wasm
gzip -c shim.js | wc -c
```

Expected: tests report `0 failures` with no sanitizer output; the smoke test prints `ok`; `sim.wasm` is under 40960 bytes; gzipped `shim.js` is under 3072 bytes.

- [ ] **Step 2: Walk the spec's browser checklist (section 15)**

With `make serve` running, confirm each item and note the result in the commit message of any fix:

- Desktop grid at 1440×900 and mobile fallback with avatar at 390×844 (screenshots).
- Each vignette via `?vignette=race`, `?vignette=detective`, `?vignette=regression`.
- Hover linking adds `is-playing` to the hovered card and removes it when the vignette ends; keyboard focus does the same.
- Crossover: load `http://localhost:8000/` and wait; within roughly two minutes the character walks off the inner edge of its panel and reappears on the other side. To force it sooner, `evaluate_script` cannot reach wasm state, so instead confirm it in the native tests (`test_scene`, `test_crossover`) and accept the visual check as best-effort.
- `?motion=reduce` shows a static frame and the page performs no animation.
- The network panel shows `sim.wasm` under 40 KB with content type `application/wasm`, and no request other than `index.html`, `styles.css`, `shim.js`, `sim.wasm`.
- Canvases carry `aria-hidden="true"` and `role="presentation"` (`take_snapshot` shows no canvas nodes in the accessibility tree).
- Temporarily rename `sim.wasm` and reload: the hero card renders fully, the side columns stay plain navy, the console shows the `bug smasher disabled` warning, and `body` has class `no-sim`. Rename it back.

- [ ] **Step 3: Commit any fixes and tag the phase**

```bash
git status --short          # expect clean, or only fixes from this task
git tag phase-1
```

---

## Self-review notes (for the plan author)

- **Spec coverage.** Layout and hero card: Task 19. Mobile avatar and static mode: Tasks 17 and 20. World rules, palette, sprites: Tasks 6 to 9. Vignettes 6.1 to 6.5: Tasks 11, 14, 15, 16, 13. Content linking, idle auto-play, crossover triggers, debug hook: Tasks 12 and 20. Architecture and exports: Tasks 10 and 17. Frame loop and clamping: Tasks 10 and 20. Build and toolchain: Task 18. Accessibility and motion: Tasks 19 and 20. Resilience: Tasks 17 (guards), 20 (`fail`). Budgets: Tasks 18, 20, 21. Testing: every task, plus 18 and 21.
- **Deviations from the spec, on purpose.** The event queue lives in `events.c` rather than inside `sim.c`, and `font.c`, `palette.c`, `fmath.h`, and `vignettes/table.c` are extra files, all for testability. The floor is a band from 55% to 90% of the panel height with the ground line at the bottom edge, so bugs can wander in two dimensions on a tall narrow panel. The crossover hides the character for `(gap - HERO_W) / speed` rather than `gap / speed`, because the sprite is already half hidden when it reaches the panel edge.
- **Phase 2 hooks.** `Stage`, `EventQueue` with a stage id, `SIM_EXPORT`, and the `draw.h`/`font.h` primitives are stage-agnostic. The settlement stage adds `sim/oasis/*`, a second static framebuffer, and `oasis_*` exports without touching any file from this plan except `Makefile` globs (none needed) and `shim.js`.
