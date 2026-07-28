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

This targets the observed Arduino Nano Every / ATmega4809 controller. Use the default ATmega328 register-emulation mapping; IDUM's firmware pin numbers depend on it.

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
make build FQBN=arduino:megaavr:nona4809:mode=off
make upload FQBN=arduino:megaavr:nona4809:mode=off PORT=/dev/cu.usbmodem101
```
