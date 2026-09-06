# Superlooper transport test build

Test image: `build/release/alt-dum-superlooper-v1.0.1-transport-test.hex`.
Baseline: `build/release/alt-dum-superlooper-v1.0.0.hex` (preserved from
commit `68a9ef8`, before these fixes).

This image fixes three Superlooper transport problems:

- Live clocks on LOOP exit follow queued catch-up clocks without masking them.
- Skip catch-up accounts for all `8*N` raw clocks in an eight-stage sequence.
- The enabled clock-stop timeout cancels generated repeats and closes stale
  Chance windows. Incoming clocks resume operation. The compile-time opt-out
  remains available.

The classic runner is unchanged. No EEPROM format or pin assignments change.

## Build and upload

```sh
make build FIRMWARE_VERSION=1.0.1-transport-test BUILD_PATH=build/idum-superlooper-transport-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-transport-test PORT=<detected-port>
```

Use the detected Nano Every port in the last command. The separate build path
is required: plain `make upload` selects the existing baseline build directory.

To restore the preserved baseline image, use the existing baseline build:

```sh
make upload BUILD_PATH=build/idum-superlooper PORT=<detected-port>
```

Do not rebuild that baseline directory from the modified source before using
it for rollback.

## Validation and hardware checks

`make test-superlooper` passes, including the classic output fingerprint and
new tests that run the complete firmware loop through simulated panel inputs.
The three new scenarios failed on the original engine. Tests cover LOOP exit
with 100 us and 1 ms loop steps, Skip catch-up at N=1/2/3/4/6/8, and clock loss
followed by restart. Timeout-disabled behavior is checked in a separate build.
The Nano Every compile uses 46,403 bytes of flash and 4,035 bytes of static RAM.

These remain software tests. On the module, enable Superlooper with CYCLE up
and enable external-sequencer tracking in setup-menu position 0. Use an
eight-step external sequencer with a recognizable pattern:

1. With Chance down, N=1, and L=1, record at least ten clocks. Enter LOOP,
   leave it after a few clocks, and check that the external sequencer returns
   to the expected step. Repeat at different loop speeds. A logic analyzer
   can verify that catch-up pulses retain low gaps while CLOCK IN is high.
2. Set N=3, L=1, and Skip with PARAM on the left. Allow a mutation to start,
   then lower Chance before it ends. Check that the sequencer returns to the
   same phase as an unmodified N-divided clock reference. Repeat with other N
   values and longer L settings.
3. In Skip with PARAM on the right, start clock ratcheting, then stop CLOCK IN.
   Generated clocks should stop after two missed intervals (at least 100 ms).
   Lower Chance and restart CLOCK IN; normal divided clocking should resume.

Record any missed steps, stuck gates, or unexpected trigger changes, together
with N, L, PARAM, clock rate, and whether tracking was enabled.
