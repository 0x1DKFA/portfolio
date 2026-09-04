# Bug Smasher Profile — Phase 1b follow-ups

Deferred minor findings from the phase 1b task reviews, triaged as safe to leave for a polish pass or phase 2, plus known polish ideas noted during the rework. Branch `phase-1b`, tag `phase-1b-complete`.

## Deferred from reviews
- trig_sin's float→int cast is undefined beyond |a| ≈ 1.35e10 rad; document or guard.
- map.c uses static scratch buffers (documented non-reentrant); the farthest-hiding-spot fallback is deterministic first-in-scan.
- texture_wall has no bounds guard on tex/u/v (callers clamp); sprite generator boilerplate repeated.
- decals: the 9th pair stays fully bright before fading; register_print uses HALF_LEN for both axes; the 8-slot tile cap skips registration silently on a full tile.
- sprite_shade duplicates raycast_shade's constants; DX/DY direction tables repeated in map.c, dog.c, hunt.c, world.c.
- (int) truncation toward zero for sub-pixel wall and sprite edges.
- trash.c: dead `open_x ||` disjunct in the gust fallback; axis-separated collision could corner-cut on non-rectangular maps.
- hunt.c: HUNT_NEW_TRAIL is vestigial; the redundant `!h->inspecting` clause after the early return in follow_path; lamp/trash sprites_add calls unguarded (44 of 64 slots used).
- sim.c: sim_render_static ignores world_init's return, clobbers the live world by design (undocumented), and sim_poll_event is not gated on g_ready; the 250 ms clamp test is indirect.
- shim.js: canvas backing reset on every applyMode; offscreen canvas recreated on mode changes.
- Footprint culling and HUD timers run inside the hunt step, before the camera step, so culling lags the camera by one frame (documented in spec §11).

## Polish ideas
- Variable wall heights.
- Rain.
- Visitor steering.
- Softer lamp light falloff.
- Drainpipes on walls.
- Flickering windows (time-varying pane hash).
- A shared shade helper in draw.h.
