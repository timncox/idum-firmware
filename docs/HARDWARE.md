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

The `mode=off` board option disables ATmega328 register emulation and uses native ATmega4809 register definitions. The imported v0.99 firmware compiles successfully with this target.

The plain `arduino:megaavr:nona4809` FQBN defaults to `mode=on`, which enables ATmega328 register emulation. Arduino's megaAVR core warns that this may have speed implications.

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

## Firmware Size Baseline

With `arduino:megaavr:nona4809:mode=off`, imported v0.99 builds at:

```text
Sketch uses 31689 bytes (64%) of program storage space. Maximum is 49152 bytes.
Global variables use 1323 bytes (21%) of dynamic memory, leaving 4821 bytes for local variables. Maximum is 6144 bytes.
```

