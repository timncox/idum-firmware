# Hardware Notes

Observed module hardware:

- Mystic Circuits IDUM main PCB, silkscreened `IDUM TRIGGER MULTI-FX`.
- PCB text: `Designed by Eli Pechman in 2021`.
- Hardware revision text visible near the Nano footprint: `V1.3 Hardware Redesign 10/18/25`.
- Controller daughterboard: Arduino Nano Every.
- Main MCU marking: ATmega4809.
- USB connector: micro USB on the Nano Every.
- Upload target: Arduino megaAVR Nano Every.

## Arduino CLI Target

Use:

```sh
arduino:megaavr:nona4809:mode=off
```

This sets Nano Every register emulation to `None`, matching upstream build notes.

The plain `arduino:megaavr:nona4809` FQBN defaults to ATmega328 register emulation. Keep it as a diagnostic override only.

## Upload Method

Arduino's board package reports:

- upload protocol: `jtag2updi`
- upload speed: `115200`
- 1200 bps touch reset: enabled
- VID/PID: `2341:0058`
- max flash: `49152` bytes
- max RAM: `6144` bytes

This means a normal Nano Every bootloader upload should work through the USB port:

```sh
make upload PORT=/dev/cu.usbmodem...
```

## Mode-ring LED Order and Boot Reset

Hardware v1.3 and later uses the remapped mode-ring mux order observed on this
module. Earlier boards use the original direct mux order. Firmware stores that
hardware choice independently in EEPROM byte 5:

- Hold **LOOP** during power-on to toggle the LED order. A reverse ring sweep
  confirms the change; release LOOP to continue booting.
- Hold **MODE/REMOVE** during power-on to restore user settings. A forward ring
  sweep confirms the reset; release MODE to continue booting. This reset does
  not change the stored LED order.

The default for a new or newly formatted EEPROM is the v1.3+ order. The EEPROM
format marker is `R`; upgrading from an older format resets bytes 1–4 once.

## Firmware Size Baseline

With `arduino:megaavr:nona4809:mode=off`, imported v0.99 builds at:

```text
Sketch uses 31689 bytes (64%) of program storage space. Maximum is 49152 bytes.
Global variables use 1323 bytes (21%) of dynamic memory, leaving 4821 bytes for local variables. Maximum is 6144 bytes.
```
