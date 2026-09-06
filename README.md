# Alt-DUM Firmware

**Alt-DUM is independent alternate firmware for the Mystic Circuits IDUM
Eurorack module. It is not the official Mystic Circuits firmware.**

Alt-DUM keeps a stock-derived classic runner and pairs it with one independently
selectable alternate engine. The physical CYCLE switch chooses between them at
runtime. Superlooper is the alternate engine; the shared firmware is based on
the official IDUM v0.99 Arduino source.

The current test build is **v1.0.8-chance-meter-noise-test**, with quantized and
bidirectional loop speed, reverse trigger timing, separate loop/mutation control
memories, and a cubic Chance curve with the stock-sized bottom dead zone and a
temporary probability meter. The meter opens after movement of more than 12
ADC counts, so small fluctuations around the floor do not continually restart
its timeout. See the [Chance meter test notes](docs/CHANCE_METER_TEST_BUILD.md)
for validation and hardware checks.

The repository makes the firmware hackable without the Arduino GUI: it provides
separated engine source, repeatable command-line builds, a host simulator, and
regression tests.

## Layout

- `firmware/idum/` - stock-derived/shared IDUM sketch and hardware layer
- `firmware/engines/superlooper/` - Superlooper's independent engine source
- `sim/` - host-side simulator for exercising firmware behavior without flashing the module
- `scripts/` - convenience wrappers for command-line setup, build, and upload
- `docs/` - architecture notes and build/install details

Arduino requires every `.ino` tab in one sketch directory. The source tree does
not mix engines to satisfy that constraint: builds generate an ignored staged
sketch under `build/staged/<engine>/idum/` by copying the stock/shared tabs and
then only the selected engine. Generated staging files are never edited by hand.

## Quick Start

Install `arduino-cli`, then from this repo:

```sh
make doctor
make setup
make build
make board-list
make upload PORT=/dev/cu.usbmodem101
```

Arduino CLI state is kept under the ignored `.arduino/` directory via `arduino-cli.yaml`, so a user does not need the Arduino GUI or pre-existing IDE preferences.

The default board target is:

```sh
arduino:megaavr:nona4809:mode=off
```

That matches the observed hardware and upstream build notes: an Arduino Nano Every daughterboard with an ATmega4809 MCU and register emulation set to `None`.

Override the board target like this:

```sh
make build FQBN=arduino:avr:nano
```

`make build` compiles the current source using the default v1.0.0 output name:

1. Copy the stock-derived/shared sketch into
   `build/staged/superlooper/idum/`.
2. Add only the selected source from `firmware/engines/superlooper/`.
3. Compile the staged sketch for the Arduino Nano Every / ATmega4809.
4. Export `build/release/alt-dum-superlooper-v1.0.0.hex`.

To build and upload the current test version with its own output directory:

```sh
make test-superlooper
make build FIRMWARE_VERSION=1.0.8-chance-meter-noise-test BUILD_PATH=build/idum-superlooper-chance-meter-noise-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-chance-meter-noise-test PORT=<detected-port>
```

Use the Nano Every port reported by `make board-list`. Output version names are
labels, not source checkouts: rebuilding an older filename from current source
does not reproduce the older firmware.

The explicit Superlooper target is:

```sh
make superlooper-build
```

The release basename identifies both the Alt-DUM family and its included engine.
To produce a differently versioned development build without renaming the
Arduino sketch:

```sh
make build FIRMWARE_VERSION=1.0.1-dev
```

The shell wrapper accepts the same setting:

```sh
FIRMWARE_VERSION=1.0.1-dev scripts/build.sh
```

Those examples produce `build/release/alt-dum-superlooper-v1.0.1-dev.hex`;
without an override, the default is
`build/release/alt-dum-superlooper-v1.0.0.hex`. A future engine build can
override `FIRMWARE_BASENAME` with its corresponding `alt-dum-<engine>` name.
The boot animation identifies the Alt-DUM firmware family; the release filename
is the authoritative exact version.

