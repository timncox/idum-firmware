# Superlooper controls and behavior

[Back to the overview](../README.md)

## Recording and loop length

Superlooper is an independent rendered-output looper alongside the
stock-derived classic runner. Superlooper taps the final four trigger
states after mutation and choke resolution into a rolling tape with 32 cells
per raw clock. LOOP bypasses mutation rendering and plays that tape directly,
so the loop repeats what the module actually emitted. LOOP entry and exit are
committed on the next raw clock edge, keeping the tape and external sequencer
transport aligned.

One sequencer stage contains `N` raw clocks. Six contiguous regions of the
calibrated Length slider select `N = 1, 2, 3, 4, 6, 8` while the setup menu is
open; the selected ring LED is shown for one second and the value is saved in EEPROM.
Outside LOOP, CLOCK OUT advances once per N raw clocks. Superlooper always keeps
the preceding eight complete N-clock stages. Length chooses the backward span:
`1` repeats only the newest complete stage, `4` selects the last four stages,
and `8` selects the complete captured window. Forward playback reads the
selected span oldest-to-newest; reverse entry and ordering are described below.

When external-sequencer tracking is enabled, Superlooper records sequencer
advances alongside the rendered trigger tape. Playback replays those advances
at their recorded tape positions, scaled by the active loop speed. Mutations
span `L * N` raw clocks, so their transport catch-up is computed in complete
Superlooper stages: for example, `N=2, L=8` advances an external sequencer eight
times over sixteen raw clocks and returns an eight-step sequence to its starting
stage.

Param and Length have separate mutation and loop memories. You can adjust both
for mutations outside LOOP, then re-enter LOOP with its previous speed,
direction/pause, and length intact. Each physical control takes over its newly
active value only after moving beyond a small noise threshold. Leaving LOOP
similarly restores the previous mutation values.

## Playback setup

With CYCLE up, hold MODE to enter setup. Move PARAM into one of four quarters to
select the playback style, then hold MODE again to save and exit:

| PARAM position, left to right | Playback style | Ring LED |
| --- | --- | --- |
| First quarter | Smooth forward | 1 |
| Second quarter | Quantized forward | 3 |
| Third quarter | Smooth bidirectional | 5 |
| Fourth quarter | Quantized bidirectional | 7 |

Entering setup alone does not change the selection; PARAM must move. Length
continues to edit N, and the existing mode-dial menu options remain available.
**Playback style and N survive reboot** after saving on setup exit. The loop and
mutation performance memories last only until reboot or a CYCLE engine switch.
The first LOOP then starts at +1x and takes its length from the physical slider.
Changing playback style in setup also resets loop speed to +1x until PARAM moves
in LOOP. The default style is smooth forward, from 1/8x through 8x with a broad
exact-speed center.

## Speed, reverse, and pause

Quantized forward speeds are 1/8x, 1/4x, 1/2x, 1x, 2x, 4x, and 8x, with
hysteresis between adjacent settings. Bidirectional PARAM runs from reverse
8x at the left through a center pause to forward 8x at the right. Its quantized
version uses the same seven magnitudes in each direction. Reverse reflects the
recorded trigger onsets and transport events across the whole phrase, including
rhythms within a stage. It emits normal trigger pulses with recorded widths
scaled by speed. Overlapping reversed tails are separated by one output-service
pass so the next attack remains audible.

Reverse entry is anchored to a stage attack. On a divided stage boundary it
starts one stage before the most recently completed stage; between divided
boundaries it uses the most recent complete stage. L=1 repeats its sole captured
stage. Thus an ordinary beat stays on the beat while a recorded accelerating
bounce plays with decelerating hit spacing. Length changes restart at the newest
captured stage's attack. Pause freezes the playhead and silences trigger outputs.
Speed and direction changes preserve the tape position; direction is retained
through pause. Control recall and the first-entry 1:1 default remain unchanged.

CLOCK IN continues advancing the independent eight-step reference and N-divider
phase during forward playback, reverse playback, and pause. With external
tracking enabled, LOOP exit discards unplayed playback clocks and catches up
from the emitted sequencer position to that reference. The next normal advance
remains on the original N-divided clock grid. Reverse transport uses forward
clock bursts to reach preceding steps on an eight-step external sequencer.

Moved Length changes take effect at a loop-stage boundary; PARAM speed changes
take effect immediately.

## Chance and its display

Chance remains live while recording and follows a cubic curve. Readings 0-15
(about 1.5% of the ADC range, matching stock v0.99) block new mutations. Above
15, the curve rises gradually toward guaranteed eligibility at 1000-1024. Approximate base probabilities at
quarter, half, and three-quarter input range are 1.45%, 12.8%, and 44.5%. The curve
uses 16-bit probability resolution to retain very small probabilities near the
bottom. The existing length penalty still reduces eligibility below the top.

A CV gate that drives the combined Chance reading to 1000 or above forces the
next clock's eligibility window open, even with an active length penalty.
Dropping to 15 or below closes open windows immediately. Existing mutations
finish their sampled duration. Firmware sees one combined slider/CV reading,
so the floor is not an independent physical-fader mute. These probabilities
apply per raw clock: faster clocks and longer mutations can sound denser.

Chance decisions are already present in the recorded tape and are not reapplied
during LOOP playback.

Changing Chance temporarily turns the mode ring into a probability meter:
leftmost LED (position 0) means off, and positions 1-7 divide the curved base
probability into seven equal bands. While the meter is visible, position 1
appears immediately above the floor. This shows probability before the length
penalty, not a mutation firing.
The normal display returns one second after the last input movement exceeding
12 ADC counts. While visible, the meter follows the current reading, including
one-count crossings of the floor; these crossings do not restart the timer.
Both slider and CV drive the meter, so substantial ongoing CV movement can keep
it visible. Small changes accumulate against the last movement reference, so a
slow sweep still opens it. Setup and LOOP position take priority. See the
[Chance meter test notes](CHANCE_METER_TEST_BUILD.md) for the mapping and
patched-gate test procedure.

## Mutation timing and Ball

Each raw CLOCK IN edge evaluates Chance and opens or closes an eligibility
window until the next clock edge. A trigger inside an open window begins its
selected mode immediately for a sampled `L` stages (`L * N` raw clocks), even
when the trigger is off-grid. A same-mode trigger during that mutation does not
restart its lifetime; an eligible trigger after a Mode change preempts it with a
fresh L-stage mutation. Mode changes without triggers do nothing. PARAM remains
live, and Ball additionally allows live L movement to bend the active curve
without changing its sampled endpoint.

SuperLooper Ball uses a time-domain curve while the classic Ball implementation
remains untouched. Center PARAM is dry pass-through. At either side, the curve
moves continuously between the most recent plausible trigger period and a fast
14 ms period that preserves a real low gap instead of collapsing into a solid
gate. With tracking enabled, Skip modifies external clock transport and restores
its N-divided phase using an eight-stage (`8*N` raw-clock) reference. With tracking
disabled, external clocking stays on the plain N division. Skip retains the
trigger-channel pass-through behavior.



For the complete mutation state contract, see the
[mutation scheduler](SUPERLOOPER_MUTATION_SCHEDULER.md).
For build instructions, see [Building and uploading](BUILDING.md).
