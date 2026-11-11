# Beatgen

Beatgen is an Alt-DUM alternate engine that turns IDUM into a drum pattern
generator. Patch a clock into CLOCK IN and nothing else, flip CYCLE up, and
the four trigger outputs play a beat. Flip CYCLE down for the stock IDUM
engine; nothing about the stock engine changes in a Beatgen build (the
simulator's stock-parity fingerprint matches the reference build).

Build it with `make beatgen-build` (or `make build ALT_ENGINE=beatgen`).

## Controls

Every CLOCK IN rising edge plays one step. The MODE ring picks one of eight
slots:

| LED | Slot | What PARAM does |
| --- | --- | --- |
| 1 | House | Variation: ghost notes, fills, hat ratchets |
| 2 | Techno | Variation |
| 3 | Breaks | Variation |
| 4 | Hip-hop | Variation |
| 5 | Euclid | Rotation of OUT2-OUT4 against OUT1 |
| 6 | Generative | Chance the pattern is re-rolled each loop (CCW = keep it) |
| 7 | User | Variation (the pattern meant for the web programmer) |
| 8 | VCO | Pitch (see below) |

- **PROB**: density. Fully CCW plays only each pattern's core (`X`) hits;
  turning up adds `3`, then `2`, then `1` steps. In Euclid it sets how many
  hits each output spreads over the loop.
- **LENGTH**: loop length, 4, 6, 8, 10, 12, 16, 24 or 32 steps. Patterns are
  32 steps long; shorter lengths loop the start of the pattern, which makes
  odd meters out of the stock grooves.
- **PARAM**: per the table. Above about two thirds, Variation also ratchets
  OUT3 now and then. Fills land in the last quarter of the loop.
- **LOOP button**: locks the current variation so every loop repeats exactly;
  press again to unlock. A high LOOP GATE locks while held. The LOOP LED
  shows the lock.
- **MODE button**: restarts the pattern on the next clock.
- **CLOCK OUT**: a pulse at the top of every loop, for resetting other
  sequencers.

Trigger inputs are optional:

| Input | Effect |
| --- | --- |
| TR1 | Reset: the next clock plays step 1 |
| TR2 | Fill while high: OUT2 rolls with doubled hits |
| TR3 | New variation now (works while locked too) |
| TR4 | Breakdown while high: only OUT1 plays |

The ring lights the slot, with a brief flash of the step position on each
clock.

Trigger width is 10 ms, or a quarter of the clock period at fast clocks.

## VCO mode (LED 8)

The four outputs become square-wave oscillators: "speed a gate up enough and
it's a square wave."

- **PARAM** (knob plus CV) sets the root pitch on OUT4, about 16 Hz fully CCW
  to 4 kHz fully CW, exponential. It is not calibrated to 1 V/oct.
- **LENGTH** picks a chord for OUT1-OUT3 above the root: unison, octaves,
  fifths, major, minor, minor 7, major 7, fourths.
- **PROB** detunes OUT1-OUT3 up to about half a semitone, for a thicker
  unison.

OUT4 is a hardware square from timer TCA0 (its pin, PB2, is TCA0's WO2), so it
is clean at any pitch. OUT1-OUT3 sit on pins with no usable timer output, so a
31.25 kHz interrupt on TCB2 runs three phase accumulators; they get rougher
(audibly jittery) toward the top of the range. Leaving slot 8, or flipping
CYCLE down, stops both timers and puts TCA0 and TCB2 back the way the Arduino
core set them up.

The gate output stage was designed for triggers, so treat the audio as
lo-fi; check levels before sending it to a mixer.

## Editing patterns

The House, Techno, Breaks, Hip-hop and User patterns are stored in flash as
text after the marker `IDUMBEAT1`, four 32-character rows each (OUT1-OUT4).
Each character is a step: `.` rest, then `1`, `2`, `3`, `X` from least to
most likely to play.

Edit them either in
`firmware/engines/beatgen/c10_BeatgenEngine.ino` and rebuild, or with the web
programmer in [`web/`](../web/README.md), which reads the patterns out of a
built `.hex`, lets you edit and audition them, and flashes the result.

## Try it before flashing

`web/emulator.html` runs this firmware in the browser: knobs, CYCLE, LOOP and
MODE buttons, TR1-TR4 and LOOP GATE pads, an internal clock (or tap it), the
ring and LOOP LEDs, and the four outputs as sound (drum voices for triggers,
the raw squares in VCO mode) with a two-second scope. It can play your edits
from the web programmer or the patterns in a `.hex`. See
[`web/README.md`](../web/README.md#emulator) for how to open it.

It runs the firmware's C++ (every engine, the stock modes too), compiled to
WebAssembly from the same sources as the `.hex`. It is not an AVR emulator: the
timer and interrupt setup for VCO mode, the analog inputs and the output stage
are modelled, so a hardware test still has the last word.

## Tests

```sh
make test-beatgen
```

That checks the stock engine is unchanged in this build, then runs the
Beatgen scenarios in the simulator: clock-only beats, density, every loop
length, reset, the LOOP lock, fills, breakdown, Euclid counts, generative
re-rolls, VCO pitch and chords (including the software oscillator maths),
and that leaving VCO mode stops the oscillators. `make test-all` now runs it
too. `make test-emulator` checks the browser emulator's build plays beats,
VCO pitches and patterns and returns to stock with CYCLE down.

Not covered by the simulator: real timer registers, the interrupt's CPU load
(about 16% at 31.25 kHz), how the output stage handles audio, and the ring
display. Those need a module on the bench.
