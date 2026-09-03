# Bug Smasher Profile — Design Spec

- **Date:** 2026-09-03
- **Status:** Approved design, revision 4, ready for implementation planning
- **Owner:** Rohit Kumar
- **Revision:** 4 — the first screen is redesigned as a first-person alley hunt (a Doom-style raycaster) behind a compact card; the side panels, vignettes, and hover linking from revisions 1 to 3.1 are removed. Section 7 (settlement stage) is unchanged in intent.

## 1. Purpose and audience

A single-page personal profile aimed primarily at recruiters and hiring managers. The page must deliver name, pitch, and contact within seconds. Behind the card, a single ambient animation shows the author's debugging character: a first-person walk through the back alleys of a night-time city, following a trail of glowing footprints to a hidden bug, smashing it with a toy hammer, and starting again. It shows, rather than claims, patience and method in finding bugs.

Below the fold, a second animated stage (phase 2) shows a different strength: leading greenfield projects from nothing to a thriving state. See section 7.

A secondary goal is a credibility signal for technical hiring managers: the animation engine is written in C and compiled to WebAssembly with no framework and no art assets, and the page says so with a link to the source.

### Delivery phases

- **Phase 1 (done, superseded):** two side panels with hover-linked vignettes. Its engine core (drawing primitives, font, RNG, events, stage abstraction, build, tests) carries forward.
- **Phase 1b (this revision):** the first-person alley hunt as the only first-screen animation; removal of the side panels, vignettes, and hover linking.
- **Phase 2:** the settlement stage (section 7), added as a second stage to the same module.

## 2. Layout

### Desktop (viewport width 1024px and above)

- **Scene canvas.** One `<canvas>` fixed to the viewport behind everything, rendering the first-person view at an integer scale that covers the viewport (at most one scale unit of overflow, cropped; no letterbox).
- **Hero card.** A compact panel floating over the scene, left-aligned: width `clamp(320px, 34vw, 520px)`, a 5vw left margin, vertically centred with a minimum 6vh margin, dark and nearly opaque (about 92% alpha) with a soft shadow so text stays readable over motion. The right two thirds of the screen show the alley, its centre, and the hammer.
- **Below the fold.** The settlement section (section 7) and a plain placeholder section follow the first screen. They have an opaque background so the fixed canvas never shows through behind text.

### Mobile (below 1024px)

Single column: the card takes the full width with margins. The scene canvas renders **one static frame** of the alley as a still backdrop and never loops. There is no separate avatar.

Switching between layouts at runtime (a resize across 1024px) re-evaluates the mode via `matchMedia`.

## 3. Hero card content

Top to bottom:

1. **Identity:** name, a one-line title, and a one-sentence pitch. Copy is author-supplied.
2. **Three case files**, plain text: a tag (Race condition, Heisenbug, Regression), a one-line symptom, a one-line fix, an optional link. They no longer trigger anything and are not focusable.
3. **Contact row:** email, GitHub, resume PDF. No LinkedIn.
4. **Live counter:** "bugs squashed this visit: N", incremented on each smash. In-memory only.
5. **Engine credit**, one small line: the background is C compiled to WebAssembly, no framework, no art assets, with a link to the source repository.

## 4. Visual style

- **First person at Doom's resolution.** About 200 logical pixels tall, width following the viewport aspect, chunky integer-scaled pixels. Walls are texture-mapped, the floor is flat, the sky is a skyline strip, sprites face the camera.
- **Windy night.** Dark blue-black sky with skyscraper silhouettes and lit windows. Brick and concrete alley walls with grime, drainpipes, posters, lit windows in two or three colours (a few flicker), one neon sign with the author's first name and one generic sign. Street lamps brighten pools of floor. Distance fog darkens everything as it recedes.
- **Palette.** About sixteen colours generated with the textures: asphalt greys, brick reds, concrete, window ambers and cyans, neon magenta, puddle blue, footprint yellow, hammer yellow, dog browns.
- **Everything is procedural.** Textures, sprites, the skyline, and the HUD are generated at init from code and the seed. No image assets are loaded.
- **Bugs and dogs are cute.** Round cartoon bug with two antennae; friendly pixel dogs with wagging tails. Never realistic.

