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
  cards.forEach(c => c.classList.remove('is-playing'));   // sim_init resets the scene; no END event follows
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
