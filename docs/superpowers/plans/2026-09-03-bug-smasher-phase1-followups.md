# Bug Smasher Profile — Phase 1 follow-ups

Deferred minor findings from the phase 1 task reviews and the final whole-branch review, triaged as safe to leave for a polish pass or phase 2. Branch `phase-1`, tag `phase-1-complete`.

## Code
- test/test.h includes <string.h> unused until later tests need memset.
- font.c BOX literal duplicates the '0' glyph pattern.
- sprites.c pixel offsets are unnamed magic numbers (inherited from plan text).
- scene idle_timer/since_crossover keep accumulating during a vignette, so a vignette longer than 45 s could trigger auto-play right after returning to patrol (plan-mandated; harmless at current 8-12 s durations).
- patrol retargets by Bug pointer; a recycled slot could be silently adopted as the target (cosmetic).
- file-static vignette state is a singleton per module (by design for one World).
- hero vanishes when its centre crosses the inner edge (renderer gates on world_hero_side); a softer emerge/exit is a polish item.
- race.c 0.3f settle constant is test-derived; document or name it.
- regression BOUNCE bouncer spawns at the far panel edge; if the hero stands within ~25 px of the opposite edge the rush (30 px/s) exceeds the 2.0 s phase and the bug idles by the shield until exit clears it. Suggested fix: spawn the bouncer at most 50 px from the shield. Final review to triage.
- race.c phase compare lacks the epsilon tolerance regression.c and hero.c use; standardise.
- REG_EPS duplicates hero.c's inline 1e-4f literal.
- sim_init() on re-init does not emit END for a live vignette; the shim must clear `is-playing` from all cards whenever it calls sim_init (resize re-init).
- sim_render_static's canned scene ignores spawn NULLs; patrol enter consumes two rng draws on reset.
- make -g0 overridable (WASM_DEBUG ?= -g0) with a comment so a future debug build isn't silently stripped; sim.wasm committed with mode 100755.
- focus indicator on case files is subtle (1 px border shift); meta description is not bracketed placeholder copy; below-fold section sits outside <main>.
- on a mid-session failure the canvases keep their last frame rather than blanking (acceptable; note for polish). BG literal duplicates --bg. gapW rounds while panelH floors.

## From the final review (kept deferred)
- Guard `sim_init` against `gap_w < panel_w` (or filter fx by side) so cross-panel clipping never depends on the gap width.
- `S.shield_x` in regression.c is not clamped to the panel; the bouncer spawn is, which covers the visible case.
- Some mode switches render the static frame twice back-to-back (harmless).
- Launch gate, not a merge gate: fill in the bracketed copy, real contact links, and add `resume.pdf` before deploying.

## Process notes
- Plan defects found during implementation: include guards colliding with size constants (FONT_H, BUG_H, HERO_H), a missing palette.h include in two files, the crossover reappearance point, the race queue cadence, a float tolerance on regression phase boundaries, and the detective dim covering both panels. All fixed on the branch.