Every production build chooses one alternate engine; the finished firmware's
CYCLE switch always selects between the classic runner (down) and that alternate
(up). A no-alternate personality exists only in the host simulator as a classic
behavior oracle.

Generated repeats normally stop after the external clock disappears. To
deliberately restore v0.99's unbounded behavior for a custom build:

```sh
make build FIRMWARE_DEFINES=-DIDUM_ENABLE_CLOCK_STOP_TIMEOUT=0
```

### Ball loop-speed diagnostic

The optional Ball timing image measures how many SuperLooper engine iterations
fit in one real CLOCK interval while TR1 Ball is active. It does not change the
production image or print during the measured interval. The first sample after
boot is saved to EEPROM so it survives the USB-disconnect test workflow.

```sh
make ball-timing-diagnostic-test
make ball-timing-diagnostic-build
make ball-timing-diagnostic-upload PORT=/dev/cu.usbmodem101
```

After flashing, disconnect USB, run a TR1 Ball mutation for at least one full
clock interval, then reconnect USB. At 115200 baud the saved line repeats once
per second until the next clock arrives. It reports engine loops, clock period,
PARAM, sampled and live L, direction, and the opening divider.

### Trigger-owned SuperLooper mutations

SuperLooper uses clock-opened Chance windows and trigger-owned mutation
lifetimes. Off-grid triggers inside an open window receive a complete L stage
span (`L * N` raw clocks). Same-mode triggers do not renew that lifetime, while
an eligible trigger with a newly selected Mode intentionally preempts it. The
full state contract is in
[`docs/SUPERLOOPER_MUTATION_SCHEDULER.md`](docs/SUPERLOOPER_MUTATION_SCHEDULER.md).

## Host Simulator

The `sim/` directory contains a small desktop harness that compiles the firmware as ordinary C++ and provides fake Arduino functions for pins, analog reads, EEPROM, time, and random numbers. It is not an ATmega4809 emulator, but it lets us exercise the firmware's normal `setup()` and `loop()` paths and inspect output pin behavior before flashing hardware.

The simulator uses one firmware compatibility shim: when built with `IDUM_SIM`, `digitalReadFast()` and `digitalWriteFast()` route to the simulator's fake `digitalRead()` and `digitalWrite()` functions. Normal Arduino builds keep the original fast AVR I/O path.

Run a smoke test:

```sh
make sim
```

Run characteristic scenarios for all documented modes:

```sh
make sim-modes
```

Run the engine-specific verification batteries:

```sh
make test-stock-reference
make test-superlooper
make test-all
```

`test-stock-reference` exercises the simulator-only classic reference.
`test-superlooper` first runs the classic path in both simulator personalities and compares an exact,
deterministic output/state fingerprint; it then runs the independent
Superlooper battery with CYCLE up. This makes stock-path drift a test failure
while allowing Superlooper's tests to change with its intentional semantics.

Inspect Break mode's embedded pattern table, or recreate those patterns through simulated clock and trigger I/O:

```sh
make sim-break-patterns
make sim-break-playback
```

The simulator is useful for regression tests around timing-sensitive behavior, CV scaling, mode selection, clock-derived events, loop playback, and output inversion. It does not replace flashing the module for final validation, but it gives us a much faster place to iterate on behavior.

## ADC Diagnostic Build

To measure the physical analog range of the front-panel controls and CV inputs, build and flash the temporary ADC diagnostic firmware:

```sh
make adc-diagnostic-build
make adc-diagnostic-upload PORT=/dev/cu.usbmodem101
arduino-cli --config-file arduino-cli.yaml monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```

The diagnostic prints `mode`, `chance`, `length`, and `param` values over USB about ten times per second, including the minimum and maximum values seen since boot. Move each control through its full travel to see the actual hardware range.

This diagnostic firmware does not run IDUM's musical behavior. Flash the normal firmware again after measuring:

```sh
make build
make upload PORT=/dev/cu.usbmodem101
```

## Current Firmware Flow

At a high level:

