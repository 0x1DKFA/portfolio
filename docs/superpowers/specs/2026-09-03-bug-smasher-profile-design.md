# Bug Smasher Profile — Design Spec

- **Date:** 2026-09-03
- **Status:** Approved design, ready for implementation planning
- **Owner:** Rohit Kumar

## 1. Purpose and audience

A single-page personal profile aimed primarily at recruiters and hiring managers. The page must deliver name, pitch, and contact within seconds, while pixel-art side animations show, rather than claim, the author's debugging strengths: fixing race conditions, hunting hard-to-reproduce bugs, and preventing regressions.

Success looks like: a recruiter lands on the page, reads the pitch and case files without distraction, notices the side animation acting out the case file they are looking at, and finds the contact links immediately. On a phone they get the same information with no animation.

## 2. Layout

### Desktop (viewport width 1024px and above)

Three-column CSS grid, `grid-template-columns: 1fr 2fr 1fr`, filling the viewport height. The first screen does not need to scroll.

- **Left and right columns:** one `<canvas>` each, rendering slices of a single shared world (see section 6).
- **Middle column:** the hero card, a solid surface on top of the world. The character can walk *behind* it: it disappears at one panel's inner edge and reappears at the other's after a delay proportional to the card's width.

### Mobile (below 1024px)

Single column containing only the hero card. Canvases are never created and no animation loop runs. A single static pixel-art frame of the character mid-bonk is shown above the name as an avatar.

Switching between the two layouts (for example a window resize across 1024px) creates or tears down the panels at runtime via `matchMedia`.

## 3. Hero card content

Top to bottom:

1. **Identity:** name, a one-line title, and a one-sentence pitch. Copy is author-supplied. Example pitch, for tone only: "I fix the bugs that only happen in production, for one customer, on Fridays."
2. **Three case files.** Each is a focusable `<article class="case-file" data-vignette="...">` containing a tag, a one-line symptom, a one-line fix, and an optional link to a longer write-up. The three tags and their linked vignettes:
   - `Race condition` → `race`
   - `Heisenbug` → `detective`
   - `Regression` → `regression`
3. **Contact row:** email, GitHub, resume PDF. No LinkedIn.
4. **Live counter** in a corner of the card: "bugs squashed this visit: N". Increments from scene events. In-memory only, resets each visit.

Anything else (experience, skills list) lives below the fold in a plain HTML placeholder section and is out of scope for version one.

## 4. Visual style

- Pixel art, retro-arcade tone. Version one uses **code-drawn placeholders**: filled rectangles on a logical pixel grid. Real sprite sheets can replace them later through `sprites.js` alone (section 9).
- Dark navy background, a very faint grid, a ground line at roughly 85% of panel height.
- Palette of five or six muted colours so the panels sit visually behind the card.
- **Bugs:** round, cute, two antennae, stub legs, three colour variants. Never realistic. Roughly 10×8 logical px.
- **Character:** small pixel person with a bright toy hammer. Roughly 14×20 logical px. Gains a deerstalker hat and magnifying glass in the detective vignette.
- **Squash effect:** pixel dust puff (about 0.4s) and a small floating "+1".

## 5. World rules

- **Single world, two views.** World x-coordinates run continuously across left panel, hero gap, and right panel. Each canvas draws the slice it covers.
- **Logical resolution.** Each panel is 96 logical px wide. Logical height is derived from the panel's aspect ratio. The hero gap's logical width is derived from the card's CSS width at the same scale.
- **Integer scaling.** `scale = floor(canvasBackingWidth / 96)`. Content is rendered to an offscreen canvas at logical resolution and blitted up with `imageSmoothingEnabled = false`. Any remainder is letterboxed with the background colour.
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
6. **Resolved (0.5s):** the box turns green; the character wipes their brow.

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

## 7. Content linking

- **Hover intent:** `pointerenter` on a case file starts a 150ms timer; on expiry the scene receives `request(vignetteName)`. `pointerleave` cancels the timer only. Sweeping the cursor across a card does nothing.
- **Keyboard:** `focusin` on a case file requests immediately.
- **Request semantics:** the scene holds at most one pending request. A newer request replaces an older pending one. A request for the currently playing vignette is ignored. The swap happens at the current vignette's next beat boundary. Target worst-case latency: about one second.
- **Location:** the vignette plays wherever the character is. A crossover in progress finishes first.
- **Feedback in the card:** while a vignette plays, its case file gets a subtle glow and a small pixel play icon. Removed on `vignette:end`.
- **Leaving the card does not stop the vignette.** It always plays to completion, then patrol resumes.
- **Idle auto-play:** if no hover request has arrived for 45 seconds, the scene picks a linked vignette at random (never the same one twice in a row), plays it, and emits `vignette:start` with `auto: true` so the matching card is highlighted. This is the path most passive viewers will experience.
- **Debug hook:** a `?vignette=<name>` query parameter jumps straight to that vignette on load, for manual verification.

## 8. Architecture

Plain HTML, CSS, and JavaScript. ES modules loaded via one `<script type="module">`. No build step, no dependencies, no network requests beyond the page's own files (plus the resume PDF on click).

