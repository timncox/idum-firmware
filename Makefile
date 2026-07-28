ARDUINO_CLI ?= arduino-cli
ARDUINO_CONFIG ?= arduino-cli.yaml
FQBN ?= arduino:megaavr:nona4809:mode=off
SKETCH ?= firmware/idum
BUILD_PATH ?= build/idum
PORT ?=

.PHONY: help doctor setup build upload clean board-list sim sim-modes sim-clock-stop sim-break-patterns sim-break-playback sim-clean

help:
	@echo "IDUM firmware build targets"
	@echo
	@echo "  make setup                Install/update the Arduino megaAVR core"
	@echo "  make doctor               Check local command-line toolchain"
	@echo "  make build                Compile firmware/idum for $(FQBN)"
	@echo "  make sim                  Build and run the host-side simulator smoke test"
	@echo "  make sim-modes            Build and run simulator scenarios for all modes"
	@echo "  make sim-clock-stop       Run the clock-stop generated-repeat regression"
	@echo "  make sim-break-patterns   Print the Break mode preset rhythm table"
	@echo "  make sim-break-playback   Recreate Break mode patterns through simulated I/O"
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

sim:
	mkdir -p build/sim
	c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare -Isim -Ifirmware/idum sim/idum_sim.cpp -o build/sim/idum-sim
	build/sim/idum-sim smoke

sim-modes:
	mkdir -p build/sim
	c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare -Isim -Ifirmware/idum sim/idum_sim.cpp -o build/sim/idum-sim
	build/sim/idum-sim all-modes

sim-clock-stop:
	mkdir -p build/sim
	c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare -Isim -Ifirmware/idum sim/idum_sim.cpp -o build/sim/idum-sim
	build/sim/idum-sim clock-stop-burst

sim-break-patterns:
	mkdir -p build/sim
	c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare -Isim -Ifirmware/idum sim/idum_sim.cpp -o build/sim/idum-sim
	build/sim/idum-sim break-patterns

sim-break-playback:
	mkdir -p build/sim
	c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare -Isim -Ifirmware/idum sim/idum_sim.cpp -o build/sim/idum-sim
	build/sim/idum-sim break-playback

clean:
	rm -rf build

sim-clean:
	rm -rf build/sim
