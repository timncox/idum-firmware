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

The suite currently covers 16 behaviors, including aggregate failure status,
interrupt-record consumption, looper recording phase, per-channel frozen
parameters, removed-mode hysteresis, ROTATE routing/chokes, ratchet safety,
and startup RNG seeding. The interrupt race scenario injects an edge exactly
when the simulated critical section re-enables interrupts; this exercises the
firmware's consume/clear ordering without pretending to emulate the AVR.

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
build/sim/idum-sim all-regressions
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

## Engine verification

Run these commands from the repository root:

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

The `IDUM_SIM` compatibility shim routes fast digital I/O through the fake
Arduino functions. Hardware builds retain the normal AVR fast I/O path.
The simulator exercises firmware logic; final timing validation still requires
the module. See [hardware diagnostics](../docs/DIAGNOSTICS.md).
