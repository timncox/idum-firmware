# Boot controls and firmware identification

[Back to the overview](../README.md)

### Identifying Alt-DUM

After any boot-control confirmation, Alt-DUM shows this mode-LED
signature:

1. The two outer LEDs appear together for 250 ms.
2. The pair steps inward three times, ending on the two center LEDs.
3. The animation lands on the single far-left LED for 250 ms.

The full signature lasts about 1.25 seconds. Stock IDUM v0.99 instead holds a
single orange mode LED, so the two firmware families are visually distinct at
power-on.

The mode LEDs share a three-bit hardware multiplexer, so only one ring LED is
electrically selected at any instant. Firmware rapidly alternates between the
two LEDs in each frame, making the pair appear continuously lit.

Two power-on gestures keep user settings and the physical mode-ring wiring
separate:

- Hold **MODE/REMOVE** while powering on to restore the normal firmware options,
  enabled modes, and Param/Length resolutions. The stored LED order is preserved.
- Hold **LOOP** while powering on to toggle between the hardware v1.3+ mode-ring
  order and the legacy order. The selection is stored in EEPROM byte 5.

Fresh EEPROM is initialized for hardware v1.3 and later. Installing this firmware
over the previous `Q` EEPROM format performs a one-time settings reset and writes
the new `R` format. The blinking LOOP LED confirms first-time initialization;
forward and reverse ring sweeps confirm settings reset and LED-order toggle.



See [hardware notes](HARDWARE.md) for wiring details.
