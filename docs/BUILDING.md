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

The build first composes a generated Arduino sketch at
`build/staged/superlooper/idum/`: it copies the stock/shared tabs from
`firmware/idum/`, then overlays the selected source from
`firmware/engines/superlooper/`. This staging step preserves Arduino's required
single-sketch layout while keeping alternate engines separate in the source
tree.

The Arduino compiler's intermediate remains under `build/idum-superlooper/`.
The flashable release file is copied to
`build/release/alt-dum-superlooper-v1.0.0.hex`. Override the release version for
a development build without renaming the sketch:

```sh
make build FIRMWARE_VERSION=1.0.1-dev
```

That example produces `build/release/alt-dum-superlooper-v1.0.1-dev.hex`; the
default remains `build/release/alt-dum-superlooper-v1.0.0.hex`. The basename
includes the selected engine so later Alt-DUM engine builds remain
distinguishable.

Default target:

```sh
FQBN=arduino:megaavr:nona4809:mode=off
```

This targets the observed Arduino Nano Every / ATmega4809 controller with register emulation set to `None`.

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
make build FQBN=arduino:megaavr:nona4809
make upload FQBN=arduino:megaavr:nona4809 PORT=/dev/cu.usbmodem101
```
