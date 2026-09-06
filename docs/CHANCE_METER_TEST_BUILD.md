# Superlooper Chance meter test build

Image: `build/release/alt-dum-superlooper-v1.0.8-chance-meter-noise-test.hex`.
Builds on v1.0.7's meter and stock-sized floor. Version 1.0.8 fixes the display
being held open by small input changes or repeated dead-zone crossings.
Classic behavior and the Chance probability calculation are unchanged.

## Chance mapping

The dead zone is 0-15 ADC counts, matching stock v0.99 (about 1.5% of the input
range). The v1.0.6 test image used 0-50. Above 15, normalize the input up to
1000 and cube it. Readings 1000-1024 still guarantee eligibility on the next
raw clock, including with a length penalty. A higher custom shared floor wins.

| Combined slider/CV reading | Base probability per raw clock |
| --- | --- |
| 0-15 | 0% |
| 16 | 1/65536 |
| 102 | 0.067% |
| 256 | 1.45% |
| 512 | 12.79% |
| 768 | 44.54% |
| 922 | 77.85% |
| 1000-1024 | 100% |

The firmware sees one combined slider/CV input, not physical fader position.
Dropping to the floor closes mutation windows immediately; existing mutations
keep their sampled lifetime. Chance is not reapplied to recorded LOOP playback.

## Temporary meter

One mode LED shows the curved base probability, before the length penalty:

| LED position (left to right, numbered 0-7) | Probability |
| --- | --- |
| 0 | Off, inside the dead zone |
| 1 | Above 0 through 1/7 (~14.3%) |
| 2 | Above 1/7 through 2/7 (~28.6%) |
| 3 | Above 2/7 through 3/7 (~42.9%) |
| 4 | Above 3/7 through 4/7 (~57.1%) |
| 5 | Above 4/7 through 5/7 (~71.4%) |
| 6 | Above 5/7 through 6/7 (~85.7%) |
| 7 | Above 6/7 through 100% |

The leftmost LED lights to mean off; it is not a blank display. Movement of
more than 12 ADC counts from the last movement reference opens the meter or
extends its one-second timer. Slow sweeps accumulate against that reference.
While visible, the readout follows the current curved probability, including
single-count crossings of the 15-count floor. These crossings alone do not open
or extend the display, so small fluctuations around the floor cannot hold it
open forever. This changes only display detection, not the actual Chance ADC,
floor, curve, or gate response. Large noise or continuous CV changes exceeding
12 counts can still keep the meter visible; actual noise amplitude is unmeasured.
Setup and LOOP suppress the meter and retain their normal displays; leaving
either requires fresh Chance movement to reopen it. Engine selection clears it.

## Validation and hardware check

The simulator checks the complete ADC curve, every midpoint random roll,
length penalties, immediate floor closure, and high-gate reopening with Split
on/off. Meter checks cover all eight positions and the actual mux pin outputs,
one-count floor crossings while visible, timeout, display priority, and repeated
+/-6-count jitter both around the floor and in the live range. Noise tests check
that the display expires and stays hidden, then reopens on a slow deliberate
sweep. These model noise cases; they do not measure hardware ADC noise.
The full Superlooper battery and classic output parity passed for v1.0.8. The
Nano Every build uses 49,101/49,152 bytes of flash and 4,066/6,144 bytes of RAM.
The v1.0.8 image was uploaded with verification successfully; physical noise
rejection still needs confirmation on the module.

```sh
make test-superlooper
make build FIRMWARE_VERSION=1.0.8-chance-meter-noise-test BUILD_PATH=build/idum-superlooper-chance-meter-noise-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-chance-meter-noise-test PORT=<detected-port>
```

On the module, sweep Chance and check the meter returns to the mode display
after settling. Leave the control stationary at the bottom and at midrange to
check that noise does not keep the overlay on. Patch the Planar2 gate: low
should select position 0 and stop new mutations; high should select position 7 and force eligibility when it
spans a CLOCK edge. Check setup and LOOP still show their normal positions.
Actual LED readability and the patched gate's voltage offset need hardware
confirmation; the stock floor does not calibrate the other rack's low level.
The v1.0.7 build remains available locally for rollback; do not rebuild its
historical directory from newer source.
