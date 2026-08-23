ARDUINO_CLI ?= arduino-cli
ARDUINO_CONFIG ?= arduino-cli.yaml
FQBN ?= arduino:megaavr:nona4809:mode=off
SKETCH ?= firmware/idum
ALT_ENGINE ?= superlooper
ifeq ($(ALT_ENGINE),none)
ALT_ENGINE_ID := 0
else ifeq ($(ALT_ENGINE),superlooper)
ALT_ENGINE_ID := 1
else
$(error Unknown ALT_ENGINE '$(ALT_ENGINE)'; expected none or superlooper)
endif
ENGINE_DEFINE := -DIDUM_ALT_ENGINE=$(ALT_ENGINE_ID)
FIRMWARE_DEFINES ?=
BUILD_PATH ?= build/idum-$(ALT_ENGINE)
PORT ?=
SIM_CXX ?= c++
SIM_FIRMWARE ?= firmware/idum
SIM_MAIN ?= idum.ino
SIM_SDKROOT := $(shell xcrun --show-sdk-path 2>/dev/null)
SIM_LIBCXX := $(SIM_SDKROOT)/usr/include/c++/v1
SIM_PLATFORM_FLAGS := $(if $(wildcard $(SIM_LIBCXX)/cstdint),-isystem $(SIM_LIBCXX))
SIM_CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wno-sign-compare
SIM_DEFINES ?= -DIDUM_SIM_HAS_ADC_DIAGNOSTIC=1 -DIDUM_SIM_HAS_RNG_SEEDING=1 -DIDUM_SIM_HAS_ATOMIC_LOOP_RECORD=1 -DIDUM_SIM_HAS_BOOT_RESET=1 -DIDUM_SIM_CONST_BREAK_TABLE=1
SIM_BUILD := build/sim/idum-sim-$(ALT_ENGINE)

.PHONY: help doctor setup require-alternate-engine build superlooper-build upload safe-io-build safe-io-upload adc-diagnostic-build adc-diagnostic-upload clean board-list sim sim-compile sim-modes sim-alternate-modes sim-regressions sim-stock-parity test-stock-reference test-superlooper test-all sim-clock-stop sim-break-patterns sim-break-playback sim-id00m-regressions sim-clean

help:
	@echo "IDUM firmware build targets"
	@echo
	@echo "  make setup                Install/update the Arduino megaAVR core"
	@echo "  make doctor               Check local command-line toolchain"
	@echo "  make build                Compile firmware/idum for $(FQBN)"
	@echo "  make superlooper-build    Compile production firmware (CYCLE selects Superlooper)"
	@echo "  make safe-io-build        Compile firmware using normal Arduino pin I/O"
	@echo "  make safe-io-upload PORT=<port> Upload normal Arduino pin I/O firmware"
	@echo "  make adc-diagnostic-build Compile USB ADC diagnostic firmware"
	@echo "  make adc-diagnostic-upload PORT=<port> Upload USB ADC diagnostic firmware"
	@echo "  make sim                  Build and run the host-side simulator smoke test"
	@echo "  make sim-modes            Build and run simulator scenarios for all modes"
	@echo "  make sim-alternate-modes  Run every mode through the selected alternate runner"
	@echo "  make sim-stock-parity     Compare exact stock-runner output across firmware personalities"
	@echo "  make test-stock-reference Run the no-alternate reference regression battery"
	@echo "  make test-superlooper     Confirm stock parity, then run the Superlooper battery"
	@echo "  make test-all             Run both firmware-personality contracts"
	@echo "  make sim-regressions      Run all faithful bug-fix regressions against this fork"
	@echo "  make sim-clock-stop       Run the clock-stop generated-repeat regression"
	@echo "  make sim-break-patterns   Print the Break mode preset rhythm table"
	@echo "  make sim-break-playback   Recreate Break mode patterns through simulated I/O"
	@echo "  make sim-id00m-regressions ID00M_ROOT=/path/to/id00m  Compare stock, current, and ID00M fixes"
	@echo "  make upload PORT=<port>   Compile and upload to a connected module"
	@echo "  make board-list           List detected serial ports/boards"
	@echo "  make clean                Remove build output"
	@echo
	@echo "Variables:"
	@echo "  ARDUINO_CONFIG=$(ARDUINO_CONFIG)"
	@echo "  FQBN=$(FQBN)"
	@echo "  SKETCH=$(SKETCH)"
	@echo "  BUILD_PATH=$(BUILD_PATH)"
	@echo "  ALT_ENGINE=$(ALT_ENGINE)"
	@echo "  FIRMWARE_DEFINES=$(FIRMWARE_DEFINES)"

