# Bug Smasher Profile — Design Spec

- **Date:** 2026-09-03
- **Status:** Approved design, ready for implementation planning
- **Owner:** Rohit Kumar
- **Revision:** 3.1 — wording amendments after the phase 1 implementation (budgets, latency, resize, debug hooks)

## 1. Purpose and audience

A single-page personal profile aimed primarily at recruiters and hiring managers. The page must deliver name, pitch, and contact within seconds, while pixel-art side animations show, rather than claim, the author's debugging strengths: fixing race conditions, hunting hard-to-reproduce bugs, and preventing regressions.

Below the fold, a second animated stage shows a different strength: leading greenfield projects from nothing to a thriving state. The character arrives on barren land, digs a well, lays foundations, ships a first hut, brings in a team, and directs the growth of a settlement around an oasis. Each landmark maps to a real project.

A secondary goal is a credibility signal for technical hiring managers: the animation engine is written in C and compiled to WebAssembly with no framework, and the page says so with a link to the source.

### Delivery phases

- **Phase 1:** the first screen (sections 2 to 6 and 8) and the engine with its stage abstraction in place.
- **Phase 2:** the greenfield settlement stage (section 7) as a second stage added to the same module.

Phase 1 ships on its own. The stage abstraction is built in phase 1 so that phase 2 adds a stage rather than restructuring the engine.

Success looks like: a recruiter lands on the page, reads the pitch and case files without distraction, notices the side animation acting out the case file they are looking at, and finds the contact links immediately. On a phone they get the same information with no animation.

## 2. Layout

### Desktop (viewport width 1024px and above)

Three-column CSS grid, `grid-template-columns: 1fr 2fr 1fr`, filling the viewport height. The first screen does not need to scroll.

- **Left and right columns:** one `<canvas>` each, showing the two halves of a single simulated world (see section 5).
- **Middle column:** the hero card, a solid surface on top of the world. The character can walk *behind* it: it disappears at one panel's inner edge and reappears at the other's after a delay proportional to the card's width.

### Mobile (below 1024px)

Single column containing only the hero card. No animation loop runs. A single static pixel-art frame of the character mid-bonk is shown above the name as an avatar, rendered once by the same C code into a small canvas (section 10, static mode).

Switching between the two layouts (for example a window resize across 1024px) creates or tears down the panels at runtime via `matchMedia`.

## 3. Hero card content

Top to bottom:

1. **Identity:** name, a one-line title, and a one-sentence pitch. Copy is author-supplied. Example pitch, for tone only: "I fix the bugs that only happen in production, for one customer, on Fridays."
2. **Three case files.** Each is a focusable `<article class="case-file" data-vignette="...">` containing a tag, a one-line symptom, a one-line fix, and an optional link to a longer write-up. The three tags and their linked vignettes:
   - `Race condition` → `race`
   - `Heisenbug` → `detective`
   - `Regression` → `regression`
3. **Contact row:** email, GitHub, resume PDF. No LinkedIn.
4. **Live counter** in a corner of the card: "bugs squashed this visit: N". Increments from simulation events. In-memory only, resets each visit.
5. **Engine credit**, one small line at the bottom of the card: side panels are C compiled to WebAssembly, no framework, with a link to the source repository.

Below the fold comes the greenfield settlement section (section 7), then a plain HTML placeholder section for anything else such as experience or a skills list, which is out of scope for version one.

## 4. Visual style

- Pixel art, retro-arcade tone. Version one uses **code-drawn placeholders**: filled rectangles on a logical pixel grid. Real sprite artwork can replace them later through `sprites.c` alone (section 9).
- Dark navy background, a very faint grid, a ground line at roughly 85% of panel height.
- Palette of five or six muted colours so the panels sit visually behind the card.
- **Bugs:** round, cute, two antennae, stub legs, three colour variants. Never realistic. Roughly 10×8 logical px.
- **Character:** small pixel person with a bright toy hammer. Roughly 14×20 logical px. Gains a deerstalker hat and magnifying glass in the detective vignette.
- **Squash effect:** pixel dust puff (about 0.4s) and a small floating "+1".
- **Text in the world** (the `count` label, "+1") uses a 3×5 bitmap font drawn by the C renderer.

