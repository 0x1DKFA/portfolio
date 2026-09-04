import { readFile } from 'node:fs/promises';

const REQUIRED = ['memory', 'sim_init', 'sim_update', 'sim_render', 'sim_framebuffer', 'sim_framebuffer_len',
  'sim_poll_event', 'sim_render_static'];
const MAX_BYTES = 64 * 1024;

const bytes = await readFile(new URL('../sim.wasm', import.meta.url));
const mod = await WebAssembly.compile(bytes);
const imports = WebAssembly.Module.imports(mod);
if (imports.length) throw new Error('module has imports: ' + JSON.stringify(imports));
const names = WebAssembly.Module.exports(mod).map(e => e.name);
for (const n of REQUIRED) if (!names.includes(n)) throw new Error('missing export ' + n);
if (names.length !== REQUIRED.length) throw new Error('unexpected exports: ' + names.filter(n => !REQUIRED.includes(n)).join(', '));
if (bytes.length > MAX_BYTES) throw new Error(`sim.wasm is ${bytes.length} bytes, budget is ${MAX_BYTES}`);

const { exports: ex } = await WebAssembly.instantiate(mod, {});
if (ex.sim_init(1234, 356, 200) !== 0) throw new Error('sim_init rejected valid sizes');
if (ex.sim_init(1234, 356, 900) !== -1) throw new Error('sim_init accepted a bad height');
ex.sim_init(1234, 356, 200);
for (let i = 0; i < 60; i++) ex.sim_update(17);
ex.sim_render();
const len = ex.sim_framebuffer_len();
if (len !== 356 * 200 * 4) throw new Error('bad framebuffer length ' + len);
const px = new Uint8Array(ex.memory.buffer, ex.sim_framebuffer(), len);
let painted = 0;
for (let i = 0; i < len; i += 4) if ((px[i] | px[i + 1] | px[i + 2]) && px[i + 3] === 255) painted++;
if (painted < 356 * 200 * 0.9) throw new Error('framebuffer looks empty: ' + painted + ' painted pixels');

let squashes = 0, ms = 0;
while (!squashes && ms < 120000) {
  ex.sim_update(17); ms += 17;
  for (let e = ex.sim_poll_event(); e !== 0; e = ex.sim_poll_event()) if ((e >>> 24) === 3) squashes++;
}
if (!squashes) throw new Error('no squash within 120 s of simulated time');

ex.sim_render_static();
console.log(`ok: ${bytes.length} bytes, ${names.length} exports, ${painted} painted pixels, first squash at ${(ms / 1000).toFixed(1)} s`);
