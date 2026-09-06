# Superlooper Chance curve test build

Image: `build/release/alt-dum-superlooper-v1.0.6-chance-curve-test.hex`.
Historical v1.0.6 image: the current build restores the stock 15-count floor
and adds a meter; see [CHANCE_METER_TEST_BUILD.md](CHANCE_METER_TEST_BUILD.md).
The commands below rebuild the checked-out source, not this historical image.

Includes the playback, reverse-onset, control-recall, and transport changes from
v1.0.5. Classic firmware probability behavior is unchanged.

## Mapping

The lower dead zone is now 0-50 ADC counts, approximately 5% of the full input
range. Above that, the normalized live range from 50 to 1000 follows a cubic
curve. Readings 1000-1024 are guaranteed on, including when the length penalty
is active, so a high CV gate can force eligibility without reaching the final
ADC count. A larger custom `probabilityLowerThreshold` still takes precedence.

| Combined Chance input | Base probability per raw clock |
| --- | --- |
| 0-50 | 0% |
| 102 (about 10% input) | 0.015% |
| 256 (25% input) | 1.02% |
| 512 (50% input) | 11.43% |
| 768 (75% input) | 43.02% |
| 922 (90% input) | 77.11% |
| 1000-1024 | 100% |

Input percentages refer to the combined ADC reading, not calibrated physical
slider travel. The firmware cannot separate fader position from incoming CV.
The curve uses integer Q10 math and 16-bit random rolls; values just above the
floor retain a minimum nonzero probability of 1/65536. Below the top saturation
zone, the length penalty keeps its previous fractional strength.

A high gate opens eligibility on a raw clock edge. Returning to the floor closes
open windows immediately; already active mutations finish their sampled
lifetime. This does not change mutation durations or the Ball timing curve.
More clocks per second mean more opportunities, and long mutations can make
low probabilities sound denser than the nominal per-clock percentage. Chance
is not reapplied during LOOP playback.

## Validation

- Every ADC input from 0 to 1024 checked against an independent cubic reference,
  with monotonicity, floor, and top saturation assertions.
- All 65536 possible rolls checked at the midpoint.
- Existing length-penalty fractions checked on the new random scale, and the
  top endpoint checked with penalties from 0 through 7.
- Actual simulated panel input transitions from high to low and back, with
  Split on and off: immediate closure, retained active mutation lifetime,
  next-clock reopening, and all-lane eligibility at a high reading of 1000.
- Full Superlooper regressions and classic output parity.

## Build and test

```sh
make test-superlooper
make build FIRMWARE_VERSION=1.0.6-chance-curve-test BUILD_PATH=build/idum-superlooper-chance-curve-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-chance-curve-test PORT=<detected-port>
```

For the patched Planar2 gate, first confirm the low level stops new mutations
with Chance down. Then confirm the high level forces eligibility when it spans
a CLOCK IN edge. If either endpoint fails, measure the combined ADC reading
with the existing ADC diagnostic before changing the thresholds again. The
larger floor is a test value, not a calibration of that gate or rack.

Sweep Chance slowly with fast clocks and short mutations to evaluate the new
low-probability range. Then test longer L and N values. The v1.0.5 build directory
is retained locally for rollback; do not rebuild it from changed source.
