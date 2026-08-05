# IDUM Host Simulator

This directory contains a small desktop harness for running the IDUM firmware
logic without flashing the module. It is not an ATmega4809 emulator. Instead,
it provides fake Arduino functions and records pin writes while the firmware's
normal `setup()` and `loop()` functions run on the host.

Run the smoke test:

```sh
make sim
```

Run one characteristic scenario for each documented mode:

```sh
make sim-modes
```

Run the faithful bug-fix regression suite against this fork only—no module or
external source checkout required:

```sh
make sim-regressions
```

Run the clock-stop regression for generated repeats:

```sh
make sim-clock-stop
```

Print Break mode's preset rhythm table:

```sh
make sim-break-patterns
```

Recreate Break mode's preset rhythms through simulated clock and trigger I/O:

```sh
make sim-break-playback
```

Compare stock v.99, this fork, and ID00M's claimed faithful bug fixes using
an extracted copy of the public ID00M source repository:

```sh
make sim-id00m-regressions ID00M_ROOT=/path/to/id00m
```

The comparison stages both external firmware trees in a temporary directory
and overlays only the simulator's desktop fast-I/O shim. It does not modify the
stock or ID00M sources.

Or run the simulator binary directly:

```sh
build/sim/idum-sim smoke
build/sim/idum-sim hold
build/sim/idum-sim clock-ratchet
build/sim/idum-sim trigger-ratchet
build/sim/idum-sim ball
build/sim/idum-sim rotate
build/sim/idum-sim delay
build/sim/idum-sim break
build/sim/idum-sim clock-stop-burst
build/sim/idum-sim break-patterns
build/sim/idum-sim break-playback
build/sim/idum-sim skip
build/sim/idum-sim all-modes
```

The simulator currently stubs:

- `micros()`, `millis()`, and `delay()`
- `digitalRead()`, `digitalWrite()`, `digitalReadFast()`, `digitalWriteFast()`
- `analogRead()`
- `EEPROM`
- `random()`
- `attachInterrupt()`

The trace records output changes for:

- `CL_OUT`
- `TR1_OUT`
- `TR2_OUT`
- `TR3_OUT`
- `TR4_OUT`

Remember that IDUM's hardware inverts output gates, so a firmware write of
`LOW` means the physical output is on/high.

Good next scenarios:

- clock-stop regression for stuck generated retriggers
- chance curve sweep
- fixed per-track mode assignment
- loop-bound modulation
- CV routing switch behavior