## 5. World rules

- **Single world, two views.** World x-coordinates run continuously across left panel, hero gap, and right panel. The renderer produces both panel views into one framebuffer.
- **Logical resolution.** Each panel is 96 logical px wide. Logical height is derived from the panel's aspect ratio, capped at 320. The hero gap's logical width is derived from the card's CSS width at the same scale.
- **Integer scaling** happens in the browser shim: `scale = floor(canvasBackingWidth / 96)`. The logical framebuffer is blitted up with `imageSmoothingEnabled = false`. Any remainder is letterboxed with the background colour.
- **Ambient rule.** The character is never on both sides at once: it is on one side, or behind the hero card during a crossover. The unattended side still has slowly crawling bugs. Motion everywhere, action in one place.
- **Bug cap:** 20 per side.
- **Character walk speed:** about 24 logical px per second.

## 6. Vignettes

Every vignette implements the same interface (section 9). Durations are targets, not hard limits. A **beat boundary** is a moment where interrupting looks intentional rather than broken.

### 6.1 Patrol (default state)

- Bugs spawn from panel edges every 4–8 seconds per side up to the cap and wander with seeded randomness.
- The character walks to the nearest bug, winds up, bonks it. Cadence: one squash every 3–5 seconds.
- Never finishes on its own; it is the state every other vignette returns to.
- Beat boundaries: whenever the character is not mid-swing.

### 6.2 Race condition (`race`, ~10s)

Phases, in order:

1. **Setup (1s):** a shared box labelled `count` appears mid-panel.
2. **Rush (1.5s):** two bugs sprint toward the box from opposite sides.
3. **Collide and glitch (2s):** the bugs collide at the box; the box flickers between values, turns red, jitters.
4. **Lock (2s):** the character walks up and drops a padlock on the box.
5. **Queue (3s):** three bugs approach one at a time, each waits for the previous to finish, each update is clean.
6. **Resolved (0.5s):** the box turns green; the character wipes their brow (optional pose; the placeholder art omits it).

Beat boundaries: between phases. Done after phase 6; props are removed on exit.

### 6.3 Detective / Heisenbug (`detective`, ~12s)

1. **Dim (0.5s):** the panel darkens.
2. **Footprints (3s):** an invisible bug leaves five footprints, one every 0.6s, across the ground.
3. **Gear up (1s):** the character puts on a deerstalker and raises a magnifying glass.
4. **Follow (3s):** the character walks the trail; footprints glow only inside the glass circle.
5. **Reveal (1.5s):** the trail ends at a hiding spot. The bug is visible only while the glass is over it.
6. **Bonk (0.5s)** and **undim (0.5s)**, with slack to reach roughly 12s.

Beat boundaries: after each footprint in phase 2, after phase 4, after phase 6. Done after undim.

### 6.4 Regression (`regression`, ~8s)

1. **Bonk, two appear (1.5s).**
2. **Bonk, four appear (1.5s).**
3. **Pause and think (1s):** the character stops.
4. **Plant shield (1s):** a small shield with a checkmark is placed on the ground.
5. **Bounce (2s):** the next respawn hits the shield and dissipates.
6. **Clear (1s):** remaining bugs are squashed or dissipate.

Beat boundaries: after each bonk, after the shield is planted. Done after clear.

### 6.5 Crossover (transitional)

The character walks to the inner edge of the current panel, disappears behind the hero card, and emerges at the inner edge of the other panel. Traversal time is `gapLogicalWidth / walkSpeed`.

- **Triggers:** the far side has at least 6 more bugs than the current side, or 120 seconds have passed since the last crossover. Never triggered while a linked vignette is playing.
- **Uninterruptible** while the character is behind the card. Any pending request waits and plays on the new side.

## 7. Greenfield settlement stage (phase 2)

