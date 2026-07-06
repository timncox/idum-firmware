# Building and Uploading

This repo is designed for `arduino-cli` so users do not need to open the Arduino IDE.

## Install Arduino CLI

On macOS with Homebrew:

```sh
brew install arduino-cli
```

Or follow Arduino's official install instructions:

https://arduino.github.io/arduino-cli/latest/installation/

Then verify the CLI is visible:

```sh
make doctor
```

## Install Board Core

```sh
make setup
```

This installs the Arduino megaAVR core used by the default board target. Arduino CLI state is configured in `arduino-cli.yaml` to live under `.arduino/`, which is ignored by git.

## Compile

```sh
make build
```

Default target:

```sh
FQBN=arduino:megaavr:nona4809
```

The default is an informed starting point, not yet a hardware-confirmed fact. The source uses the Nano analog pin numbering pattern (`14..21`) and v.99 attaches interrupts on several trigger pins, which is compatible with a Nano Every/ATmega4809-style target in a way a classic ATmega328P Nano is not.

## Upload

Connect the module over USB, then list detected boards:

```sh
make board-list
```

Upload using the detected port:

```sh
make upload PORT=/dev/cu.usbmodem101
```

If the bootloader/board is different, override `FQBN`:

```sh
make build FQBN=arduino:avr:nano
make upload FQBN=arduino:avr:nano PORT=/dev/cu.usbserial-110
```
