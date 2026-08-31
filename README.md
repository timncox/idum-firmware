# Alt-DUM Firmware

**Alt-DUM is independent alternate firmware for the Mystic Circuits IDUM
Eurorack module. It is not the official Mystic Circuits firmware.**

Alt-DUM keeps a stock-derived classic runner and pairs it with one independently
selectable alternate engine. The physical CYCLE switch chooses between them at
runtime. Alt-DUM v1.0.0 ships with Superlooper as that alternate engine and is
built from the official IDUM v0.99 Arduino source.

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

`make build` performs the complete v1.0.0 production build:

1. Copy the stock-derived/shared sketch into
   `build/staged/superlooper/idum/`.
2. Add only the selected source from `firmware/engines/superlooper/`.
3. Compile the staged sketch for the Arduino Nano Every / ATmega4809.
4. Export `build/release/alt-dum-superlooper-v1.0.0.hex`.

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
does not vary with N. With tracking disabled, CLOCK OUT remains a plain N-divided
clock. The classic runner otherwise retains the stock clock ordering and timing
algorithms.

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
`1` repeats only the newest complete stage, `4` plays the last four stages
oldest-to-newest, and `8` plays the complete captured window.

When external-sequencer tracking is enabled, Superlooper records sequencer
advances alongside the rendered trigger tape. Playback replays those advances
at their recorded tape positions, scaled by the active loop speed. Mutations
span `L * N` raw clocks, so their transport catch-up is computed in complete
Superlooper stages: for example, `N=2, L=8` advances an external sequencer eight
times over sixteen raw clocks and returns an eight-step sequence to its starting
stage.

Param and Length have separate mutation and loop values. At each LOOP boundary
the newly active values remain latched until that physical control moves beyond
a small noise threshold. Entering LOOP always starts Param at exact 1:1 speed;
moving it gives a smooth quadratic range from 1/8x through 8x, with an
exact-speed center region. On the first LOOP after selecting Superlooper,
Length uses the current physical slider position. Later loop entries recall the
preceding loop span until Length moves. Changes to a moved Length are applied at
a loop-stage boundary. Leaving LOOP restores the prior mutation Param and
Length until they move.

Chance deliberately remains the live v0.99 probability control while recording.
Its decisions are already present in the rendered tape, so Chance is not
reapplied during playback. New loop-density semantics remain deferred.

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
gate. Skip cannot add or remove clocks from Superlooper's dedicated transport;
it retains the stock trigger-channel pass-through behavior.

The Superlooper test battery covers fresh CYCLE transitions, control pickup,
newest-stage anchoring, N division, `L * N` mutation lifetimes, trigger-owned
launch and mode preemption, sub-clock capture, crossed-onset playback, smooth
speed endpoints, tape-head resumption, recorded external transport, Skip
transport isolation, and all trigger-producing mutation modes. It also compares
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