Shows that the author starts things and leads them, not only fixes them. It is a second **stage** in the same engine with its own framebuffer, palette, sequencer, content, and hover linking. It reuses the engine's drawing primitives, event queue, and character.

### 7.1 Placement and layout

- A below-the-fold section titled in the spirit of "Built from scratch", directly under the first screen.
- One landscape `<canvas>` spanning the section width. Fixed logical size **320×80**, integer-scaled by width (`scale = floor(canvasBackingWidth / 320)`), letterboxed vertically. Decorative and hidden from assistive tech like the side panels.
- Beneath the canvas, two to five **project cards**, `<article class="project" data-building="k" data-kind="watchtower">`, each with a project name, one line on what it was, one line on the outcome, and the author's role. Each card corresponds to one building. Copy is author-supplied.
- A small pixel **shovel button** in the section corner replays the build.
- **Palette:** sand, cracked-earth brown, teal water, palm green, terracotta, and warm lantern amber, with a deep indigo sky at dusk. Deliberately distinct from the navy side-panel palette so the section reads as a new chapter.
- **Mobile:** the canvas shows the finished settlement as one static frame via `oasis_render_static`, and no loop runs. Cards remain ordinary text. This keeps the "no animation on mobile" rule.

### 7.2 Build sequence

About 15 seconds from trigger to utopia, then an ambient loop for as long as the section is in view.

| # | Phase | Duration | What happens |
|---|---|---|---|
| 0 | Barren | until triggered | Cracked earth, two dead trees, a rock, a tumbleweed, a crooked plank sign reading `?`, harsh noon sun. |
| 1 | Arrival | 2.0s | The character walks in from the left with a rolled blueprint, unrolls it, looks at the land, plants a flag. |
| 2 | Well | 2.0s | The character digs at the centre. Water bubbles up, the pool spreads, the first palm sprouts. |
| 3 | Scaffold | 1.5s | A grid of paving stones radiates fast from the oasis. |
| 4 | First hut | 1.5s | A tent, then a hut. A `v0.1` tag floats up. |
| 5 | Team arrives | 1.5s | Three crew members walk in from the right. The character puts the hammer down, hands out blueprints, and points. From here the character directs; the crew builds. |
| 6 | Time-lapse | 4.5s | The sun arcs across the sky. Project buildings complete one by one in slot order, each followed by a canal from the oasis. `v0.5` floats up when half are done, `v1.0` at the last. Palms grow taller. |
| 7 | Dusk | 1.0s | Sky turns indigo, lanterns glow, fireflies drift over the water, the plank sign becomes a carved sign. The character sits on a bench at the edge. |
| 8 | Wink | 1.0s | One bug wanders in from the edge. The character gets up, bonks it, sits back down. |
| 9 | Ambient | loops | Water shimmer, crew walking between buildings, lantern flicker, an occasional bird. The character idles near the bench. |

### 7.3 Triggers and controls

- **Start:** an `IntersectionObserver` fires when at least 30% of the section is visible and the shim calls `oasis_start`. Before that the canvas shows the Barren frame.
- **Pause:** when the section leaves the viewport the shim stops calling `oasis_update`. On return it resumes where it left off; no reset.
- **Replay:** the shovel button calls `oasis_replay`, which resets to Barren and starts again.
- **Fast-forward:** focusing a building that does not exist yet during phases 1 to 6 runs the sequencer at 8× speed until that building completes, then resumes normal speed. Visitors who hover a project card mid-build never wait.

### 7.4 Project linking

Same 150ms hover-intent rule as the case files.

- **Card → canvas:** hover or keyboard focus on a project card calls `oasis_focus(k)`. The character walks to building k and the building glows for as long as the card is hovered or focused. Pointer leave or blur calls `oasis_focus(-1)`, which clears the glow; the character stays where it is and wanders back to the bench after 5 seconds idle.
- **Canvas → card:** pointer movement over the canvas is converted to logical pixels and passed to `oasis_building_at(x, y)`. A hit highlights the matching card and calls `oasis_focus(k)`; the cursor becomes a pointer. Leaving the building clears both.
- **Event:** when the character reaches the focused building the stage emits `HERO_ARRIVED(k)`, which the shim can use for a small card animation.

