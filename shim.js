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
const resumeDialog = document.getElementById('resume-dialog');

document.querySelectorAll('[data-resume-open]').forEach((button) =>
  button.addEventListener('click', () => resumeDialog.showModal()));
document.querySelector('[data-resume-close]').addEventListener('click', () => resumeDialog.close());
resumeDialog.addEventListener('click', (event) => {
  if (event.target === resumeDialog) resumeDialog.close();
});

let sim = null, geom = null, off = null, raf = 0, last = 0, squashed = 0, resizeTimer = 0;
let staticRendered = false;

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
  if (Number.isFinite(s) && s >= 0) return s >>> 0;
  return (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0;
}

function measure() {
  const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));
  const bw = Math.max(1, Math.round(innerWidth * dpr)), bh = Math.max(1, Math.round(innerHeight * dpr));
  const scale = Math.max(1, Math.round(bh / LOGICAL_H), Math.ceil(bw / MAX_W));
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
function renderStatic() { sim.sim_render_static(); blit(); drainEvents(); staticRendered = true; }

function applyMode() {
  stopLoop();
  if (!staticMode() && staticRendered) { geom = null; staticRendered = false; }
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
