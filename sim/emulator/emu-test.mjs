// node sim/emulator/emu-test.mjs web/emulator/idum-beatgen.wasm
// Drives the WebAssembly build the way web/emulator.html does and checks the firmware
// behaves: boots, plays beats from a clock alone, VCO mode makes the expected pitch,
// patterns can be swapped in memory, and CYCLE down returns to the stock engine.
import { readFileSync } from 'node:fs';

const file = process.argv[2] || 'web/emulator/idum-beatgen.wasm';
const mod = await WebAssembly.compile(readFileSync(file));
if (WebAssembly.Module.imports(mod).length) throw new Error('the emulator must not import anything');
const { exports: e } = await WebAssembly.instantiate(mod, {});
const mem = () => new Uint8Array(e.memory.buffer);
const SR = 48000;
let failures = 0;
const check = (ok, name, detail) => { console.log(`${ok ? 'PASS' : 'FAIL'} emulator-${name} (${detail})`); if (!ok) failures++; };

// Run for `seconds`, counting rising edges per bit (0-3 outputs, 4 CLOCK OUT, 5 CLOCK IN).
function run(seconds) {
  const counts = [0, 0, 0, 0, 0, 0];
  let prev = 0;
  for (let left = Math.round(seconds * SR); left > 0; left -= 960) {
    const n = Math.min(960, left);
    const p = e.emu_run(n, SR);
    for (const x of mem().subarray(p, p + n)) {
      for (let c = 0; c < 6; c++) if ((x >> c & 1) && !(prev >> c & 1)) counts[c]++;
      prev = x;
    }
  }
  return counts;
}

e.emu_boot();
check(e.emu_micros() > 0, 'boot', `setup() finished at ${(e.emu_micros() / 1e6).toFixed(2)} s`);

// House, length 16, density mid, clock at 120 BPM sixteenths.
e.emu_set_gate(17, 1);
e.emu_set_analog(21, 20); e.emu_set_analog(20, 512); e.emu_set_analog(18, 780); e.emu_set_analog(19, 0);
e.emu_set_clock(125000, 5000);
run(0.5);
let c = run(4);
check(e.emu_leds() & 16, 'cycle-up', 'CYCLE up runs Beatgen');
check(c[5] === 32 && c[0] === 8 && c[4] === 2, 'house-beats',
  `${c[5]} clocks -> OUT1 ${c[0]}, OUT2 ${c[1]}, OUT3 ${c[2]}, OUT4 ${c[3]}, CLOCK OUT ${c[4]}`);

// Swap the House kick row for every step and hear it.
const bank = e.emu_bank() + 9;
const saved = mem().slice(bank, bank + 640);
mem().set(new TextEncoder().encode('X'.repeat(32)), bank);
c = run(2);
check(c[0] === 16, 'pattern-swap', `edited kick row plays ${c[0]} of 16 clocks`);
mem().set(saved, bank);

// VCO: slot 8, unison, PARAM mid.
e.emu_set_analog(21, 1000); e.emu_set_analog(18, 20); e.emu_set_analog(19, 600); e.emu_set_analog(20, 0);
run(0.1);
const hz = e.emu_out4_hz();
c = run(1);
const near = (x) => Math.abs(x - hz) <= Math.max(2, hz * 0.02);
check((e.emu_leds() & 32) && near(c[0]) && near(c[1]) && near(c[2]) && near(c[3]), 'vco-pitch',
  `root ${hz} Hz; cycles in 1 s: ${c.slice(0, 4).join(', ')}`);

// Fifths chord: OUT1-3 near 1.5x root.
e.emu_set_analog(18, 300);
run(0.1);
c = run(1);
const fifth = hz * 1.5;
check([c[0], c[1], c[2]].some((x) => Math.abs(x - fifth) < fifth * 0.03), 'vco-chord', `OUT1-3 ${c.slice(0, 3).join(', ')} Hz vs fifth ${fifth.toFixed(0)} Hz`);

// CYCLE down: stock engine, VCO off, outputs follow the stock modes again.
e.emu_set_gate(17, 0);
e.emu_set_analog(18, 520);
run(0.5);
c = run(2);
check(!(e.emu_leds() & 16) && !(e.emu_leds() & 32) && c[4] >= 15, 'cycle-down',
  `stock engine, VCO off, CLOCK OUT ${c[4]} pulses for ${c[5]} clocks`);

const t0 = performance.now();
run(5);
const ms = (performance.now() - t0) / 5;
check(ms < 500, 'speed', `${ms.toFixed(0)} ms of CPU per emulated second`);

if (failures) { console.error(`${failures} emulator check(s) failed`); process.exit(1); }
