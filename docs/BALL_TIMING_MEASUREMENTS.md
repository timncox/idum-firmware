# SuperLooper Ball timing measurements

Captured on the Nano Every with the EEPROM-backed diagnostic image on
2026-08-30. The raw clock period was approximately 125 ms, `L=1`, and the
diagnostic counted complete SuperLooper engine passes during the first TR1 Ball
clock interval.

| Direction | PARAM | Engine loops | Clock interval | Approx. loop time | Opening divider |
| --- | ---: | ---: | ---: | ---: | ---: |
| Speeding up | 1 (fully left) | 141 | 125,092 us | 887 us | 100 |
| Slowing down | 1024 (fully right) | 132 | 125,104 us | 948 us | 10 |

## What the captures explain

- On the speeding side, divider 100 consumes about 89 ms, or 71% of the entire
  `L=1` gesture. There is not enough time left to traverse the stock
  `100 -> 55 -> 30 -> 16` sequence.
- On the slowing side, the stock `10 -> 11 -> 12...` opening spends most of the
  gesture with a 10-loop-high gate and almost no low gap, so it reads as a
  near-constant tone instead of a Ball curve.
- Evaluating a clock-progress target only at bounce boundaries fails because a
  sparse first bounce can occur after most of the gesture has elapsed. This
  produced the rejected `hit -> gap -> sudden fast` behavior.
- Holding divider 11 is also rejected: with the stock 10-loop gate width it is
  technically retriggering but practically continuous.

## Design constraints for the time-domain Ball

- Neutral PARAM produces no added Ball events.
- At full deflection, the slow boundary matches the most recent trustworthy
  per-channel trigger interval, falling back to the raw clock interval.
- The fast boundary retains a real low gap after the 10 ms generated trigger
  pulse; it must approach but never become a solid gate.
- The rate changes continuously over the sampled `L * N` raw-clock duration.
  Events come from accumulated phase, rather than from a divider sampled at
  sparse bounce times.
- Sampled L fixes the mutation endpoint. Live L may reshape the remaining curve
  without resetting phase, duplicating an event, or extending the mutation.
- Further input triggers do not restart the active gesture. They may update the
  trigger-period estimate used by the next mutation.

## Implemented SuperLooper curve

The production SuperLooper renderer now uses a microsecond phase accumulator;
the shared classic `ball()` implementation remains unchanged. The control
center (`PARAM` 480-544) is dry pass-through. Outside that deadband, distance
from center continuously scales the rate excursion:

- left travels from the measured trigger period toward a 14 ms period;
- right travels from 14 ms toward the measured trigger period;
- 14 ms is the 10 ms generated pulse plus a guaranteed 4 ms low gap;
- the curve spans the raw-clock period times sampled `L * N`, with an 8 ms
  control slew;
- live L bends progress toward an ease-out or ease-in shape without changing
  the sampled endpoint or resetting accumulated phase.

The channel's last plausible trigger interval defines its slow boundary. An
unmeasured or implausible trigger interval falls back to the raw clock period,
then to 125 ms if the clock has not yet been measured.