1. `setup()` assigns pins, samples the boot controls, initializes EEPROM, sets interval bookkeeping, attaches trigger interrupts, and displays the startup animation/version.
2. `c00_AlternateEngine.ino` supplies the production `loop()` dispatcher: CYCLE down runs the classic pipeline and CYCLE up runs the selected alternate.
3. The classic input adapter saves raw CYCLE for dispatch, then places saved menu setting 0 into the legacy `cycleIn` variable before stock looper and mutation functions run.
4. Mode files `b10_...` through `b80_...` implement the eight mutation modes.
5. `a70_handleDisplay.ino` multiplexes the mode LEDs.
6. `x_utils.ino` handles EEPROM bit packing for options and enabled modes.

## Alternate Engine Framework

The stock-derived mode, looper, mutation, and output functions remain free of
alternate-engine conditionals. The production dispatcher uses a small copied
top-level classic pipeline so it can adapt the repurposed CYCLE control before
calling those functions. A selected alternate lives under
`firmware/engines/<engine>/`, gets its own top-level runner, and may copy and
change more of the state machine as needed while sharing hardware I/O, display,
EEPROM, utilities, and mutation renderers.

The interface is intentionally tiny:

- `runSelectedAlternateEngine()` executes the selected alternate runner.
- `resetSelectedAlternateEngine()` starts it fresh at a CYCLE boundary.
- `IDUM_ALT_ENGINE` selects which alternate is linked into the firmware.

Changing CYCLE is defined as a conceptual reboot of the runner. It is not
quantized and makes no promise to preserve loop state, modification state, or
sequencer phase. Musical entry/exit semantics belong to LOOP while an alternate
runner is already active.

### Shared external-sequencer contract

CYCLE is never used directly by either sequencing engine. In the classic
runner, setup-menu position 0 enables or disables the legacy external-sequencer
cycle/position maintenance. Superlooper always owns its N-divided CLOCK OUT;
position 0 enables its recorded external-transport lane, loop-position jumps,
and exit catch-up. Jump and catch-up bursts use a fixed fast pulse cadence that
does not vary with N. With tracking disabled, Superlooper emits plain N-divided
clocking outside LOOP and clock pulses at loop-stage boundaries during playback;
it does not restore the external sequencer's absolute position. The classic
runner otherwise retains its existing clock ordering and timing algorithms.

### Superlooper build

`make superlooper-build` supplies an independent rendered-output looper while
leaving the stock runner unchanged. Superlooper taps the final four trigger
states after mutation and choke resolution into a rolling tape with 32 cells
per raw clock. LOOP bypasses mutation rendering and plays that tape directly,
so the loop repeats what the module actually emitted. LOOP entry and exit are
committed on the next raw clock edge, keeping the tape and external sequencer
transport aligned.

One sequencer stage contains `N` raw clocks. Six contiguous regions of the
calibrated Length slider select `N = 1, 2, 3, 4, 6, 8` while the setup menu is
open; the selected ring LED is shown for one second and the value is saved in EEPROM.
Outside LOOP, CLOCK OUT advances once per N raw clocks. Superlooper always keeps
the preceding eight complete N-clock stages. Length chooses the backward span:
`1` repeats only the newest complete stage, `4` selects the last four stages,
and `8` selects the complete captured window. Forward playback reads the
selected span oldest-to-newest; reverse entry and ordering are described below.

When external-sequencer tracking is enabled, Superlooper records sequencer
advances alongside the rendered trigger tape. Playback replays those advances
at their recorded tape positions, scaled by the active loop speed. Mutations
span `L * N` raw clocks, so their transport catch-up is computed in complete
Superlooper stages: for example, `N=2, L=8` advances an external sequencer eight
times over sixteen raw clocks and returns an eight-step sequence to its starting
stage.

Param and Length have separate mutation and loop memories. You can adjust both
for mutations outside LOOP, then re-enter LOOP with its previous speed,
direction/pause, and length intact. Each physical control takes over its newly
active value only after moving beyond a small noise threshold. Leaving LOOP
similarly restores the previous mutation values.

