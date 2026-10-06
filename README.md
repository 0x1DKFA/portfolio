# Portfolio

A personal profile for a senior backend engineer. The page pairs production incident reports and concise work history with a first-person alley hunt rendered by a small C raycaster compiled to freestanding WebAssembly. The page layer is plain HTML, CSS, and a small JavaScript shim.

## Files

```
index.html  styles.css  shim.js   portfolio content, resume dialog, browser integration
sim.wasm                          committed module served by static hosts
sim/                              C simulation and procedural pixel art
test/                             native test suite and WebAssembly smoke test
```

## Local use

The WebAssembly build requires Zig (`zig cc`) or a compatible Clang with wasm32 support. Node 18+ is needed for the smoke test.

```
make serve
make sim.wasm
make test
make smoke
```

With Clang installed, build with `make WASM_CC=clang sim.wasm`. Debug query parameters: `?seed=N` fixes the simulation seed; `?motion=reduce` shows the static frame.

GitHub Pages deploys the static files through `.github/workflows/pages.yml` on pushes to `master` or manually from the Actions tab.