### 7.5 Buildings, scenery, and crew

- **Fixed scenery:** oasis pool at the centre, three palms, the well, the flag and sign, the `v0.1` hut, the paving grid, the bench.
- **Project buildings** occupy fixed slots alternating left and right of the oasis, moving outward, so any number from one to six lays out sensibly. Kinds in the catalogue: `watchtower`, `market`, `waterwheel`, `granary`, `aqueduct`, `library`, `forge`. The kind comes from `data-kind`; an unknown kind falls back to `house`.
- **Growth animation:** each building rises from foundation to roof over about 0.7s, with crew hammering at its slot.
- **Canals:** a thin channel extends from the oasis to each building as it completes. Quiet infrastructure metaphor; no explanation on the page.
- **Crew:** three small pixel people distinct from the character. After phase 5 they animate hammering at whichever slot is under construction, and in Ambient they walk between buildings.
- **Milestone tags** are drawn with the same 3×5 bitmap font as the side panels.

## 8. Content linking

- **Hover intent:** `pointerenter` on a case file starts a 150ms timer; on expiry the shim calls the simulation's `sim_request(vignetteId)`. `pointerleave` cancels the timer only. Sweeping the cursor across a card does nothing.
- **Keyboard:** `focusin` on a case file requests immediately.
- **Request semantics:** the simulation holds at most one pending request. A newer request replaces an older pending one. A request for the currently playing vignette is ignored. The swap happens at the current vignette's next beat boundary. From patrol that is under half a second; inside another vignette it is the time to that vignette's next boundary, up to about three seconds in the detective's FOLLOW phase.
- **Location:** the vignette plays wherever the character is. A crossover in progress finishes first.
- **Feedback in the card:** while a vignette plays, its case file gets a subtle glow and a small pixel play icon. Removed on the `VIGNETTE_END` event.
- **Leaving the card does not stop the vignette.** It always plays to completion, then patrol resumes.
- **Idle auto-play:** if no hover request has arrived for 45 seconds, the simulation picks a linked vignette at random (never the same one twice in a row), plays it, and emits `VIGNETTE_START` with the auto flag set so the shim highlights the matching card. This is the path most passive viewers will experience.
- **Debug hooks:** a `?vignette=<name>` query parameter makes the shim call `sim_request` immediately after init, and `?motion=reduce` forces static mode, both for manual verification.

## 9. Architecture

Two layers with one narrow boundary:

- **Page layer:** HTML, CSS, and a single small JavaScript shim. The shim is the only code that touches the DOM, the canvases, or browser events.
- **Simulation layer:** C, compiled to a freestanding WebAssembly module with **no imports** and no libc. It owns all state, all behaviour, all timing, and all pixel rendering into RGBA framebuffers in its own linear memory.

The simulation hosts two **stages**, each with its own framebuffer, fixed-step accumulator, palette, and state machine, sharing the drawing primitives, the seeded RNG, the event queue, and the character's sprite and movement code:

- **Bugs stage** (`sim_*` exports): the two side panels, sections 5 to 6.
- **Oasis stage** (`oasis_*` exports): the settlement section, section 7. Phase 2.

The stage abstraction (a `Stage` struct holding framebuffer, accumulator, palette, and update/render function pointers) is built in phase 1 with a single stage so that phase 2 adds one rather than restructuring.

