# Building and Uploading

**This repository provides source only; no prebuilt firmware download is provided.**
The commands below compile the source in your checkout.

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

Output version names are labels, not source checkouts or published releases.
Rebuilding an older filename from current source does not reproduce older
firmware. Record the source commit when sharing a local build; the boot
animation identifies the firmware family, not the exact source revision.

The explicit Superlooper target is `make superlooper-build`. The shell wrapper
also accepts a label: `FIRMWARE_VERSION=1.0.1-dev scripts/build.sh`.

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

## Separate development build directories

To keep a local experiment's output separate:

```sh
make test-superlooper
make build FIRMWARE_VERSION=dev BUILD_PATH=build/idum-superlooper-dev
make board-list
make upload BUILD_PATH=build/idum-superlooper-dev PORT=<detected-port>
```

Use the Nano Every port reported by `make board-list`.

## Clock-stop behavior

Generated repeats normally stop after the external clock disappears. To
restore v0.99's unbounded behavior for a custom build:

```sh
make build FIRMWARE_DEFINES=-DIDUM_ENABLE_CLOCK_STOP_TIMEOUT=0
```

For temporary ADC and Ball timing images, see [hardware diagnostics](DIAGNOSTICS.md).