With CYCLE up, hold MODE to enter setup. Move PARAM into one of four quarters to
select the playback style, then hold MODE again to save and exit:

| PARAM position, left to right | Playback style | Ring LED |
| --- | --- | --- |
| First quarter | Smooth forward | 1 |
| Second quarter | Quantized forward | 3 |
| Third quarter | Smooth bidirectional | 5 |
| Fourth quarter | Quantized bidirectional | 7 |

Entering setup alone does not change the selection; PARAM must move. Length
continues to edit N, and the existing mode-dial menu options remain available.
**Playback style and N survive reboot** after saving on setup exit. The loop and
mutation performance memories last only until reboot or a CYCLE engine switch.
The first LOOP then starts at +1x and takes its length from the physical slider.
Changing playback style in setup also resets loop speed to +1x until PARAM moves
in LOOP. The default style is smooth forward, from 1/8x through 8x with a broad
exact-speed center.

Quantized forward speeds are 1/8x, 1/4x, 1/2x, 1x, 2x, 4x, and 8x, with
hysteresis between adjacent settings. Bidirectional PARAM runs from reverse
8x at the left through a center pause to forward 8x at the right. Its quantized
version uses the same seven magnitudes in each direction. Reverse reflects the
recorded trigger onsets and transport events across the whole phrase, including
rhythms within a stage. It emits normal trigger pulses with recorded widths
scaled by speed. Overlapping reversed tails are separated by one output-service
pass so the next attack remains audible.

Reverse entry is anchored to a stage attack. On a divided stage boundary it
starts one stage before the most recently completed stage; between divided
boundaries it uses the most recent complete stage. L=1 repeats its sole captured
stage. Thus an ordinary beat stays on the beat while a recorded accelerating
bounce plays with decelerating hit spacing. Length changes restart at the newest
captured stage's attack. Pause freezes the playhead and silences trigger outputs.
Speed and direction changes preserve the tape position; direction is retained
through pause. Control recall and the first-entry 1:1 default remain unchanged.

CLOCK IN continues advancing the independent eight-step reference and N-divider
phase during forward playback, reverse playback, and pause. With external
tracking enabled, LOOP exit discards unplayed playback clocks and catches up
from the emitted sequencer position to that reference. The next normal advance
remains on the original N-divided clock grid. Reverse transport uses forward
clock bursts to reach preceding steps on an eight-step external sequencer.

Moved Length changes take effect at a loop-stage boundary; PARAM speed changes
take effect immediately.

Chance remains live while recording and follows a cubic curve. Readings 0-15
(about 1.5% of the ADC range, matching stock v0.99) block new mutations. Above
15, the curve rises gradually toward guaranteed eligibility at 1000-1024. Approximate base probabilities at
quarter, half, and three-quarter input range are 1.45%, 12.8%, and 44.5%. The curve
uses 16-bit probability resolution to retain very small probabilities near the
bottom. The existing length penalty still reduces eligibility below the top.

A CV gate that drives the combined Chance reading to 1000 or above forces the
next clock's eligibility window open, even with an active length penalty.
Dropping to 15 or below closes open windows immediately. Existing mutations
finish their sampled duration. Firmware sees one combined slider/CV reading,
so the floor is not an independent physical-fader mute. These probabilities
apply per raw clock: faster clocks and longer mutations can sound denser.

Chance decisions are already present in the recorded tape and are not reapplied
during LOOP playback.

Changing Chance temporarily turns the mode ring into a probability meter:
leftmost LED (position 0) means off, and positions 1-7 divide the curved base
probability into seven equal bands. While the meter is visible, position 1
appears immediately above the floor. This shows probability before the length
penalty, not a mutation firing.
The normal display returns one second after the last input movement exceeding
12 ADC counts. While visible, the meter follows the current reading, including
one-count crossings of the floor; these crossings do not restart the timer.
Both slider and CV drive the meter, so substantial ongoing CV movement can keep
it visible. Small changes accumulate against the last movement reference, so a
slow sweep still opens it. Setup and LOOP position take priority. See the
[Chance meter test notes](docs/CHANCE_METER_TEST_BUILD.md) for the mapping and
patched-gate test procedure.

