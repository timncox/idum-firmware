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

1. `setup()` assigns pins, samples the boot controls, initializes EEPROM, sets interval bookkeeping, attaches trigger interrupts, and displays the startup animation/version.
2. `loop()` ticks software timers, reads gate/CV inputs, edge-detects events, chooses the active mode, handles looper record/playback, applies modification logic, handles the menu, then writes outputs.
3. Mode files `b10_...` through `b80_...` implement the eight mutation modes.
4. `a70_handleDisplay.ino` multiplexes the mode LEDs.
5. `x_utils.ino` handles EEPROM bit packing for options and enabled modes.

## Loop Modulation Switch

This fork repurposes the front-panel `CYCLE` switch as a performance-time loop
modulation layer. When the switch is raised:

- the current mutation mode and mutation length stay latched;
- the live Mode control sets the loop offset/start position;
- the live Length control sets the loop end/length;
- the live Param control sets loop playback speed while LOOP is active; and
- Chance keeps its normal mutation behavior.

Lowering the switch returns Mode, Length, and Param to normal mutation control. The
external-sequencer cycle-maintenance option that originally occupied the switch
is available at setup-menu position 0.

### What happened to the original CYCLE function

In the stock firmware, the physical CYCLE switch enabled external-sequencer
cycle/position maintenance: after IDUM added or removed clocks, it emitted
catch-up clocks intended to return a downstream sequencer to the expected step.
This fork keeps that behavior, but moves its on/off control to setup-menu
position 0 so the physical switch can host the loop-modulation performance
layer.

That move displaces the stock setting previously found at menu position 0.
Originally, position 0 selected fast versus slower “analog clock” timing for
the catch-up pulses. The menu position now enables or disables cycle
maintenance itself, and catch-up speed is fixed to the fast behavior by the
`fastExternalSequencerClock` firmware option.

### Setup-menu map

Hold MODE/REMOVE for about one second to enter or leave the setup menu. The Mode
control selects a ring position; press and release MODE/REMOVE to change that
position's setting.

| Ring position | Setting |
| --- | --- |
| 1 | Toggle the former CYCLE external-sequencer maintenance behavior |
| 2 | Param resolution: powers of two |
| 3 | Param resolution: even values |
| 4 | Param resolution: odd values |
| 5 | Length resolution: odd values |
| 6 | Length resolution: even values |
| 7 | Length resolution: powers of two |
| 8 | Toggle split mode |

While the setup menu is open, pressing LOOP separately toggles the Loop CV
between toggle behavior and loop-while-high behavior. Menu choices are saved to
EEPROM when the menu is exited.

### Post-loop Param pickup

Param also gets a one-shot safety hold when LOOP is turned off while the CYCLE
performance layer is raised. During LOOP, Param remains free to change loop
speed. On exit, firmware restores the mutation Param value from before LOOP and
keeps it frozen until it detects one deliberate movement of the combined Param
knob/CV signal. Normal Param tracking then resumes from the new position.

This is movement pickup rather than value pickup: the control does not need to
cross its old value. Small ADC fluctuations are ignored. The hold is armed only
by leaving LOOP with CYCLE raised; lowering CYCLE cancels it immediately. This
prevents a loop-speed setting from silently becoming a mutation Param setting
the instant LOOP is released. The same handoff applies when LOOP ends from the
button or the Loop CV input, including the menu's loop-while-high behavior.

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
