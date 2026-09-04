# Portfolio

A personal profile page. One fixed, full-viewport canvas renders a first-person alley hunt — a raycaster written in C, compiled to a freestanding WebAssembly module with no imports and no framework — behind a compact hero card, driven by a small JavaScript shim. The card is plain HTML and CSS.

## Layout

```
index.html  styles.css  shim.js   the page, its canvas and card, and the ~110-line shim
sim.wasm                          the compiled simulation (committed; static hosts serve it)
sim/                              C sources: trig, map, textures, camera, raycast, sprites, decals, actors, hud, hunt, world
test/                             native test suite and a Node smoke test for the module
docs/superpowers/                 design spec and implementation plans
```

## Build and test

Requires `zig` (for `zig cc` with the wasm32 target) and Node 18+.

```
make test        # native tests with ASan/UBSan
make sim.wasm    # rebuild the module
make smoke       # instantiate sim.wasm in Node and render a frame
make serve       # http://localhost:8000
```

Debug query parameters: `?seed=N` fixes the simulation seed; `?motion=reduce` shows the static frame.

## Status

Phase 1b complete; phase 2 (settlement) planned.
