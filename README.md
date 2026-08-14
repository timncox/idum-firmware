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

1. `setup()` assigns pins, displays the firmware version, initializes EEPROM options, sets interval bookkeeping, and attaches trigger interrupts.
2. `loop()` ticks software timers, reads gate/CV inputs, edge-detects events, chooses the active mode, handles looper record/playback, applies modification logic, handles the menu, then writes outputs.
3. Mode files `b10_...` through `b80_...` implement the eight mutation modes.
4. `a70_handleDisplay.ino` multiplexes the mode LEDs.
5. `x_utils.ino` handles EEPROM bit packing for options and enabled modes.

## Loop Modulation Switch

This fork repurposes the front-panel `CYCLE` switch as a performance-time loop
modulation layer. When the switch is raised:

- the current mutation mode and mutation length stay latched;
- the live Mode control sets the loop offset/start position;
- the live Length control sets the loop end/length; and
- Chance and Param keep their normal mutation behavior.

Lowering the switch returns Mode and Length to normal mutation control. The
external-sequencer cycle-maintenance option that originally occupied the switch
is available at setup-menu position 0.

See [docs/HARDWARE.md](docs/HARDWARE.md) for hardware notes, [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for a file map, and [docs/CODE_WALKTHROUGH.md](docs/CODE_WALKTHROUGH.md) for a source-derived explanation of how v0.99 works.

## Source

Canonical upstream source: [mysticcircuits/IDUM](https://github.com/mysticcircuits/IDUM).

This repo started from the v.99 firmware archive at `IDUM-github-official/firmwares/IDUM_Firmware_v_99.zip`.

Original author: Eli Pechman / Mystic Circuits.

Original license statement: Creative Commons Attribution-ShareAlike 4.0.