```
index.html
styles.css
shim.js                  wasm load, frame loop, canvas blit, DOM wiring (~60 lines)
sim/
  sim.h                  exported API and shared constants
  sim.c                  entry points, fixed-step accumulator, event queue
  scene.c / scene.h      vignette state machine, pending request, idle timer, crossover triggers
  rng.c / rng.h          seeded generator (mulberry32)
  bug.c / bug.h          bug entity: wander, queue, hide, squash
  hero.c / hero.h        character entity: walk to target, wind up, bonk, crossover
  vignette.h             uniform vignette interface
  vignettes/
    patrol.c  race.c  detective.c  regression.c  crossover.c
  draw.c / draw.h        framebuffer primitives: fill_rect, put_pixel, dim, 3x5 bitmap text
  sprites.c / sprites.h  draw_bug, draw_hero, draw_prop, draw_fx (placeholder rects in v1)
  freestanding.c         memset, memcpy, memmove for the wasm build
  stage.h                Stage struct: framebuffer, accumulator, palette, update/render hooks
  oasis/                 phase 2
    oasis.c / oasis.h    build sequencer, phases, fast-forward, replay, ambient loop
    buildings.c / .h     slot layout, kinds, growth animation, glow, canals, hit test
    crew.c / crew.h      crew members: walk in, hammer at slot, wander
  sprites_oasis.c        settlement sprites: buildings, palms, water, lanterns, crew (placeholder rects in v1)
test/
  test.h                 minimal assert/report harness
  test_main.c            native test binary entry
  test_*.c               one file per module under test
Makefile                 targets: wasm, test, serve, clean
docs/superpowers/specs/  this document
```

### Exported API (C → shim)

All exports are plain integers or pointers into wasm memory. No strings cross the boundary.

| Export | Purpose |
|---|---|
| `sim_init(seed, panel_w, panel_h, gap_w)` | Reset all state for the given logical sizes. |
| `sim_update(elapsed_ms)` | Advance the fixed-step accumulator; runs as many 1/60s steps as needed, clamped. |
| `sim_render()` | Draw both panels into the framebuffer. |
| `sim_framebuffer()` / `sim_framebuffer_len()` | Pointer and byte length of the RGBA buffer, laid out as left panel and right panel side by side. |
| `sim_request(vignette_id)` | Request a linked vignette (1 race, 2 detective, 3 regression). |
| `sim_poll_event()` | Pop the next event as a packed integer, or 0 if none. Types: `VIGNETTE_START(name, auto)`, `VIGNETTE_END(name)`, `BUG_SQUASHED`. |
| `sim_render_static()` | Arrange a fixed composition in both panels (character mid-bonk, a few bugs) and draw it into the framebuffer once, for the reduced-motion fallback on desktop. |
| `sim_render_avatar()` | Draw a fixed 24×24 frame of the character mid-bonk with one bug into a separate small buffer, for the mobile avatar. |
| `memory` | The module's linear memory. |

Phase 2 adds the oasis stage exports, following the same integer-only rule:

| Export | Purpose |
|---|---|
| `oasis_init(seed, n_buildings)` | Reset the settlement stage with N project buildings (clamped to 1 to 6) in the Barren phase. |
| `oasis_set_kind(k, kind_id)` | Set the building kind for slot k, called once per card after init. |
| `oasis_start()` | Begin the build from Barren. |
| `oasis_update(elapsed_ms)` | Advance the settlement stage; same fixed-step and clamping rules as `sim_update`. |
| `oasis_render()` | Draw the settlement into its own 320×80 framebuffer. |
| `oasis_framebuffer()` / `oasis_framebuffer_len()` | Pointer and byte length of the settlement framebuffer. |
| `oasis_focus(k)` | Focus building k, or -1 to clear. Fast-forwards the build if k is not yet built. |
| `oasis_replay()` | Reset to Barren and start again. |
| `oasis_building_at(x, y)` | Hit test in logical pixels; returns k or -1. |
| `oasis_render_static()` | Draw the finished settlement once, for mobile. |

Both stages push into the one event queue read by `sim_poll_event`; each packed event carries a stage id. Oasis events: `BUILDING_DONE(k)`, `BUILD_COMPLETE`, `HERO_ARRIVED(k)`.

Memory is static: fixed-size arrays sized for the caps in sections 5 and 7, a bugs framebuffer of 2 × 96 × 320 px, and an oasis framebuffer of 320 × 80 px. No allocator.

### Vignette interface (inside C)