doctor:
	scripts/doctor.sh

setup:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core update-index
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) core install arduino:megaavr

require-alternate-engine:
	@if [ "$(ALT_ENGINE)" = "none" ]; then echo "ALT_ENGINE=none is simulator/reference-only; production firmware always assigns CYCLE to an alternate engine"; exit 2; fi

build: require-alternate-engine
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --fqbn $(FQBN) --build-path $(BUILD_PATH) --build-property compiler.cpp.extra_flags="$(ENGINE_DEFINE) $(FIRMWARE_DEFINES)" $(SKETCH)

superlooper-build:
	$(MAKE) build ALT_ENGINE=superlooper

upload:
	@if [ -z "$(PORT)" ]; then echo "Set PORT, for example: make upload PORT=/dev/cu.usbmodem101"; exit 2; fi
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_PATH) $(SKETCH)

safe-io-build: require-alternate-engine
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --fqbn $(FQBN) --build-path $(BUILD_PATH)-safe-io --build-property compiler.cpp.extra_flags="$(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_DISABLE_FAST_IO=1" $(SKETCH)

safe-io-upload:
	@if [ -z "$(PORT)" ]; then echo "Set PORT, for example: make safe-io-upload PORT=/dev/cu.usbmodem101"; exit 2; fi
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_PATH)-safe-io $(SKETCH)

adc-diagnostic-build: require-alternate-engine
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) compile --fqbn $(FQBN) --build-path $(BUILD_PATH)-adc --build-property compiler.cpp.extra_flags="$(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_ADC_DIAGNOSTIC=1" $(SKETCH)

adc-diagnostic-upload:
	@if [ -z "$(PORT)" ]; then echo "Set PORT, for example: make adc-diagnostic-upload PORT=/dev/cu.usbmodem101"; exit 2; fi
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_PATH)-adc $(SKETCH)

board-list:
	$(ARDUINO_CLI) --config-file $(ARDUINO_CONFIG) board list

sim:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) smoke

sim-compile:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)

sim-modes:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) all-modes

sim-alternate-modes:
	$(MAKE) sim-alternate-modes-run ALT_ENGINE=superlooper

.PHONY: sim-alternate-modes-run
sim-alternate-modes-run:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) all-alternate-modes

sim-stock-parity:
	$(MAKE) sim-compile ALT_ENGINE=none
	$(MAKE) sim-compile ALT_ENGINE=superlooper
	scripts/compare-stock-engine-fingerprint.sh

test-stock-reference:
	$(MAKE) sim-regressions ALT_ENGINE=none
	$(MAKE) sim-modes ALT_ENGINE=none

test-superlooper:
	$(MAKE) sim-stock-parity
	$(MAKE) sim-compile ALT_ENGINE=superlooper
	build/sim/idum-sim-superlooper all-superlooper-regressions

test-all:
	$(MAKE) test-stock-reference
	$(MAKE) test-superlooper

sim-regressions:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) all-regressions

sim-clock-stop:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) clock-stop-burst

sim-break-patterns:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) break-patterns

sim-break-playback:
	mkdir -p build/sim
	$(SIM_CXX) $(SIM_CXXFLAGS) $(SIM_PLATFORM_FLAGS) $(SIM_DEFINES) $(ENGINE_DEFINE) $(FIRMWARE_DEFINES) -DIDUM_SIM_MAIN='"$(SIM_MAIN)"' -Isim -I$(SIM_FIRMWARE) sim/idum_sim.cpp -o $(SIM_BUILD)
	$(SIM_BUILD) break-playback

sim-id00m-regressions:
	@if [ -z "$(ID00M_ROOT)" ]; then echo "Set ID00M_ROOT to the extracted ID00M source repository"; exit 2; fi
	scripts/compare-id00m-regressions.sh "$(ID00M_ROOT)"

clean:
	rm -rf build

sim-clean:
	rm -rf build/sim