```
index.html
styles.css
js/
  main.js            DOM boot and wiring (only module that touches the DOM)
  world.js           canvases, shared coordinates, integer scaling, frame loop
  scene.js           vignette state machine, requests, idle timer, events
  rng.js             seeded random number generator
  sprites.js         all drawing of bugs, hero, props, fx (placeholder rects in v1)
  actors/
    bug.js           bug entity: wander, queue, hide, squash
    hero.js          character entity: walk to target, wind up, bonk, crossover
  vignettes/
    patrol.js
    race.js
    detective.js
    regression.js
    crossover.js
docs/superpowers/specs/   this document
test/                    Node test files (section 12)
```

### Module responsibilities

- **main.js:** evaluates `matchMedia('(min-width: 1024px)')` and creates or tears down panels on change; attaches hover/focus handlers to case files and forwards requests to the scene; listens for scene events and updates card glow and the counter; handles the debug query parameter and the reduced-motion media query.
- **world.js:** owns both canvases and the offscreen logical canvas; computes logical dimensions and integer scale on resize via `ResizeObserver`; runs the frame loop; draws background, ground, actors (via `sprites.js`), then delegates to the current vignette's `draw`.
- **scene.js:** holds `current` and `pending`; each tick calls `current.update(dt)`, then swaps if `pending` exists and `current.atBeatBoundary()`; returns to patrol when `current.isDone()`; runs the idle timer and crossover triggers; extends `EventTarget` and dispatches `vignette:start {name, auto}`, `vignette:end {name}`, `bug:squashed`.
- **actors/\*:** hold state and simple behaviours; no drawing, no DOM.
- **vignettes/\*:** script actors and props. No DOM. Uniform interface:

```js
export function create(world) {
  return {
    enter(),           // place props and actors
    update(dt),        // advance logic by a fixed step in seconds
    draw(ctx),         // draw vignette-specific props and fx on the logical canvas
    atBeatBoundary(),  // true when an interruption is safe
    isDone(),          // true when finished; scene returns to patrol
    exit(),            // remove props
  };
}
```

- **sprites.js:** `drawBug(ctx, bug, frame)`, `drawHero(ctx, hero, frame)`, `drawProp(ctx, kind, x, y, state)`, `drawFx(ctx, fx)`. Version one implements these with `fillRect` on the logical grid. A later sprite-sheet version replaces the function bodies with `drawImage` calls; call sites and frame indices do not change.
- **rng.js:** small seeded generator (for example mulberry32). The world is seeded from the clock in production and from a fixed value in tests.

## 9. Frame loop and rendering

- Fixed logic step of 1/60s with an accumulator; render once per `requestAnimationFrame`.
- Delta time clamped (for example to 0.25s) so the world never fast-forwards after a tab switch.
- Loop pauses on `visibilitychange` to hidden and resumes with a reset accumulator.
- Rendering order per panel: background and grid, ground line, props behind actors, bugs, hero, fx, vignette overlays (for example the detective dim).

## 10. Accessibility and motion

- Canvases carry `aria-hidden="true"` and `role="presentation"`.
- Case files are ordinary focusable elements with real text; the page is fully readable with no animation.
- Under `prefers-reduced-motion: reduce`, the loop never starts. Each panel draws one static frame: the character mid-bonk with a few bugs.
- The counter is not announced live.

## 11. Resilience

- The failure mode is always "hero card still works." Panel creation is wrapped in a try/catch; any error falls back to the hero-only layout.
- Resize is handled by `ResizeObserver`; the integer scale is recomputed and the remainder letterboxed.
- The resume link and all below-the-fold content are plain HTML and never depend on JavaScript.

## 12. Performance budget

- Under 50KB of unminified JavaScript in total.
- At most 20 bugs per side.
- Around 40 rectangle draws per panel per frame in the placeholder phase.
- No web fonts in version one; system monospace stack.

## 13. Testing

- **Unit tests** with Node's built-in test runner (`node --test`), no dependencies:
  - Scene: a request mid-beat waits; a request at a boundary swaps; a request for the current vignette is ignored; a newer pending request replaces an older one; the idle timer fires after 45s and never repeats the last vignette; a finished vignette returns to patrol.
  - Hero: walk-to-target arrives and stops; crossover duration equals gap width over walk speed.
  - Bugs: wander stays within panel bounds; cap is respected.
  - World: integer scale and logical height computation for several panel sizes.
  - RNG: same seed yields the same sequence.
- **Headless vignette tests:** step each vignette's `update` with a fixed dt and assert phase progression without a canvas. For `race`: setup → rush → collide → lock → queue → resolved → done.
- **Browser verification** via Chrome DevTools: screenshots at 1440×900 (desktop grid) and 390×844 (mobile fallback); each vignette via `?vignette=`; hover linking and card glow; `prefers-reduced-motion` emulated.

## 14. Out of scope for version one

- Real sprite artwork and sound.
- Clicking bugs to squash them or steering the character.
- Analytics.
- Below-the-fold content beyond a placeholder section.
- Persisting the counter across visits.

## 15. Author-supplied content

These are content inputs, not design decisions, and can be filled in at any time before launch:

- Name, title line, pitch sentence.
- Three case files: tag (fixed above), symptom line, fix line, optional write-up link.
- Email address, GitHub URL, resume PDF.