```c
typedef struct {
    void (*enter)(World *w);
    void (*update)(World *w, int dt_ms);
    void (*draw)(World *w, Framebuffer *fb);   /* vignette props and overlays only */
    int  (*at_beat_boundary)(const World *w);
    int  (*is_done)(const World *w);
    void (*exit)(World *w);
} Vignette;
```

Vignettes script the actors and props; they never touch the framebuffer except through `draw.h` and `sprites.h`.

### Shim responsibilities

Instantiate the module with `WebAssembly.instantiateStreaming`; compute logical sizes and integer scale on resize via `ResizeObserver` and call `sim_init`; run one `requestAnimationFrame` loop that calls `sim_update` then `sim_render`, wraps the framebuffer in an `ImageData`, and blits each half to its canvas through a small offscreen canvas with smoothing off; drain `sim_poll_event` each frame and update card glow and counter; attach hover and focus handlers to case files; handle the desktop media query, reduced motion, and the debug query parameter. In phase 2 it also drives the oasis stage: an `IntersectionObserver` for start and pause, the same blit path into the landscape canvas, project card hover and focus, canvas hit testing, and the replay button.

## 10. Frame loop and rendering

- Fixed logic step of 1/60s inside C with an accumulator; the shim calls `sim_update` with real elapsed milliseconds once per animation frame, then `sim_render`.
- Elapsed time is clamped to 250ms so the world never fast-forwards after a tab switch.
- The shim stops scheduling frames on `visibilitychange` to hidden and resumes with a fresh timestamp.
- **Static mode**: the shim instantiates the module, calls `sim_init`, renders exactly once, blits, and never schedules a frame. On desktop with reduced motion it calls `sim_render_static` for both panels; on mobile it calls `sim_render_avatar` for the avatar canvas.
- Rendering order per panel: background and grid, ground line, props behind actors, bugs, hero, fx, vignette overlays such as the detective dim.
- Floating point is allowed in C (wasm has native float ops); libm functions are not. Positions use fixed-point or float arithmetic without `sqrt`, `sin`, or similar.

## 11. Build and toolchain

- **Compiler:** clang with the `wasm32` target. Apple's bundled clang lacks it. Preferred install is Zig, which ships a wasm-capable clang as `zig cc`; Homebrew LLVM is the alternative.
- **Wasm build:** one command in the Makefile, in spirit:

  ```sh
  clang --target=wasm32 -nostdlib -ffreestanding -fvisibility=hidden -O2 \
        -Wl,--no-entry -o sim.wasm sim/*.c sim/vignettes/*.c
  ```

  Exports are marked with `__attribute__((export_name("...")))`, which exports them despite hidden default visibility; the linker exports linear memory by default. The compiler may lower struct copies and zero-fills to `memcpy` and `memset` calls, which `freestanding.c` provides. With Zig the same flags apply through `zig cc --target=wasm32-freestanding`. Optional `wasm-opt -Oz` from Binaryen for a further size reduction; not required.
- **Debug build:** add `-g` for DWARF so Chrome's C/C++ DevTools extension can step through source.
- **Native test build:** the same sources minus `freestanding.c`, compiled with the system compiler and `-fsanitize=address,undefined`, linked with the test files into one binary. `make test` builds and runs it.
- **Serving:** any static file server. The `.wasm` file must be served as `application/wasm`; the shim falls back to `WebAssembly.instantiate` on an array buffer if streaming instantiation is rejected.

## 12. Accessibility and motion

- Canvases carry `aria-hidden="true"` and `role="presentation"`.
- Case files are ordinary focusable elements with real text; the page is fully readable with no animation.
- Under `prefers-reduced-motion: reduce`, the shim uses static mode with `sim_render_static`: each panel shows one frame of the character mid-bonk with a few bugs, and no loop runs.
- The counter is not announced live.

## 13. Resilience

- The failure mode is always "hero card still works." Wasm fetch or instantiation failure, canvas creation failure, or any shim exception falls back to the hero-only layout with the panels left as plain background colour.
- Resize is handled by `ResizeObserver`, debounced by about 200 ms so a window drag does not restart the scene on every pixel; the shim recomputes the integer scale and calls `sim_init` again only when logical sizes change, otherwise just re-letterboxes.
- The resume link and all below-the-fold content are plain HTML and never depend on JavaScript or wasm. If the oasis stage fails for any reason the project cards still read as plain text and the canvas stays sand-coloured.

