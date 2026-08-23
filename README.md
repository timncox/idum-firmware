# Mystic Circuits IDUM Firmware

This is a clean local repo for the Mystic Circuits IDUM Eurorack module firmware, starting from the official v.99 Arduino sketch.

The goal is to make the firmware hackable without requiring the Arduino GUI editor. The source still builds as an Arduino sketch for now, but the repo has a normal layout, repeatable CLI commands, and a place to document the system as we overhaul it.

## Layout

- `firmware/idum/` - imported v.99 firmware source, with the main sketch normalized to `idum.ino`
- `sim/` - host-side simulator for exercising firmware behavior without flashing the module
- `scripts/` - convenience wrappers for command-line setup, build, and upload
- `docs/` - architecture notes and build/install details

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

`make build` produces the production selector firmware with Superlooper as its
compile-time-selected alternate. The explicit equivalent is:

```sh
make superlooper-build
```

Every production build chooses one alternate engine; the finished firmware's
CYCLE switch always selects between the classic runner (down) and that alternate
(up). A no-alternate personality exists only in the host simulator as a classic
behavior oracle.

Generated repeats normally stop after the external clock disappears. To
deliberately restore v0.99's unbounded behavior for a custom build:

```sh
make build FIRMWARE_DEFINES=-DIDUM_ENABLE_CLOCK_STOP_TIMEOUT=0
```

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

The stock mode, looper, mutation, and output functions remain free of alternate
engine conditionals. The production dispatcher uses a small copied top-level
classic pipeline so it can adapt the repurposed CYCLE control before calling
those functions. A selected alternate gets its own top-level runner and may copy
and change more of the state machine as needed while sharing hardware I/O,
display, EEPROM, utilities, and mutation renderers.

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
position 0 controls only its loop-position jumps and exit catch-up. Catch-up
pulses use the fast stock path by default. The classic runner otherwise retains
the stock clock ordering and timing algorithms.

### Superlooper build

`make superlooper-build` supplies an independent rendered-output looper while
leaving the stock runner unchanged. Raw CLOCK IN still drives the v0.99
mutation engine at its original rate. Superlooper taps the final four trigger
states after mutation and choke resolution into a rolling tape with 32
cells per raw clock. LOOP bypasses mutation rendering and plays that tape
directly, so the loop repeats what the module actually emitted.

One sequencer stage contains `N` raw clocks. Six contiguous regions of the
calibrated Length slider select `N = 1, 2, 3, 4, 6, 8` while the setup menu is
open; the selected ring LED is shown for one second and the value is saved in EEPROM.
Outside LOOP, CLOCK OUT advances once per N raw clocks. Superlooper always keeps
the preceding eight complete N-clock stages. Length chooses the backward span:
`1` repeats only the newest complete stage, `4` plays the last four stages
oldest-to-newest, and `8` plays the complete captured window.

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

Skip cannot add or remove clocks from Superlooper's dedicated transport; it
still follows the stock trigger-channel pass-through behavior. The other stock
mutation modes run at the raw CLOCK IN rate before their rendered trigger output
is captured.

The Superlooper test battery covers fresh CYCLE transitions, control pickup,
newest-stage anchoring, N division without mutation-timing changes, sub-clock
capture, crossed-onset playback, smooth speed endpoints, tape-head resumption,
Skip transport isolation, and all trigger-producing mutation modes.

## Boot Controls

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
