# Hardware diagnostics

Run these commands from the repository root after following the
[toolchain setup](BUILDING.md). These are locally built diagnostic images.

## Ball loop-speed diagnostic

The optional Ball timing image measures how many SuperLooper engine iterations
fit in one real CLOCK interval while TR1 Ball is active. It does not change the
production image or print during the measured interval. The first sample after
boot is saved to EEPROM so it survives the USB-disconnect test workflow.

```sh
make ball-timing-diagnostic-test
make ball-timing-diagnostic-build
make ball-timing-diagnostic-upload PORT=/dev/cu.usbmodem101
```

After flashing, disconnect USB, run a TR1 Ball mutation for at least one full
clock interval, then reconnect USB. At 115200 baud the saved line repeats once
per second until the next clock arrives. It reports engine loops, clock period,
PARAM, sampled and live L, direction, and the opening divider.

See [Ball timing measurements](BALL_TIMING_MEASUREMENTS.md) for recorded results
and the time-domain curve design.

## ADC diagnostic

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