## 14. Performance and load budget

- `sim.wasm` under 40KB uncompressed in phase 1 and under 64KB with the oasis stage; `shim.js` under 3KB gzipped in phase 1 and under 5KB gzipped in phase 2. Comparable to the earlier JavaScript design and with fewer requests.
- The hero card renders before any script runs; the module script is deferred, so first paint is unaffected.
- At most 20 bugs per side; the bugs renderer touches at most 2 × 96 × 320 pixels per frame and the oasis renderer 320 × 80. Target under 1ms of CPU per frame per stage including the blit. The oasis stage only updates while its section is in view.
- No web fonts in version one; system monospace stack for the hero card.

## 15. Testing

- **Native unit tests** in C, run by `make test` under AddressSanitizer and UndefinedBehaviorSanitizer:
  - Scene: a request mid-beat waits; a request at a boundary swaps; a request for the current vignette is ignored; a newer pending request replaces an older one; the idle timer fires after 45s and never repeats the last vignette; a finished vignette returns to patrol; crossover triggers on bug imbalance and on the 120s timer but never during a linked vignette.
  - Hero: walk-to-target arrives and stops; crossover duration equals gap width over walk speed.
  - Bugs: wander stays within panel bounds; cap is respected.
  - Events: queue order is preserved; overflow drops the oldest; packed encoding round-trips.
  - RNG: same seed yields the same sequence.
  - Draw: `fill_rect` clips at framebuffer edges; the dim overlay reduces every channel; bitmap text renders the expected pixels for a known glyph.
- **Headless vignette tests:** step each vignette's `update` with a fixed dt and assert phase progression. For `race`: setup → rush → collide → lock → queue → resolved → done.
- **Oasis stage tests (phase 2):**
  - Sequencer: phases advance in order with the durations in section 7.2; `BUILDING_DONE` is emitted exactly once per building in slot order; `v0.5` appears when half the buildings are done and `v1.0` at the last; `BUILD_COMPLETE` follows the wink.
  - Fast-forward: focusing an unbuilt building reaches `BUILDING_DONE(k)` in less simulated time than a normal run, and the rate returns to normal afterwards.
  - Replay resets to Barren; skipping `oasis_update` calls preserves state exactly.
  - Hit test returns k at each slot's centre and -1 on sand and water; unknown kinds fall back to house; N clamps to 1 to 6.
  - Canal for building k exists only after `BUILDING_DONE(k)`.
- **Browser verification** via Chrome DevTools: screenshots at 1440×900 (desktop grid) and 390×844 (mobile fallback with avatar); each vignette via `?vignette=`; hover linking and card glow; `prefers-reduced-motion` emulated; the network panel confirming wasm size and `application/wasm` content type. Phase 2 adds: scroll into the settlement section and screenshot the Barren, mid-build, and Ambient states; project card hover and canvas hover both ways; replay; the static frame on mobile.

## 16. Out of scope for version one

- Real sprite artwork and sound.
- Clicking bugs to squash them or steering the character.
- Analytics.
- Below-the-fold content beyond a placeholder section.
- Persisting the counter across visits.
- Minification or a bundling step; the shim is small enough to ship as written.
- Settlement ideas held for later: a sandstorm setback mid-build, and the side-panel character walking off its panel to arrive in the settlement on scroll.
- Animated settlement on mobile; mobile gets the static finished frame.

## 17. Author-supplied content

These are content inputs, not design decisions, and can be filled in at any time before launch:

- Name, title line, pitch sentence.
- Three case files: tag (fixed above), symptom line, fix line, optional write-up link.
- Email address, GitHub URL, resume PDF.
- The source repository URL for the engine credit line.
- Two to five project cards for the settlement section: project name, what it was, outcome, role, and a building kind from the catalogue in section 7.5.
