# Firmware development

[Back to the overview](../README.md)

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

## Current firmware flow

At a high level:

1. `setup()` assigns pins, samples the boot controls, initializes EEPROM, sets interval bookkeeping, attaches trigger interrupts, and displays the startup animation/version.
2. `c00_AlternateEngine.ino` supplies the production `loop()` dispatcher: CYCLE down runs the classic pipeline and CYCLE up runs the selected alternate.
3. The classic input adapter saves raw CYCLE for dispatch, then places saved menu setting 0 into the legacy `cycleIn` variable before stock looper and mutation functions run.
4. Mode files `b10_...` through `b80_...` implement the eight mutation modes.
5. `a70_handleDisplay.ino` multiplexes the mode LEDs.
6. `x_utils.ino` handles EEPROM bit packing for options and enabled modes.

## Alternate engine framework

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

Setup-menu position 0 replaces the former Fast Sequencer option with the
original CYCLE behavior, freeing the physical switch for classic/alternate
engine selection. Fast-sequencer behavior is hardcoded on
(`fastExternalSequencerClock = 1`); changing it requires rebuilding.
Superlooper is the first alternate engine.

CYCLE is never used directly by either sequencing engine. In the classic
runner, setup-menu position 0 enables or disables the legacy external-sequencer
cycle/position maintenance. Superlooper always owns its N-divided CLOCK OUT;
position 0 enables its recorded external-transport lane, loop-position jumps,
and exit catch-up. Jump and catch-up bursts use a fixed fast pulse cadence that
does not vary with N. With tracking disabled, Superlooper emits plain N-divided
clocking outside LOOP and clock pulses at loop-stage boundaries during playback;
it does not restore the external sequencer's absolute position. The classic
runner otherwise retains its existing clock ordering and timing algorithms.

See the [architecture and file map](ARCHITECTURE.md),
[engine extension guide](../firmware/engines/README.md), and
[host simulator](../sim/README.md) for more detail.
