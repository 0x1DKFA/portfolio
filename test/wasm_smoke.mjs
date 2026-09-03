import { readFile } from 'node:fs/promises';

const REQUIRED = ['memory', 'sim_init', 'sim_update', 'sim_render', 'sim_framebuffer', 'sim_framebuffer_len',
  'sim_request', 'sim_poll_event', 'sim_render_static', 'sim_render_avatar', 'sim_avatar_buffer', 'sim_avatar_len'];
const MAX_BYTES = 40 * 1024;

const bytes = await readFile(new URL('../sim.wasm', import.meta.url));
const mod = await WebAssembly.compile(bytes);

const imports = WebAssembly.Module.imports(mod);
if (imports.length) throw new Error('module has imports: ' + JSON.stringify(imports));
const names = WebAssembly.Module.exports(mod).map(e => e.name);
for (const n of REQUIRED) if (!names.includes(n)) throw new Error('missing export ' + n);
if (bytes.length > MAX_BYTES) throw new Error(`sim.wasm is ${bytes.length} bytes, budget is ${MAX_BYTES}`);

const { exports: ex } = await WebAssembly.instantiate(mod, {});
if (ex.sim_init(1234, 96, 240, 200) !== 0) throw new Error('sim_init rejected valid sizes');
if (ex.sim_init(1234, 96, 9999, 200) !== -1) throw new Error('sim_init accepted a bad height');
ex.sim_init(1234, 96, 240, 200);
for (let i = 0; i < 60; i++) ex.sim_update(17);
ex.sim_render();

const len = ex.sim_framebuffer_len();
if (len !== 2 * 96 * 240 * 4) throw new Error('bad framebuffer length ' + len);
const px = new Uint8Array(ex.memory.buffer, ex.sim_framebuffer(), len);
let painted = 0;
for (let i = 0; i < len; i += 4) if (!(px[i] === 18 && px[i + 1] === 22 && px[i + 2] === 40) && px[i + 3] === 255) painted++;
if (painted < 500) throw new Error('framebuffer looks empty: ' + painted + ' painted pixels');

ex.sim_request(1);
let started = false;
for (let i = 0; i < 120 && !started; i++) {
  ex.sim_update(17);
  for (let e = ex.sim_poll_event(); e !== 0; e = ex.sim_poll_event()) if ((e >>> 24) === 1 && ((e >>> 8) & 255) === 1) started = true;
}
if (!started) throw new Error('race vignette never started');

ex.sim_render_avatar();
const av = new Uint8Array(ex.memory.buffer, ex.sim_avatar_buffer(), ex.sim_avatar_len());
if (av.length !== 24 * 24 * 4) throw new Error('bad avatar length');

console.log(`ok: ${bytes.length} bytes, ${names.length} exports, ${painted} painted pixels`);
