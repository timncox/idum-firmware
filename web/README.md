# Beatgen web programmer

`beatgen.html` edits the five Beatgen drum patterns inside a built firmware
`.hex`, plays them back in the browser, and flashes the result to the module
over USB.

```sh
make beatgen-build          # writes build/release/alt-dum-beatgen-v1.0.0.hex
cd web
python3 -m http.server 8000
```

Open <http://localhost:8000/beatgen.html> in desktop Chrome, Edge or Brave
(Web Serial needs a Chromium browser and a localhost or https page), load the
`.hex`, edit, then **Flash to module**. **Download edited .hex** saves a copy
you can flash from the terminal instead.

Your edits are kept in the browser between visits, and **Save patterns as a
file** exports them as JSON.

## Emulator

`emulator.html` plays the Beatgen build in the browser so you can try it before
flashing. Serve the folder the same way and open
<http://localhost:8000/emulator.html> (any modern browser; no module needed):

```sh
cd web
python3 -m http.server 8000
```

Press **Power on**, then turn the knobs (drag up and down, scroll, or use the
arrow keys; double-click resets), flip CYCLE, and hold the TR and LOOP GATE
pads. The clock runs at 120 BPM in sixteenths until you change or stop it.
**Use my programmer edits** plays the patterns you saved in `beatgen.html`.

It runs `emulator/idum-beatgen.wasm`, the firmware's own C++ compiled with the
simulator's hardware stubs (`sim/emulator/emu.cpp`). It is committed, so you
only rebuild it after changing firmware: `make emulator` (needs a clang with
the WebAssembly target; on a Mac, `brew install llvm lld`, then
`make emulator EMU_CXX=$(brew --prefix llvm)/bin/clang++`). `make test-emulator`
warns if the `.wasm` is older than the sources.

What it models rather than emulates: the AVR timers and interrupts behind VCO
mode, the analog front end, and the output stage. The sound is a stand-in too:
drum voices for trigger outputs, ideal squares for VCO mode.

## Files

- `beatgen.html`: the page.
- `beatgen-bank.js`: finds the `IDUMBEAT1` pattern bank in a firmware image,
  reads and rewrites it, and re-encodes Intel HEX. Only the 640 pattern bytes
  change.
- `webserial-jtag2updi.js`: the browser flasher from
  [ID00M](https://00modular.com/id00m/resources/) by 00 Modular (`flasher/webserial-jtag2updi.js`),
  unchanged apart from a credit header. It speaks avrdude's `jtag2updi`
  protocol to the Nano Every's onboard programmer.
- `emulator.html`, `emulator/idum-worklet.js`, `emulator/idum-beatgen.wasm`:
  the emulator page, the AudioWorklet that runs the firmware one audio block at
  a time, and the firmware build it runs.
- `test/bank-test.mjs`: `node web/test/bank-test.mjs <beatgen .hex>` checks a
  no-edit round trip reproduces the image byte for byte and that an edit only
  touches the pattern bank.

## Credits and license

CC BY-SA 4.0. Beatgen is part of Alt-DUM, alternate firmware based on IDUM
v0.99 by Eli Pechman / Mystic Circuits. The flasher module is from ID00M by
00 Modular. Not affiliated with or endorsed by Mystic Circuits.
