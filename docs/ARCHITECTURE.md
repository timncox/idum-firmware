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

`loop()` lives in `a20_Loop.ino` and runs this pipeline:

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
- `x_utils.ino` - EEPROM packing/unpacking helpers.

## Hardware Notes To Verify

The imported source defines:

- gate inputs: `clockPin=15`, `trig1Pin=14`, `trig2Pin=9`, `trig3Pin=11`, `trig4Pin=10`
- gate outputs: `clockOut=13`, `trig1Out=7`, `trig2Out=8`, `trig3Out=6`, `trig4Out=5`
- analog inputs: `modePin=21`, `probPin=20`, `lengPin=18`, `paramPin=19`
- controls: `loopButton=16`, `loopGate=12`, `cycleSw=17`, `modeBtn=0`
- LEDs: `loopLED=1`, `mode1LED=2`, `mode2LED=3`, `mode3LED=4`

The pin numbering and interrupt usage suggest the production build may target an Arduino Nano Every / ATmega4809-compatible core. Confirm this before flashing hardware.

