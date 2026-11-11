// node sim/emulator/worklet-test.mjs
// Runs web/emulator/idum-worklet.js outside a browser (with a stand-in for the AudioWorklet
// globals) to check it boots the firmware, answers the page's messages and makes sound.
import { readFileSync } from 'node:fs';

const root = new URL('../../web/emulator/', import.meta.url);
let Processor = null;
globalThis.sampleRate = 48000;
globalThis.AudioWorkletProcessor = class { constructor() { this.port = { posted: [], postMessage(m) { this.posted.push(m); } }; } };
globalThis.registerProcessor = (name, cls) => { Processor = cls; };
await import(new URL('idum-worklet.js', root));

const module = await WebAssembly.compile(readFileSync(new URL('idum-beatgen.wasm', root)));
const p = new Processor({ processorOptions: { module } });
const send = (m) => p.port.onmessage({ data: m });
let failures = 0;
const check = (ok, name, detail) => { console.log(`${ok ? 'PASS' : 'FAIL'} worklet-${name} (${detail})`); if (!ok) failures++; };

check(p.port.posted[0]?.type === 'ready' && p.port.posted[0].bank.length === 640, 'ready', 'posts the built-in 640-byte bank');

function render(seconds) {
  const out = [new Float32Array(128), new Float32Array(128)];
  let peak = 0, masks = 0;
  for (let i = 0; i < Math.round(seconds * sampleRate / 128); i++) {
    p.process([], [out]);
    for (const s of out[0]) peak = Math.max(peak, Math.abs(s));
  }
  for (const m of p.port.posted) if (m.type === 'state') for (const x of m.masks) masks |= x;
  p.port.posted.length = 0;
  return { peak, masks };
}

send({ type: 'gate', pin: 17, high: true });
for (const [pin, value] of [[21, 20], [20, 600], [18, 780], [19, 0]]) send({ type: 'analog', pin, value });
send({ type: 'sound', mode: 'auto', mute: [false, false, false, false], volume: 0.5 });
check(render(0.5).peak === 0, 'silent-without-clock', 'no clock, no sound');

send({ type: 'clock', period: 125000, width: 5000 });
let r = render(2);
check(r.peak > 0.05 && (r.masks & 0x0f) === 0x0f, 'drums', `peak ${r.peak.toFixed(2)}, outputs seen ${(r.masks & 15).toString(2)}`);

send({ type: 'sound', mode: 'auto', mute: [true, true, true, true], volume: 0.5 });
render(0.7);  // let the last hits ring out
check(render(1).peak < 0.01, 'mute', 'all outputs muted');

send({ type: 'sound', mode: 'auto', mute: [false, false, false, false], volume: 0.5 });
send({ type: 'analog', pin: 21, value: 1000 });
send({ type: 'analog', pin: 19, value: 600 });
render(0.2);
r = render(0.5);
check(r.peak > 0.02, 'vco-raw', `VCO slot heard as raw squares, peak ${r.peak.toFixed(2)}`);

send({ type: 'bank', bytes: new Uint8Array(640).fill('.'.charCodeAt(0)) });
send({ type: 'analog', pin: 21, value: 20 });
render(0.3);
r = render(2);
check((r.masks & 0x0f) === 0, 'bank', `a blank bank silences House (outputs ${(r.masks & 15).toString(2)})`);

if (failures) { console.error(`${failures} worklet check(s) failed`); process.exit(1); }
