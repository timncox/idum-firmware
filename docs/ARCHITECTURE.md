# Firmware Architecture Notes

These notes describe the imported v.99 firmware before any behavioral overhaul.

## Runtime Shape

The firmware is a multi-tab Arduino sketch. Arduino concatenates the `.ino` files into one translation unit, so the many globals in `a00_Variables.ino` are visible everywhere.

`idum.ino` owns library includes and global timer instances:

- `fastdigitalIO.h` for optimized AVR digital writes
- `EEPROM.h` for persisted mode/menu state
- `arduino-timer.h` for cooperative software timers

`setup()` lives in `a10_Setup.ino` and:

- configures all digital/analog pins
- flashes the firmware version on the mode LEDs
- initializes EEPROM defaults or reads saved options
- initializes timing boundary state
- attaches rising-edge interrupts for trigger inputs 1-4
- starts the display timer

The production `loop()` lives in `c00_AlternateEngine.ino` and dispatches
between the classic runner and one compile-time-selected alternate. The
classic runner duplicates only the top-level pipeline from `a20_Loop.ino`; its
input adapter captures raw CYCLE for dispatch and then substitutes saved menu
setting 0 before calling the stock looper and mutation functions. That pipeline
runs:

1. Tick clock/display software timers.
2. `readInputs()` samples gate, button, switch, and CV inputs.
3. `edgeDetect()` derives rising/falling edge flags and timing intervals.
4. `chooseMode()` maps the mode CV/pot into one of the active mode slots.
5. `handleLooper()` records incoming gate data or plays stored loop data.
6. `modifyTest()` decides whether a modification is active and samples per-channel parameters.
7. `handleMenu()` processes the hold-mode-button configuration menu.
8. `handleOutputs()` writes final clock and trigger states to the output pins.

## File Map

- `a0000_superSecretOptions.ino` - tweakable behavior flags such as clock speed and carryover behavior.
- `a00_Variables.ino` - pin assignments and global runtime state.
- `a01_timers.ino` - cooperative timer callbacks for slow clock and LED display scanning.
- `a10_Setup.ino` - startup, pin setup, EEPROM initialization, interrupt setup.
- `a20_Loop.ino` - top-level runtime loop.
- `a30_ReadInputs.ino` - input sampling.
- `a31_EdgeDetect.ino` - edge detection, debounce, timing interval tracking, derived clock multiplication.
- `a32_PinInterrupts.ino` - ISR handlers for loop recording timing.
- `a40_handleLooper.ino` - loop enable/record/playback coordination.
- `a41_LoopTriggers.ino` - loop playback clocking and trigger generation.
- `a42_RecordTriggers.ino` - loop recording state and tick calculation.
- `a50_ChooseMode.ino` - mode selection and enabled-mode EEPROM save.
- `a51_boundaryUtils.ino` - mode boundary calculation utilities.
- `a60_ModifyTest.ino` - probability and per-channel modification activation logic.
- `a70_handleDisplay.ino` - LED display multiplexing.
- `a80_HandleMenu.ino` - hidden option menu.
- `a90_handleOutputs.ino` - final output pin writes.
- `a91_followUtils.ino` - clock/choke/follow behavior helpers.
- `b10_holdMode.ino` - hold mode.
- `b20_burstMode.ino` - burst mode.
- `b30_multiplyMode.ino` - multiply mode.
- `b40_ballMode.ino` - ball mode.
- `b50_rotateMode.ino` - rotate mode.
- `b60_delayMode.ino` - delay mode.
- `b70_breakMode.ino` - break mode.
- `b80_skipMode.ino` - skip mode.
- `c00_AlternateEngine.ino` - compile-time selection of one alternate runner.
- `c10_SuperlooperEngine.ino` - independent Superlooper runner and reset boundary.
- `engine_config.h` - alternate engine IDs and the production alternate default.
- `x_utils.ino` - EEPROM packing/unpacking helpers.

## Alternate Runner Boundary

Every production build contains the classic runner plus exactly one
compile-time-selected alternate. The physical CYCLE switch always selects
between them. Menu position 0 independently enables the legacy
external-sequencer tracking/catch-up behavior in either runner, and
`fastExternalSequencerClock` selects its pulse speed at rebuild time (fast by
default). A CYCLE transition resets the alternate boundary and deliberately
provides no phase or state handoff guarantee.

Alternates may duplicate the state-machine functions they need, but share the
hardware, display, persistence, utility, and mutation-mode primitives. A literal
second copy of the entire firmware would not fit in ATmega4809 flash.

Superlooper currently owns its input-control routing and looper coordination in
`c10_SuperlooperEngine.ino`. It reuses the calibrated ADC scaling, history
arrays, modification playback, output modes, and external-sequencer helpers,
but does not add conditionals to the classic looper.

Its loop window is expressed relative to the newest captured history cell:
offset zero is the anchor and a length `L` begins at `1-L`. Thus `L=4` walks
relative positions `-3,-2,-1,0` before wrapping. Param and Length use separate
mutation/loop banks with movement pickup at LOOP transitions. Loop Param resets
to 1:1 on entry; loop Length recalls its prior span and initializes to eight.
Chance is intentionally not banked or reinterpreted while the engine still
records input triggers and replays mutation metadata.

## Verification Contract

The two runners have separate test contracts:

- `make test-stock-reference` runs the bug-fix regression suite and all classic
  mode scenarios against the simulator-only no-alternate oracle.
- `make test-superlooper` compares an exact deterministic stock-runner
  fingerprint between the stock-only and Superlooper binaries, then runs the
  alternate-specific battery with CYCLE up, including control pickup and loop
  anchor/window regressions.
- `make test-all` runs both contracts.

The stock fingerprint is deliberately stricter than checking that scenarios
merely pass: the observed output writes and selected internal state must remain
identical when an alternate engine is compiled beside the stock runner. The
Superlooper battery is independent so its expected behavior can evolve without
weakening that isolation check.

## Hardware Notes To Verify

The imported source defines:

- gate inputs: `clockPin=15`, `trig1Pin=14`, `trig2Pin=9`, `trig3Pin=11`, `trig4Pin=10`
- gate outputs: `clockOut=13`, `trig1Out=7`, `trig2Out=8`, `trig3Out=6`, `trig4Out=5`
- analog inputs: `modePin=21`, `probPin=20`, `lengPin=18`, `paramPin=19`
- controls: `loopButton=16`, `loopGate=12`, `cycleSw=17`, `modeBtn=0`
- LEDs: `loopLED=1`, `mode1LED=2`, `mode2LED=3`, `mode3LED=4`

The pin numbering and interrupt usage suggest the production build may target an Arduino Nano Every / ATmega4809-compatible core. Confirm this before flashing hardware.
