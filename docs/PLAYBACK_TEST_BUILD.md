# Superlooper playback test build

Image: `build/release/alt-dum-superlooper-v1.0.5-reverse-onsets-test.hex`.
Includes the transport fixes and the 0-10 Chance dead zone.

## Settings

With CYCLE selecting Superlooper, hold MODE to enter setup. Move PARAM into a
region to choose a setting; merely entering setup does not change it.

| PARAM region, left to right | Setting | Ring LED position |
| --- | --- | --- |
| First quarter | Smooth forward | 1 |
| Second quarter | Quantized forward | 3 |
| Third quarter | Smooth bidirectional | 5 |
| Fourth quarter | Quantized bidirectional | 7 |

The LED briefly confirms the selected setting. PARAM and the region boundaries
have movement thresholds to reject jitter. Length continues to select N and
briefly displays its own selected value. Hold MODE to exit setup and save.
The mutation PARAM value is held until the knob moves again after menu exit.

The first LOOP enters at +1x until PARAM moves. Later entries recall the previous
speed and direction, including pause, and Length until each control moves.
Changing playback style in setup resets speed to +1x until PARAM moves in LOOP. In bidirectional mode the left end
is -8x, the center pauses, and the right end is +8x. Quantized bidirectional
steps are:

`-8, -4, -2, -1, -1/2, -1/4, -1/8, pause, +1/8, +1/4, +1/2, +1, +2, +4, +8`

Smooth bidirectional playback continuously approaches pause from each side.
Forward-only settings retain the 1/8x-to-8x range. Reverse reflects recorded
**hit times** across the whole phrase, including the rhythms within stages.
Each hit produces a normal pulse of its recorded width, scaled by speed. If
reversed tails overlap, a one-service-pass low gap separates the next attack.
This can delay that attack by one output service interval; it does not drop it.

Reverse entry starts at a stage attack rather than a clock tail. On an N-divided
stage boundary, it starts one stage before the last complete stage (e.g. after
stage 4, enter on 3, then 2, then 1). Mid-stage entry selects the most recent
complete stage. L=1 repeats its only captured stage. Length changes restart at
the newest captured stage's attack. Playback then continues backward through
all recorded events; stages' internal rhythms are not played forward.

Pause freezes the playhead and silences gates. Speed and direction changes keep
the current tape position, and the last direction is retained through pause.

## External sequencing

Enable external tracking using the existing setup-menu position 0. The firmware
assumes an eight-step sequencer that accepts fast forward clock bursts. Reading
the transport events backward restores their recorded positions by advancing
forward modulo eight, including mutation events within a raw clock. The external
position holds between events rather than jumping at the end of a recorded gate.

The reference position and N-divider phase continue following CLOCK IN while
LOOP plays or pauses. LOOP exit still occurs on the next raw input clock. It
discards unplayed loop-clock backlog, allows a pulse already started to finish,
then catches up to the reference. For example, at N=3, exiting on input pulse 5
returns to step 2; the next normal advance remains on input pulse 7.

At high reverse speeds, clock bursts can lag the trigger tape if the hardware
cannot service them quickly enough. Exit catch-up does not drain that obsolete
backlog. Physical pulse acceptance and timing still need testing on the module
and attached sequencer.

## Validation

- Existing Superlooper and classic parity regression suites.
- All seven forward and fifteen bidirectional speed selections, continuous
  speed monotonicity, hysteresis, and the first-entry +1x latch.
- Input-driven button and gate exit/re-entry across all four playback styles:
  speed/direction/pause and Length recall, jitter rejection, independent control
  pickup, mutation-bank restoration, and retention of quantizer hysteresis.
- Real simulated menu gestures, movement guard, independent N editing,
  settings persistence, and fallback for absent or invalid stored settings.
- Reverse/forward direction changes and phrase wraps against an independent
  checkpoint-and-event reference, including multi-advance transport cells and
  all four trigger lanes. Pause freezes playback and silences gates.
- 144 input-driven reverse-entry cases: N=1/3/8 at every divider phase,
  L=1/4/8, both bidirectional styles, tracking on/off. A recorded beat attacks
  on entry and repeats at the expected raw-clock period, without an extra hit.
  The previous-stage selection, initial alignment, and exit catch-up are checked.
- Input-driven four-lane rhythm recording: an accelerating bounce example,
  a steady beat, unequal pulse widths, and off-grid hits. Observed output onset
  times are compared to an independent reflection of the input times over two
  phrase repeats. The overlap case includes the one-service-pass retrigger gap.
- Direction changes, phrase wraps, and Length restart verify reversed transport
  event order and all four trigger-onset masks against a checkpoint/event oracle.
- 144 input-driven exit cases: both bidirectional settings, reverse/pause/
  forward extremes, and every divider phase at N=1/2/3/4/6/8. Tests use 2 ms
  simulator service intervals to create queue pressure, and check physical
  output edge counts plus the timing of the first normal divided advance.

The host simulator is not an AVR emulator or an electrical timing model.

## Build and upload

```sh
make build FIRMWARE_VERSION=1.0.5-reverse-onsets-test BUILD_PATH=build/idum-superlooper-reverse-onsets-test
make board-list
make upload BUILD_PATH=build/idum-superlooper-reverse-onsets-test PORT=<detected-port>
```

The previous transport and Chance test build directories remain available for
rollback. When uploading this image, use the reverse-onsets-test build path above.

For the first hardware check, use Chance down, an eight-step recognizable
sequence, and a steady clock. Select quantized bidirectional playback, enter
LOOP, and sweep through reverse, pause, and forward. Exit at several points;
repeat with N=3 and verify that both sequence position and divider phase return
to the uninterrupted clock reference. Record a Ball mutation and check that its
hit spacing reverses while beat-aligned hits remain on the beat. Then test
higher speeds and other N settings.
