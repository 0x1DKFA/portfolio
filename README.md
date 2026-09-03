# Portfolio

A personal profile page. The hero card in the middle is plain HTML and CSS. The two side panels are a pixel-art simulation written in C, compiled to a freestanding WebAssembly module with no imports and no framework, and driven by a small JavaScript shim.

## Layout

```
index.html  styles.css  shim.js   the page and its ~60-line shim
sim.wasm                          the compiled simulation (committed; static hosts serve it)
sim/                              C sources: world, hero, bugs, vignettes, scene, exports
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

Debug query parameters: `?vignette=race|detective|regression` plays a vignette on load; `?motion=reduce` shows the static frame.

## Status

Phase 1 (first screen) is complete. Phase 2, a greenfield settlement stage below the fold, is specified in `docs/superpowers/specs/` and not yet planned.