## 5. World: the map

- A **48 × 48 tile map** written as an ASCII block in C, parsed at init. Legend:

  | Char | Meaning |
  |---|---|
  | `#` | brick wall |
  | `=` | concrete wall |
  | `W` | window wall (lit and unlit panes decided by a hash of the tile) |
  | `N` | neon sign wall (the author's first name) |
  | `S` | generic sign wall (`OPEN`) |
  | `P` | poster wall |
  | `D` | dumpster (a wall tile with its own texture; bugs may hide beside it) |
  | `.` | alley floor (asphalt) |
  | `~` | puddle floor |
  | `L` | floor with a street lamp sprite; lights tiles within radius 3 |
  | `T` | floor with a trash can sprite; bugs may hide beside it |
  | `c` | floor with a loose paper sprite |
  | `n` | floor with a newspaper sprite |
  | `d` | floor, dog spawn |
  | `@` | floor, camera start |

- **Walkable** tiles are all floor kinds. Alleys are one or two tiles wide and form loops with a few dead ends so trails vary.
- **Light map.** Each floor tile has a light level 0 to 1: ambient 0.25 plus lamp contributions falling off linearly to radius 3. Walls take the light of the floor tile the ray hit from.
- **Hiding spots** are floor tiles adjacent to a `T` or `D`.
- The map is authored so that every floor tile can reach every other (one connected component); a test asserts it.

## 6. The hunt loop

### 6.1 Camera

Position in tile units, view angle in radians, field of view 85 degrees, eye height 0.5 of a 2.5-tile wall. Walking speed 1.4 tiles per second, turn rate 2.5 radians per second toward the target angle by the shortest arc. Head bob: the horizon and the HUD shift vertically by up to 3 logical pixels with the step phase while walking. The horizon row also shifts for the inspect dip (down 12 px over 0.6 s) and the celebrate hop (up 8 px and back over 0.5 s).

### 6.2 States

| State | Duration | Behaviour |
|---|---|---|
| LOOK | 1.5 s | Standing at the trail start, the view pans 20 degrees left, then right, then settles toward the first waypoint. |
| FOLLOW | path length / speed | Walk the footprint trail waypoint to waypoint; turn smoothly at corners; every 8 ± 3 s pause 0.6 s to dip and inspect a print. |
| APPROACH | until in range | Within 2.5 tiles of the hiding spot the bug peeks out; the target angle becomes the bug's bearing; keep walking until within 1.0 tile and facing within 0.2 rad. |
| SMASH | 0.4 s | Hammer swing; at 0.25 s the hit lands: dust puff sprite at the bug, bug removed, `+1` flash for 0.8 s, `BUG_SQUASHED` event. |
| CELEBRATE | 1.0 s | A small hop. |
| NEW_TRAIL | instant | Pick a hiding spot at least 12 BFS steps away; lay a new trail; back to LOOK. |

A full cycle takes 30 to 60 s. Given a seed, the whole run is deterministic.

### 6.3 Trail and footprints

- The trail is the breadth-first-search path over floor tiles from the current tile to the hiding spot, as a list of tile centres.
- **Footprint decals** are laid in pairs every 0.5 tile along the polyline, oriented along the segment, alternating left and right of the centre line by 0.15 tile. They live in a decal grid (at most two decals per tile) sampled by the floor caster.
- **Glow** is brightest for the eight pairs ahead of the camera along the trail and fades to a dim amber behind; prints behind the camera are removed once more than six tiles back.

### 6.4 Dogs

Two or three dog sprites spawned at `d` tiles. Each wanders: choose a random open neighbour tile weighted to keep direction, walk at 1.0 tile per second, reverse at dead ends. Near a footprint or trash can it may pause 1 to 2 s in a sniff pose. Dogs never block the camera or each other; they are drawn, not collided with.

### 6.5 Trash and wind

Loose items (`c`, `n`) carry a velocity. A **gust** occurs every 4 to 10 s: for 1.2 s it pushes each loose item along its alley's open axis at up to 2 tiles per second, choosing the direction randomly per gust. Friction decays velocity to zero within a second after the gust; walls stop items. A tumble frame plays while moving. Trash cans, dumpsters, and lamps do not move.

### 6.6 The bug

One hidden bug per cycle at the hiding spot, drawn as a sprite half-hidden beside its trash can or dumpster. Frames: hidden, peek, exposed. It becomes `peek` during APPROACH and `exposed` when the camera is within 1.5 tiles.

## 7. Greenfield settlement stage (phase 2)

Shows that the author starts things and leads them, not only fixes them. It is a second **stage** in the same engine with its own framebuffer, palette, sequencer, content, and hover linking. It reuses the engine's drawing primitives, event queue, and RNG. It is unchanged from revision 3 except where noted.

### 7.1 Placement and layout

- A below-the-fold section titled in the spirit of "Built from scratch", directly under the first screen, with an opaque background.
- One landscape `<canvas>` spanning the section width. Fixed logical size **320×80**, integer-scaled by width (`scale = floor(canvasBackingWidth / 320)`), letterboxed vertically. Decorative and hidden from assistive tech.
- Beneath the canvas, two to five **project cards**, `<article class="project" data-building="k" data-kind="watchtower">`, each with a project name, one line on what it was, one line on the outcome, and the author's role. Each card corresponds to one building. Copy is author-supplied.
- A small pixel **shovel button** in the section corner replays the build.
- **Palette:** sand, cracked-earth brown, teal water, palm green, terracotta, and warm lantern amber, with a deep indigo sky at dusk. Distinct from the night alley palette so the section reads as a new chapter.
- **Mobile:** the canvas shows the finished settlement as one static frame via `oasis_render_static`, and no loop runs.

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
- **Fast-forward:** focusing a building that does not exist yet during phases 1 to 6 runs the sequencer at 8× speed until that building completes, then resumes normal speed.

### 7.4 Project linking

Hover intent of 150 ms.

- **Card → canvas:** hover or keyboard focus on a project card calls `oasis_focus(k)`. The character walks to building k and the building glows for as long as the card is hovered or focused. Pointer leave or blur calls `oasis_focus(-1)`; the character wanders back to the bench after 5 seconds idle.
- **Canvas → card:** pointer movement over the canvas is converted to logical pixels and passed to `oasis_building_at(x, y)`. A hit highlights the matching card and calls `oasis_focus(k)`; the cursor becomes a pointer.
- **Event:** when the character reaches the focused building the stage emits `HERO_ARRIVED(k)`.

### 7.5 Buildings, scenery, and crew

- **Fixed scenery:** oasis pool at the centre, three palms, the well, the flag and sign, the `v0.1` hut, the paving grid, the bench.
- **Project buildings** occupy fixed slots alternating left and right of the oasis, moving outward, for one to six buildings. Kinds: `watchtower`, `market`, `waterwheel`, `granary`, `aqueduct`, `library`, `forge`; unknown falls back to `house`.
- **Growth animation:** each building rises from foundation to roof over about 0.7s, with crew hammering at its slot.
- **Canals:** a thin channel extends from the oasis to each building as it completes.
- **Crew:** three small pixel people; after phase 5 they hammer at the slot under construction, and in Ambient they walk between buildings.
- **Milestone tags** use the 3×5 bitmap font.

## 8. Events and debug hooks

- **Events** keep the packed integer format `(type << 24) | (stage << 16) | (a << 8) | b` on one queue read by `sim_poll_event`. The hunt stage (stage 0) emits only `BUG_SQUASHED` (type 3). The vignette START and END types are retired. Phase 2 adds the oasis events from 7.3 and 7.4.
- **Debug hooks** are query parameters read by the shim: `?seed=N` fixes the simulation seed so a scene reproduces; `?motion=reduce` forces static mode. The `?vignette=` hook is removed.
- There is no hover or focus linking on the first screen.

## 9. Architecture

Two layers with one narrow boundary:

- **Page layer:** HTML, CSS, and a single small JavaScript shim, the only code that touches the DOM, the canvas, or browser events.
- **Simulation layer:** C compiled to a freestanding WebAssembly module with **no imports** and no libc. It owns all state, behaviour, timing, textures, and pixel rendering into RGBA framebuffers in its own linear memory.

The simulation hosts **stages**, each with its own framebuffer, fixed-step accumulator, palette, and state machine, sharing drawing primitives, RNG, event queue, and the `Stage` struct:

- **Hunt stage** (`sim_*` exports): the first-person alley hunt, sections 4 to 6.
- **Oasis stage** (`oasis_*` exports): the settlement section, section 7. Phase 2.

```
index.html
styles.css
shim.js                  wasm load, frame loop, blit, counter, fallbacks (~50 lines)
sim/
  export.h               SIM_EXPORT macro
  rng.h / rng.c          mulberry32
  events.h / events.c    packed event queue
  draw.h / draw.c        Framebuffer, Color, view/clip, rect, dim
  font.h / font.c        3x5 bitmap glyphs, draw_text
  fmath.h                abs, min, max, clamp, Newton sqrt
  trig.h / trig.c        sin/cos table (1024 entries) filled at init, atan2 approximation
  stage.h / stage.c      Stage struct and fixed-step accumulator
  palette.h / palette.c  night palette
  map.h / map.c          ASCII map, parsing, cell queries, light map, BFS, hiding spots
  textures.h / textures.c procedural wall, floor, sky, sprite, and HUD bitmaps generated at init
  camera.h / camera.c    position, angle, bob, pitch offset, steering toward waypoints
  raycast.h / raycast.c  wall casting with depth buffer, floor casting with decals, sky strip
  sprites.h / sprites.c  sprite list, projection, depth-tested drawing
  decals.h / decals.c    footprint decal grid
  actors/dog.c / dog.h   wandering dogs
  actors/trash.c / .h    loose trash and gusts
  actors/bug.c / .h      the hidden bug
  hud.h / hud.c          hand and hammer overlay, +1 flash
  hunt.h / hunt.c        LOOK / FOLLOW / APPROACH / SMASH / CELEBRATE / NEW_TRAIL state machine
  world.h / world.c      World struct: map, camera, decals, sprites, actors, rng, events, time
  sim.h / sim.c          exports
  freestanding.c         memset, memcpy, memmove for the wasm build
test/                    native tests, wasm_smoke.mjs
docs/superpowers/        this spec, plans
```

Removed from revision 3: `bug.c`/`hero.c` (side view), `vignettes/`, `scene.*`, the old `sprites.c`, the two-panel `world.c`.

### Exported API (hunt stage)

All exports are integers or pointers into wasm memory. No strings.

| Export | Purpose |
|---|---|
| `sim_init(seed, w, h)` | Reset all state and regenerate textures for a logical framebuffer of `w × h` (w 200..640, h 160..320). Returns 0, or -1 for bad sizes. |
| `sim_update(elapsed_ms)` | Advance the fixed-step accumulator; runs as many 1/60s steps as needed, clamped to 250 ms. |
| `sim_render()` | Draw the current view into the framebuffer. |
| `sim_framebuffer()` / `sim_framebuffer_len()` | Pointer and byte length of the RGBA buffer (`w × h × 4`). |
| `sim_poll_event()` | Pop the next packed event, or 0. |
| `sim_render_static()` | Arrange a fixed composition (mid-trail, a lamp ahead, footprints, the bug peeking) and draw it once, for mobile and reduced motion. |
| `memory` | The module's linear memory. |

Phase 2 adds the `oasis_*` exports listed in revision 3 (init, set_kind, start, update, render, framebuffer, focus, replay, building_at, render_static), unchanged.

Memory is static: the framebuffer buffer is sized `640 × 320 × 4`; textures, the skyline strip, the map, the decal grid, and the actor arrays are fixed-size arrays. No allocator.

### Module interfaces (inside C)

- `map_parse(const char *const rows[], Map*)`, `map_is_floor(x, y)`, `map_wall_kind(x, y)`, `map_light(x, y)`, `map_bfs(from, to, out_path, max)`, `map_pick_hiding_spot(rng, from, min_steps)`, `map_is_connected()`.
- `camera_step(Camera*, dt)`: moves toward `target` at walk speed when `walking`, turns toward `target_angle` at the turn rate, advances the bob phase; `camera_face(Camera*, x, y)` sets the target angle by `trig_atan2`.
- `raycast_walls(World*, Framebuffer*, float *depth)`, `raycast_floor(World*, Framebuffer*)`, `raycast_sky(World*, Framebuffer*)`.
- `sprites_draw(World*, Framebuffer*, const float *depth)` sorts and draws every active sprite.
- `decals_clear`, `decals_lay_trail(Decals*, const Path*)`, `decals_sample(Decals*, fx, fy, out_glow)`, `decals_cull_behind(Decals*, trail_index)`.
- `hunt_init(World*)`, `hunt_step(World*, dt)`, `hunt_state(const World*)`, `hunt_cycle_count(const World*)`.
- `hud_draw(World*, Framebuffer*)`.

## 10. Renderer

- **Walls.** One ray per screen column through the tile grid (DDA). Perpendicular distance fills a per-column depth buffer. Wall height 2.5 tiles, eye at 0.5: `lineTop = horizon - (2.0 / dist) * proj`, `lineBottom = horizon + (0.5 / dist) * proj`, where `proj` is the projection constant for the field of view. Texture column from the hit fraction; texture row from the vertical position; shading from distance fog `1 / (1 + 0.35 * dist)`, a side factor (0.8 for north/south faces), and the light map.
- **Floor.** For each row below the horizon: `rowDist = 0.5 * proj / (row - horizon)`; per column, the world point along the ray; tile kind chooses asphalt or puddle; the light map and fog shade it; the decal grid supplies footprint colour and glow where a footprint covers the point.
- **Sky.** Rows above the walls copy from the skyline strip: `skyColumn = ((angle + columnAngle) / 2π) * 1024 mod 1024`, row from the screen row. Generated at init: a gradient, stars, tower silhouettes of varying height and width with lit window dots decided by the seed.
- **Sprites.** Each sprite has a world position, kind, frame, and size in tiles. Per frame: transform into camera space, cull behind the camera, sort far to near, compute screen x and height, draw its texture column by column where `dist < depth[column]`, shaded by fog and the light at its tile.
- **Textures.** 64×64 walls (brick, concrete, window with per-tile hash for lit panes, neon name sign, generic sign, poster, dumpster), 64×64 floor (asphalt, puddle), 32×32 sprites (dog trot ×2, dog sniff, trash can, paper ×2, newspaper ×2, bug hidden/peek/exposed, dust ×2), 32×64 lamp post, 96×64 hand-and-hammer ×3 frames. All generated at init into static arrays.
- **HUD.** The hammer frame at bottom centre offset by the bob; the `+1` flash drawn with `draw_text` scaled ×3 above the hammer for 0.8 s after a hit.
- **Math.** `trig_sin`/`trig_cos` from a 1024-entry table filled at init by a degree-7 polynomial (error under 1e-4); `trig_atan2` from a rational approximation (error under 0.005 rad); `fm_sqrt` by Newton's method. No libm.

## 11. Frame loop and rendering order

- Fixed logic step of 1/60s inside C with an accumulator; the shim calls `sim_update` once per animation frame with real elapsed milliseconds (clamped to 250 ms), then `sim_render`.
- Per step: gusts and trash, dogs, the hunt state machine (which drives the camera), the camera, decal culling, HUD timers.
- Per render: sky, walls (filling the depth buffer), floor, sprites, HUD.
- The shim stops scheduling frames when the tab is hidden and resumes with a fresh timestamp.
- **Static mode** (mobile and reduced motion): `sim_init`, `sim_render_static`, one blit, no loop.

## 12. Build and toolchain

- **Compiler:** clang with the `wasm32` target via `zig cc` (Zig 0.16 installed). Homebrew LLVM is the alternative.
- **Wasm build:** one Makefile command with `-target wasm32-freestanding -std=c11 -nostdlib -ffreestanding -fno-builtin -fvisibility=hidden -mbulk-memory -O2 -g0 -Wall -Wextra -Isim -Wl,--no-entry -Wl,--strip-all`. Exports marked with `__attribute__((export_name("...")))`. The build is reproducible (identical bytes across runs).
- **Native test build:** the same sources with the system compiler and `-fsanitize=address,undefined -Wall -Wextra`; `make test` builds and runs.
- **Serving:** any static host. `.wasm` should be served as `application/wasm`; the shim falls back to array-buffer instantiation otherwise.

## 13. Accessibility and motion

- The scene canvas carries `aria-hidden="true"` and `role="presentation"` and sits behind all content.
- The hero card is ordinary HTML; the page is fully readable with no animation. Case files are not interactive.
- Under `prefers-reduced-motion: reduce`, and on mobile, static mode: one frame, no loop.
- The counter is not announced live.

## 14. Resilience

- The failure mode is always "hero card still works." Any wasm or canvas failure adds `no-sim` to `body`; the canvas stays a plain dark background and the card is untouched.
- Every shim callback (frame loop, resize, media-query change, visibility change) routes exceptions to the fallback.
- Resize is debounced by about 200 ms; the shim re-inits only when the logical framebuffer size changes, which restarts the hunt; otherwise it only rescales.
- The resume link and below-the-fold content never depend on JavaScript or wasm.

## 15. Performance and load budget

- `sim.wasm` under 64 KB uncompressed (textures are generated, not stored); under 96 KB with the oasis stage.
- `shim.js` under 2.5 KB gzipped.
- Under 3 ms of CPU per frame at 356×200 on a 2020 laptop; the renderer touches at most `w × h` pixels plus sprite columns per frame.
- The hero card paints before any script runs; the module script is deferred.
- No web fonts; system monospace stack. No requests beyond `index.html`, `styles.css`, `shim.js`, `sim.wasm`, and a `data:` favicon.

## 16. Testing

- **Native unit tests** (`make test`, ASan/UBSan, zero warnings):
  - Trig: table values at 0, π/6, π/4, π/2, π within 1e-3; `atan2` quadrants within 0.01.
  - Map: parse dimensions and legend counts on the real map; connectivity; BFS shortest path on a known 8×8 map; hiding spot at least `min_steps` away and adjacent to `T` or `D`.
  - Raycast: on a 7×7 test map with the camera at the centre facing +x, the centre column hits at distance 3 ± 0.01 and its wall height matches the formula; depth is monotonic along a corridor; floor row-to-distance mapping matches the formula at three rows; sky column wraps.
  - Sprites: a sprite straight ahead at distance 2 projects to the centre column with the expected height; behind the camera it is culled; behind a wall it is occluded; sort order is far to near.
  - Camera: turns the short way around ±π; arrives at a waypoint and stops; bob amplitude within 3 px.
  - Decals: a trail lays the expected number of pairs; sampling on a print returns glow > 0 and off it returns 0; culling removes prints more than 6 tiles behind.
  - Dogs stay on floor tiles for 60 s; trash moves only during gusts and stops at walls.
  - Hunt: a full cycle from LOOK to NEW_TRAIL emits exactly one `BUG_SQUASHED`, lays a new trail of at least 12 steps, and is deterministic for a seed (two runs, same event frames).
  - Exports: init bounds; framebuffer length; painted pixels in all four quadrants of a frame; static frame contains footprint yellow and hammer yellow; 250 ms clamp.
- **Smoke:** `node test/wasm_smoke.mjs` checks zero imports, the eight exports, size, a rendered frame, and at least one squash event within 120 s of simulated time.
- **Browser:** screenshots at 1440×900 (card left, alley right, hammer at bottom) and 390×844 (card full width over a still frame); `?motion=reduce`; console clean; network shows exactly the four files; the renamed-module fallback shows a plain card.

## 17. Out of scope for this revision

- Visitor steering (keyboard or mouse look), variable wall heights, rain, sound.
- The settlement stage (phase 2) implementation.
- Real sprite artwork; everything stays procedural.
- Persisting the counter across visits.
- Minification or bundling.

## 18. Author-supplied content

- Name, title line, pitch sentence.
- Three case files: tag, symptom line, fix line, optional link.
- Email address, GitHub URL, resume PDF.
- The first name to render on the neon sign (defaults to `ROHIT`).
- Phase 2: two to five project cards with a building kind each.
