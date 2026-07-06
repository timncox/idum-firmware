ARDUINO_CLI ?= arduino-cli
ARDUINO_CONFIG ?= arduino-cli.yaml
FQBN ?= arduino:megaavr:nona4809
SKETCH ?= firmware/idum
BUILD_PATH ?= build/idum
PORT ?=

.PHONY: help doctor setup build upload clean board-list

help:
	@echo "IDUM firmware build targets"
	@echo
	@echo "  make setup                Install/update the Arduino megaAVR core"
	@echo "  make doctor               Check local command-line toolchain"
	@echo "  make build                Compile firmware/idum for $(FQBN)"
	@echo "  make upload PORT=<port>   Compile and upload to a connected module"
	@echo "  make board-list           List detected serial ports/boards"
	@echo "  make clean                Remove build output"
	@echo
	@echo "Variables:"
	@echo "  ARDUINO_CONFIG=$(ARDUINO_CONFIG)"
	@echo "  FQBN=$(FQBN)"
	@echo "  SKETCH=$(SKETCH)"
	@echo "  BUILD_PATH=$(BUILD_PATH)"

doctor:
	scripts/doctor.sh

setup:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core update-index
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core install arduino:megaavr

build:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --fqbn $(FQBN) --build-path $(BUILD_PATH) $(SKETCH)

upload:
	@if [ -z "$(PORT)" ]; then echo "Set PORT, for example: make upload PORT=/dev/cu.usbmodem101"; exit 2; fi
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_PATH) $(SKETCH)

board-list:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) board list

clean:
	rm -rf build
