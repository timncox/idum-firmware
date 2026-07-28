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
arduino:megaavr:nona4809
```

The plain Nano Every FQBN defaults to `mode=on`, enabling ATmega328 register emulation. IDUM's firmware pin numbers appear to depend on this mapping.

Do not use `arduino:megaavr:nona4809:mode=off` for normal IDUM builds unless the pin map is audited and updated. It disables ATmega328 register emulation and can make the panel controls read the wrong pins.

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

With `arduino:megaavr:nona4809`, imported v0.99 builds at:

```text
Sketch uses 31689 bytes (64%) of program storage space. Maximum is 49152 bytes.
Global variables use 1323 bytes (21%) of dynamic memory, leaving 4821 bytes for local variables. Maximum is 6144 bytes.
```