Each raw CLOCK IN edge evaluates Chance and opens or closes an eligibility
window until the next clock edge. A trigger inside an open window begins its
selected mode immediately for a sampled `L` stages (`L * N` raw clocks), even
when the trigger is off-grid. A same-mode trigger during that mutation does not
restart its lifetime; an eligible trigger after a Mode change preempts it with a
fresh L-stage mutation. Mode changes without triggers do nothing. PARAM remains
live, and Ball additionally allows live L movement to bend the active curve
without changing its sampled endpoint.

SuperLooper Ball uses a time-domain curve while the classic Ball implementation
remains untouched. Center PARAM is dry pass-through. At either side, the curve
moves continuously between the most recent plausible trigger period and a fast
14 ms period that preserves a real low gap instead of collapsing into a solid
gate. With tracking enabled, Skip modifies external clock transport and restores
its N-divided phase using an eight-stage (`8*N` raw-clock) reference. With tracking
disabled, external clocking stays on the plain N division. Skip retains the
trigger-channel pass-through behavior.

The Superlooper test battery covers fresh CYCLE transitions, control pickup,
newest-stage anchoring, N division, `L * N` mutation lifetimes, trigger-owned
launch and mode preemption, sub-clock capture, crossed-onset playback, smooth
and quantized speed mappings, menu persistence, speed/direction/Length recall,
reverse onset timing, tape-head resumption, recorded external transport, Skip
catch-up, the Chance dead zone, clock loss/restart, and all trigger-producing
mutation modes. The playback tests include 144 reverse-entry cases and 144
exit/catch-up cases across N-divider phases. They compare actual simulated
output edges for a steady beat, accelerating bounce example, unequal gate widths,
and off-grid triggers against independently calculated reversed hit times.
The suite also compares
an exact deterministic fingerprint of the classic runner so Superlooper changes
cannot silently alter the stock timing path.

## Boot Controls

### Identifying Alt-DUM

After any boot-control confirmation, Alt-DUM v1.0.0 shows this mode-LED
signature:

1. The two outer LEDs appear together for 250 ms.
2. The pair steps inward three times, ending on the two center LEDs.
3. The animation lands on the single far-left LED for 250 ms.

The full signature lasts about 1.25 seconds. Stock IDUM v0.99 instead holds a
single orange mode LED, so the two firmware families are visually distinct at
power-on.

The mode LEDs share a three-bit hardware multiplexer, so only one ring LED is
electrically selected at any instant. Firmware rapidly alternates between the
two LEDs in each frame, making the pair appear continuously lit.

Two power-on gestures keep user settings and the physical mode-ring wiring
separate:

- Hold **MODE/REMOVE** while powering on to restore the normal firmware options,
  enabled modes, and Param/Length resolutions. The stored LED order is preserved.
- Hold **LOOP** while powering on to toggle between the hardware v1.3+ mode-ring
  order and the legacy order. The selection is stored in EEPROM byte 5.

Fresh EEPROM is initialized for hardware v1.3 and later. Installing this build
over the previous `Q` EEPROM format performs a one-time settings reset and writes
the new `R` format. The blinking LOOP LED confirms first-time initialization;
forward and reverse ring sweeps confirm settings reset and LED-order toggle.

See [docs/HARDWARE.md](docs/HARDWARE.md) for hardware notes, [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for a file map, and [docs/CODE_WALKTHROUGH.md](docs/CODE_WALKTHROUGH.md) for a source-derived explanation of how v0.99 works.

## Source

Canonical upstream source: [mysticcircuits/IDUM](https://github.com/mysticcircuits/IDUM).

This repo started from the v.99 firmware archive at `IDUM-github-official/firmwares/IDUM_Firmware_v_99.zip`.

Original author: Eli Pechman / Mystic Circuits.

Original license statement: Creative Commons Attribution-ShareAlike 4.0.
