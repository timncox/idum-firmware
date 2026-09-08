# Alt-DUM

**Alt-DUM is independent alternate firmware for the Mystic Circuits IDUM
Eurorack module, based on the official v0.99 source.** Alt-DUM is designed around
using the **CYCLE switch to choose between stock-derived classic behavior
(down) and an alternate firmware engine (up)**. Each build pairs the classic
engine with one alternate; **Superlooper is the first alternate**, capturing
and replaying the rhythms IDUM actually produces.

**This repository provides source only. No prebuilt firmware download is
provided; installation requires building and uploading it yourself.** See
[Building and uploading](docs/BUILDING.md).

## Alt-DUM versus stock IDUM

To free CYCLE for engine selection, Alt-DUM moves its original
external-sequencer cycle-maintenance behavior into **setup-menu position 0**.
This replaces the old **Fast Sequencer** menu option: fast-sequencer behavior
is now hardcoded on rather than selectable in the menu.

## What Superlooper changes

**Capture the result of mutations.** Superlooper records the result of
mutations rather than the incoming triggers. Stock IDUM reapply chance during
loop playback, so the end result could be different on each iteration.

**Loop the last LEN stages.** Superlooper continuously records the preceding
eight complete stages. On LOOP entry, it freezes that recording and repeats
the most recent **LEN** stages. Loop-length changes take effect at a
loop-stage boundary.  Stock IDUM always starts at the beginning loop position
and plays for LEN stages, making it hard to know what to expect.

**Set clocks per stage.** The setup menu lets you choose **1, 2, 3, 4, 6, or 8
input clocks per stage (N)**, independently of LEN. For example, four clocks
per stage and LEN=4 captures a loop spanning the last sixteen input clocks.
This setting also scales mutation duration: LEN stages last LEN × N input
clocks. The clocks-per-stage setting is saved across reboots.  Set this by
moving LEN in the setup menu.

**Remember separate mutation and loop settings.** Entering LOOP saves the
mutation PARAM and LENGTH settings and restores the previous loop length,
speed, and direction/pause state. Leaving LOOP saves those loop settings and
restores the mutation settings. Recalled values stay active until the
corresponding physical control moves beyond a small noise threshold.
These performance memories last until reboot or a CYCLE engine switch; the
first loop starts at 1× and takes its length from the physical LENGTH control.

**Play the captured phrase at different speeds and in reverse.** Playback can
be smooth or quantized, forward-only or bidirectional, from 1/8× to 8×.
Bidirectional styles add a center pause. Reverse reflects the recorded attacks
across the phrase, including rhythms within a stage. Speed and direction edits
preserve the playhead position; pause freezes it and silences trigger outputs.
Playback style and clocks per stage are saved in the setup menu across
reboots.  Set this by rotating the param switch in the setup menu.

| PARAM position | Playback style          | Ring LED |
|----------------|-------------------------|----------|
| First quarter  | Smooth forward          | 1        |
| Second quarter | Quantized forward       | 3        |
| Third quarter  | Smooth bidirectional    | 5        |
| Fourth quarter | Quantized bidirectional | 7        |


**Keep external transport with the phrase.** When setup-menu position 0
enables tracking, leaving loop causes the external sequencer's position to
advance to where it *would have been* if looping was not engaged.  This means
global sequencer position remains predictable while turning looping on and
off.

**Give mutations a full duration from their trigger.** Each input clock opens
or closes a Chance window. An eligible trigger starts its mutation for LEN × N
input clocks, even when it arrives off-grid. Same-mode triggers do not restart
that duration; an eligible trigger after a MODE change starts a fresh
mutation.  CHANCE uses a gradual cubic curve for sparse variations, and Ball
uses a time-based acceleration/deceleration curve with dry pass-through at
PARAM's center and distinct pulses at its fastest setting.  Ball has been
modified to predictably end its curve after LEN stages.

**Visual feedback for CHANCE, and a new probability curve.** The CHANCE input
has a dead zone at the bottom to account for noise.  When moving CHANCE, the
mode ring is lit up briefly to show the actual chance level, so if MODE LED 0
is on, CHANCE is effectively 0.  This build introduces a cubic curve such that
more of the linear range maps to low probabilities, the idea being that the
difference between 5 and 10% probability is a lot more significant than the
difference between, say 90-95%.  Note: actual probability is modified based on
LEN using logic from the stock firmware.

See the [Superlooper reference](docs/SUPERLOOPER.md) for setup instructions,
control mappings, reverse timing, Chance CV and meter behavior, and transport
details.

## Bug fixes shared by both engines

These improvements apply to both the classic engine and Superlooper:

- **Generated repeats stop when the clock stops.** Clock-dependent mutations
  time out after two missed input-clock intervals (with a 100 ms minimum),
  instead of continuing to generate repeats indefinitely after the clock is
  removed. The [build guide](docs/BUILDING.md#clock-stop-behavior) explains how
  to restore the original unbounded behavior in a custom build.
- **Randomness is seeded at startup and on the first clock.** Control readings
  and timer values seed the random generator, avoiding reliance on Arduino's
  fixed default random sequence after every power-on.

See the [simulator and regression tests](sim/README.md) for verification
details and additional fixes specific to the classic engine.

## Further documentation

- [Building and uploading](docs/BUILDING.md)
- [Boot controls, settings reset, and LED order](docs/BOOT_CONTROLS.md)
- [Host simulator and regression tests](sim/README.md)
- [Hardware diagnostics: Ball timing and ADC ranges](docs/DIAGNOSTICS.md)
- [Ball timing measurements and curve design](docs/BALL_TIMING_MEASUREMENTS.md)
- [Firmware development and source layout](docs/DEVELOPMENT.md)
- [Hardware notes](docs/HARDWARE.md)

## Credits and license

Alt-DUM started from the official IDUM v0.99 firmware by Eli Pechman /
Mystic Circuits. Canonical upstream: [mysticcircuits/IDUM](https://github.com/mysticcircuits/IDUM).

Original license: [Creative Commons Attribution-ShareAlike 4.0](LICENSE.md).
