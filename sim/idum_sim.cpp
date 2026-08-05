#define IDUM_SIM 1
#define ARDUINO 100

#include "Arduino.h"
#include "EEPROM.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <functional>
#include <string>
#include <vector>

SerialStub Serial;
EEPROMClass EEPROM;

void setup();
void loop();
void adcDiagnosticLoop();
void modifyTest();
void activateModification();
void decideModificationStatus();
void calculateModeBoundaries();
unsigned int selectCurrentMode();
void loopModificationHandler();
void ratchet();
void rotate();
void outputGates();
void writeModeMuxAddress(unsigned int muxAddress);
void writeModeLed(unsigned int ledIndex);
void displayFirmwareVersion(unsigned int firmwareNumber);
#ifdef IDUM_SIM_CONST_BREAK_TABLE
extern const int breakBeat1[16][4][16];
#else
extern int breakBeat1[16][4][16];
#endif
extern bool analogClock;
#ifdef IDUM_SIM_HAS_LOOP_MODULATION
extern bool fastExternalSequencerClock;
extern bool loopModulationMode;
#endif
extern bool trigHistory[4][8][4];
extern unsigned int historyIndex;
extern unsigned int mode;
extern unsigned int modeDialPosition;
extern unsigned int modePosition;
extern bool activeModes[8];
extern bool clockEdge;
extern bool loopEnable;
extern bool loopIndexChanged;
extern bool menuMode;
extern bool modificationActive;
extern bool splitMode;
extern bool startModification;
extern bool endModification;
extern bool carryOver;
extern bool cycleIn;
extern bool trig1In;
extern bool trig2In;
extern bool trig3In;
extern bool trig4In;
extern bool trig1State;
extern bool trig2State;
extern bool trig3State;
extern bool trig4State;
extern bool trig1Choke;
extern bool trig2Choke;
extern bool trig3Choke;
extern bool trig4Choke;
extern unsigned int intervalCount;
extern unsigned int lengthScaled;
extern unsigned int loopIndex;
extern unsigned int originalHistoryIndex;
extern unsigned int loopRandom;
extern unsigned int loopRandom1;
extern unsigned int loopRandom2;
extern unsigned int loopRandom3;
extern unsigned int loopRandom4;
extern unsigned int modeRead;
extern unsigned int mode1;
extern unsigned int mode2;
extern unsigned int mode3;
extern unsigned int mode4;
extern unsigned int modifyLength;
extern unsigned int modifyLength1;
extern unsigned int modifyLength2;
extern unsigned int modifyLength3;
extern unsigned int modifyLength4;
extern unsigned int originalModifyLength;
extern unsigned int paramHistory[5][8][4];
extern unsigned int paramResolution;
extern unsigned int probRead;
extern unsigned int probabilityModifier;
extern unsigned int ratchetAmount;
extern unsigned int ratchetAmount1;
extern unsigned int trig3Divide;
extern unsigned int unscaledParam1;
extern unsigned int unscaledParam2;
extern unsigned int unscaledParam3;
extern unsigned int unscaledParam4;
extern unsigned long lastTrig1Interval;
#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_HAS_RNG_SEEDING)
extern bool rngSeeded;
#endif

namespace {

struct PinWrite {
  unsigned long time;
  uint8_t pin;
  uint8_t value;
};

unsigned long simMicros = 0;
std::array<uint8_t, 32> pinModes {};
std::array<uint8_t, 32> digitalPins {};
std::array<uint8_t, 32> lastLoggedPins {};
std::array<int, 32> analogPins {};
std::vector<PinWrite> writes;
std::string programPath;

void resetSimulation();
void printWrites();
int runScenario(const std::string& name);

int reportFirmwareRegression(const std::string& name, bool fixed, const std::string& observation) {
  std::cout << (fixed ? "FIXED " : "BUG   ") << name << ": " << observation << '\n';
  return fixed ? 0 : 1;
}

bool tracePin(uint8_t pin) {
  switch (pin) {
    case 13:
    case 7:
    case 8:
    case 6:
    case 5:
      return true;
    default:
      return false;
  }
}

const char* pinName(uint8_t pin) {
  switch (pin) {
    case 13: return "CL_OUT";
    case 7: return "TR1_OUT";
    case 8: return "TR2_OUT";
    case 6: return "TR3_OUT";
    case 5: return "TR4_OUT";
    case 15: return "CL_IN";
    case 14: return "TR1_IN";
    case 9: return "TR2_IN";
    case 11: return "TR3_IN";
    case 10: return "TR4_IN";
    default: return "PIN";
  }
}

void advanceMicros(unsigned long delta) {
  simMicros += delta;
}

void setLogicalGate(uint8_t pin, bool high) {
  digitalPins[pin] = high ? LOW : HIGH;
}

void setLogicalAnalog(uint8_t pin, int value) {
  value = std::max(0, std::min(1024, value));
  analogPins[pin] = 1024 - value;
}

void runFor(unsigned long durationMicros, unsigned long stepMicros = 100) {
  const unsigned long end = simMicros + durationMicros;
  while (simMicros < end) {
    loop();
    advanceMicros(stepMicros);
  }
}

void runUntil(unsigned long targetMicros, unsigned long stepMicros = 100) {
  while (simMicros < targetMicros) {
    loop();
    advanceMicros(stepMicros);
  }
}

void pulseGate(uint8_t pin, unsigned long highMicros = 5000, unsigned long stepMicros = 100) {
  setLogicalGate(pin, true);
  runFor(highMicros, stepMicros);
  setLogicalGate(pin, false);
  runFor(highMicros, stepMicros);
}

void pulseTriggerInputs(unsigned long highMicros = 5000, unsigned long stepMicros = 100) {
  setLogicalGate(14, true);
  setLogicalGate(9, true);
  setLogicalGate(11, true);
  setLogicalGate(10, true);
  runFor(highMicros, stepMicros);
  setLogicalGate(14, false);
  setLogicalGate(9, false);
  setLogicalGate(11, false);
  setLogicalGate(10, false);
  runFor(highMicros, stepMicros);
}

int countOutputOnsets(uint8_t pin) {
  return static_cast<int>(std::count_if(writes.begin(), writes.end(), [pin](const PinWrite& write) {
    return write.pin == pin && write.value == LOW;
  }));
}

bool hasWrite(uint8_t pin, uint8_t value) {
  return std::any_of(writes.begin(), writes.end(), [pin, value](const PinWrite& write) {
    return write.pin == pin && write.value == value;
  });
}

bool hasWriteAfter(uint8_t pin, uint8_t value, unsigned long time) {
  return std::any_of(writes.begin(), writes.end(), [pin, value, time](const PinWrite& write) {
    return write.pin == pin && write.value == value && write.time >= time;
  });
}

bool hasWriteInRange(uint8_t pin, uint8_t value, unsigned long start, unsigned long end) {
  return std::any_of(writes.begin(), writes.end(), [pin, value, start, end](const PinWrite& write) {
    return write.pin == pin && write.value == value && write.time >= start && write.time < end;
  });
}

uint8_t outputPinForChannel(int channel) {
  static const uint8_t pins[4] = {7, 8, 6, 5};
  return pins[channel];
}

void setModeCv(unsigned int modeIndex) {
  static const int modeValues[8] = {20, 100, 300, 440, 560, 700, 900, 1000};
  setLogicalAnalog(21, modeValues[modeIndex]);
}

void configureAlwaysModify(unsigned int modeIndex, int paramValue, int lengthValue = 520) {
  setLogicalAnalog(20, 1024);
  setModeCv(modeIndex);
  setLogicalAnalog(19, paramValue);
  setLogicalAnalog(18, lengthValue);
}

void establishClockInterval(unsigned long intervalMicros = 250000) {
  pulseGate(15, 5000, 100);
  runFor(intervalMicros - 10000, 100);
  pulseGate(15, 5000, 100);
  runFor(intervalMicros - 10000, 100);
}

void establishTriggerIntervals(unsigned long intervalMicros = 250000) {
  pulseTriggerInputs(5000, 100);
  runFor(intervalMicros - 10000, 100);
  pulseTriggerInputs(5000, 100);
  runFor(intervalMicros - 10000, 100);
}

void runClockTrain(unsigned int pulses, unsigned long intervalMicros = 250000) {
  for (unsigned int i = 0; i < pulses; i++) {
    runFor(intervalMicros - 10000, 100);
    pulseGate(15, 5000, 100);
  }
}

void pulseClockAndTriggerInputs(unsigned long highMicros = 5000, unsigned long stepMicros = 100) {
  setLogicalGate(15, true);
  setLogicalGate(14, true);
  setLogicalGate(9, true);
  setLogicalGate(11, true);
  setLogicalGate(10, true);
  runFor(highMicros, stepMicros);
  setLogicalGate(15, false);
  setLogicalGate(14, false);
  setLogicalGate(9, false);
  setLogicalGate(11, false);
  setLogicalGate(10, false);
  runFor(highMicros, stepMicros);
}

void runClockAndTriggerTrain(unsigned int pulses, unsigned long intervalMicros = 250000) {
  for (unsigned int i = 0; i < pulses; i++) {
    runFor(intervalMicros - 10000, 100);
    pulseClockAndTriggerInputs();
  }
}

int runModeScenario(
  const std::string& name,
  const std::function<bool()>& scenario,
  const std::string& passDetail,
  const std::string& failDetail
) {
  resetSimulation();
  setup();
  writes.clear();

  if (!scenario()) {
    printWrites();
    std::cerr << "FAIL " << name << ": " << failDetail << '\n';
    return 1;
  }

  std::cout << "PASS " << name << " (" << passDetail << ")\n";
  return 0;
}

void resetSimulation() {
  simMicros = 0;
  pinModes.fill(INPUT);
  digitalPins.fill(HIGH);
  lastLoggedPins.fill(0xff);
  analogPins.fill(1024);
  writes.clear();
  EEPROM.clear();
}

void printWrites() {
  for (const auto& write : writes) {
    std::cout << std::setw(10) << write.time << "us "
              << pinName(write.pin) << " <- "
              << (write.value == LOW ? "LOW(on)" : "HIGH(off)") << '\n';
  }
}

int smokeScenario() {
  resetSimulation();
  setup();
  writes.clear();

  setLogicalAnalog(20, 0);      // chance/probability: no modification
  setLogicalAnalog(21, 512);    // mode
  setLogicalAnalog(19, 512);    // param
  setLogicalAnalog(18, 0);      // length

  pulseGate(15);
  runFor(1000);
  printWrites();

  const bool sawClockOn = std::any_of(writes.begin(), writes.end(), [](const PinWrite& write) {
    return write.pin == 13 && write.value == LOW;
  });
  const bool sawClockOff = std::any_of(writes.begin(), writes.end(), [](const PinWrite& write) {
    return write.pin == 13 && write.value == HIGH;
  });

  if (!sawClockOn || !sawClockOff) {
    std::cerr << "FAIL smoke: expected clock output to pass a gate pulse\n";
    return 1;
  }

  std::cout << "PASS smoke\n";
  return 0;
}

int lengthOneRegressionScenario() {
  resetSimulation();
  setup();

  loopEnable = false;
  splitMode = false;
  menuMode = false;
  clockEdge = true;
  probRead = 1024;
  probabilityModifier = 0;
  lengthScaled = 1;
  modifyLength = 1;
  modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  originalModifyLength = 1;
  modificationActive = true;
  mode = modePosition = 5;

  modifyTest();

  const bool fixed = (modifyLength == 1) && modificationActive;
  return reportFirmwareRegression(
    "length-1-active-state",
    fixed,
    "renewed length=" + std::to_string(modifyLength)
      + ", modificationActive=" + std::to_string(modificationActive)
  );
}

int splitRotateParamRegressionScenario() {
  resetSimulation();
  setup();

  modifyLength = 0;
  modifyLength1 = modifyLength3 = modifyLength4 = 0;
  modifyLength2 = 1;
  mode2 = 4;
  unscaledParam1 = 0;  // Stock incorrectly uses this for every output.
  unscaledParam2 = 2;  // Rotation 1 routes TR4 to output 2.
  trig1In = trig2In = trig3In = false;
  trig4In = true;
  trig2State = false;

  rotate();

  return reportFirmwareRegression(
    "rotate-split-per-channel-param",
    trig2State,
    std::string("TR2 output state=") + (trig2State ? "on (followed channel 2 PARAM)" : "off (followed channel 1 PARAM)")
  );
}

int rotateSilentDestinationRegressionScenario() {
  resetSimulation();
  setup();

  modifyLength = 0;
  modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 1;
  mode1 = mode2 = mode3 = mode4 = 4;
  unscaledParam1 = unscaledParam2 = unscaledParam3 = unscaledParam4 = 2;
  trig1In = trig2In = trig4In = false;
  trig3In = true;
  trig1Choke = trig2Choke = trig4Choke = false;
  trig3Choke = true;
  trig1State = false;

  writes.clear();
  for (int i = 0; i < 4; ++i) {
    rotate();
    outputGates();
    advanceMicros(100);
  }

  const bool routedOn = hasWrite(7, LOW);
  const bool flickeredOff = hasWrite(7, HIGH);
  const bool fixed = routedOn && !flickeredOff;
  return reportFirmwareRegression(
    "rotate-carries-source-choke",
    fixed,
    "held routed gate=" + std::to_string(routedOn) + ", flickered off=" + std::to_string(flickeredOff)
  );
}

int ballChannelThreeResetRegressionScenario() {
  resetSimulation();
  setup();

  splitMode = false;
  carryOver = false;
  startModification = true;
  mode = 3;
  lengthScaled = 2;
  trig1In = trig2In = trig3In = trig4In = false;
  trig3Divide = 123;

  activateModification();

  const unsigned int resetValue = static_cast<unsigned int>(-1);
  return reportFirmwareRegression(
    "ball-channel-3-reset",
    trig3Divide == resetValue,
    "TR3 divider after activation=" + std::to_string(trig3Divide)
  );
}

int backToBackModeRegressionScenario() {
  resetSimulation();
  setup();

  splitMode = false;
  loopEnable = false;
  menuMode = false;
  cycleIn = false;
  modificationActive = true;
  mode = 1;
  modePosition = 5;
  modeRead = 700;
  startModification = true;
  endModification = true;
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  originalModifyLength = 1;

  decideModificationStatus();

  return reportFirmwareRegression(
    "back-to-back-current-mode",
    mode == 5,
    "new dial mode=5, activated mode=" + std::to_string(mode)
  );
}

int removedModeBootRegressionScenario() {
  resetSimulation();
  setup();

  for (bool &activeMode : activeModes) activeMode = false;
  activeModes[2] = true;
  activeModes[6] = true;
  mode = modePosition = 0;  // Simulate booting on removed HOLD.
  modeRead = 1;             // Knob remains at the bottom of its travel.
  modificationActive = false;
  menuMode = false;

  calculateModeBoundaries();
  selectCurrentMode();

  const bool fixed = (intervalCount == 2) && (mode == 2);
  return reportFirmwareRegression(
    "removed-mode-power-on",
    fixed,
    "active interval count=" + std::to_string(intervalCount) + ", selected mode=" + std::to_string(mode)
  );
}

void prepareLoopRegression(bool split) {
  for (auto &channel : paramHistory) {
    for (auto &step : channel) {
      for (unsigned int &value : step) value = 0;
    }
  }

  loopEnable = true;
  splitMode = split;
  loopIndexChanged = true;
  menuMode = false;
  probRead = 1024;
  loopRandom = loopRandom1 = loopRandom2 = loopRandom3 = loopRandom4 = 0;
  paramResolution = 0;
}

int splitLoopEmptyStepRegressionScenario() {
  resetSimulation();
  setup();
  prepareLoopRegression(true);

  loopIndex = originalHistoryIndex = 0;
  mode1 = 6;
  paramHistory[1][0][0] = 4;
  paramHistory[1][0][2] = 0;  // No modification was recorded on this step.

  loopModificationHandler();

  return reportFirmwareRegression(
    "split-loop-empty-step",
    mode1 == 6,
    "mode before empty step=6, mode afterward=" + std::to_string(mode1)
  );
}

int loopPhaseAlignmentRegressionScenario() {
  resetSimulation();
  setup();
  prepareLoopRegression(false);

  loopIndex = 1;
  originalHistoryIndex = 3;
  paramHistory[0][1][0] = 2;  // Wrong relative slot used by stock.
  paramHistory[0][1][2] = 3;
  paramHistory[0][4][0] = 6;  // Correct absolute slot: (1 + 3) % 8.
  paramHistory[0][4][2] = 3;

  loopModificationHandler();

  return reportFirmwareRegression(
    "loop-modification-phase-alignment",
    mode == 6,
    "expected absolute slot mode=6, loaded mode=" + std::to_string(mode)
  );
}

int loopRatchetMappingRegressionScenario() {
  resetSimulation();
  setup();
  prepareLoopRegression(false);

  loopIndex = originalHistoryIndex = 0;
  paramHistory[0][0][0] = 2;
  paramHistory[0][0][1] = 0;
  paramHistory[0][0][2] = 3;

  loopModificationHandler();

  const unsigned int expected = 8;  // Live odd[] mapping at full counter-clockwise.
  return reportFirmwareRegression(
    "loop-ratchet-param-mapping",
    ratchetAmount == expected,
    "live mapping=8, playback mapping=" + std::to_string(ratchetAmount)
  );
}

int ratchetZeroIntervalRegressionScenario() {
  resetSimulation();
  setup();

  modifyLength = 0;
  modifyLength1 = 1;
  modifyLength2 = modifyLength3 = modifyLength4 = 0;
  mode1 = 2;
  unscaledParam1 = 9;
  ratchetAmount1 = 8;
  lastTrig1Interval = 0;
  trig1State = true;

  std::cout << "RUN   ratchet-zero-interval: invoking ratchet with no measured trigger interval\n";
  ratchet();  // Stock terminates with SIGFPE here; fixed builds return safely.

  return reportFirmwareRegression(
    "ratchet-zero-interval",
    !trig1State,
    std::string("returned safely, TR1 output state=") + (trig1State ? "on" : "off")
  );
}

std::array<long, 6> randomSequenceAfterFirstClock(unsigned long idleMicros) {
  resetSimulation();
  std::srand(1);  // Model the deterministic C/Arduino RNG state at power-on.
#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_HAS_RNG_SEEDING)
  rngSeeded = false;
#endif
  setup();
  runFor(idleMicros);
  pulseGate(15, 5000, 100);

  std::array<long, 6> sequence {};
  for (long &value : sequence) value = random(1000000);
  return sequence;
}

int rngPowerOnRegressionScenario() {
  const auto earlyClock = randomSequenceAfterFirstClock(10000);
  const auto lateClock = randomSequenceAfterFirstClock(137000);
  const bool fixed = earlyClock != lateClock;

  return reportFirmwareRegression(
    "rng-varies-with-first-clock",
    fixed,
    "first values with early/late first clock=" + std::to_string(earlyClock[0]) + "/" + std::to_string(lateClock[0])
  );
}

int ratchetScenario() {
  resetSimulation();
  setup();
  writes.clear();

  setLogicalAnalog(20, 1024);   // chance/probability: always modify
  setLogicalAnalog(21, 300);    // mode 2: ratchet / multiply-divide
  setLogicalAnalog(19, 850);    // right side of param: multiply trigger rate
  setLogicalAnalog(18, 520);    // medium modification length

  // Establish nonzero trigger intervals. In non-split mode ratchet activates
  // all four trigger channels, and the firmware assumes each channel has a
  // previous trigger interval.
  pulseTriggerInputs(5000, 100);
  runFor(245000);
  pulseTriggerInputs(5000, 100);
  runFor(245000);

  writes.clear();

  // Start a guaranteed modification on the next clock, then send one TR1 event
  // during the active window. Ratchet should generate extra TR1 output pulses.
  pulseGate(15, 5000, 100);
  runFor(20000);
  pulseTriggerInputs(5000, 100);
  runFor(800000);

  const int tr1Onsets = countOutputOnsets(7);
  if (tr1Onsets < 2) {
    printWrites();
    std::cerr << "FAIL ratchet: expected multiple TR1 output onsets, saw "
              << tr1Onsets << '\n';
    return 1;
  }

  std::cout << "PASS ratchet (" << tr1Onsets << " TR1 output onsets)\n";
  return 0;
}

int holdScenario() {
  return runModeScenario("hold", [] {
    establishClockInterval();
    configureAlwaysModify(0, 900, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseGate(14, 5000, 100);
    runFor(700000);

    return hasWrite(7, LOW) && digitalPins[7] == LOW;
  }, "TR1 gate remains held after input falls", "expected TR1 output to remain held beyond the input trigger");
}

int clockRatchetScenario() {
  return runModeScenario("clock-ratchet", [] {
    establishClockInterval();
    establishTriggerIntervals();
    configureAlwaysModify(1, 850, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseTriggerInputs(5000, 100);
    runFor(700000);

    return hasWrite(7, LOW) && hasWrite(8, LOW) && hasWrite(6, LOW) && hasWrite(5, LOW);
  }, "burst mode gates trigger outputs from the clock-based repeat engine", "expected trigger output activity from clock-based burst mode");
}

int clockRatchetDivideScenario() {
  return runModeScenario("clock-ratchet-divide", [] {
    establishClockInterval();
    establishTriggerIntervals();
    configureAlwaysModify(1, 0, 1024);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseTriggerInputs(5000, 100);
    runClockTrain(10);

    return countOutputOnsets(7) >= 2;
  }, "left-side PARAM produces divided clock-based repeats with a long enough window", "expected divided burst output after the input trigger");
}

int clockRatchetSparseMinLengthScenario() {
  return runModeScenario("clock-ratchet-sparse-min-length", [] {
    establishClockInterval();
    configureAlwaysModify(1, 1024, 0);

    writes.clear();
    for (unsigned int i = 0; i < 40; i++) {
      runFor(240000, 100);
      if (i == 4) {
        pulseClockAndTriggerInputs();
      } else {
        pulseGate(15, 5000, 100);
      }
    }

    const unsigned long noTriggerAfter = 3000000;
    return !hasWriteAfter(7, LOW, noTriggerAfter);
  }, "minimum-length burst stops after a sparse input trigger", "expected no TR1 bursts long after the only sparse input trigger");
}

int triggerRatchetScenario() {
  return ratchetScenario();
}

int triggerRatchetDivideScenario() {
  return runModeScenario("trigger-ratchet-divide", [] {
    establishClockInterval();
    establishTriggerIntervals();
    configureAlwaysModify(2, 0, 1024);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseTriggerInputs(5000, 100);
    runClockTrain(10);

    return countOutputOnsets(7) >= 2;
  }, "left-side PARAM produces divided trigger-based repeats with a long enough window", "expected divided ratchet output after the input trigger");
}

int triggerRatchetDivideSteadyScenario() {
  return runModeScenario("trigger-ratchet-divide-steady", [] {
    establishClockInterval();
    establishTriggerIntervals();
    configureAlwaysModify(2, 0, 1024);

    pulseGate(15, 5000, 100);
    runFor(20000);
    writes.clear();
    runClockAndTriggerTrain(8);

    return countOutputOnsets(7) != 8;
  }, "left-side PARAM changes a steady trigger stream", "expected divided ratchet mode to alter, not merely pass through, eight steady input triggers");
}

#ifdef IDUM_SIM_HAS_LOOP_MODULATION
int loopModulationModeScenario() {
  return runModeScenario("loop-modulation-mode", [] {
    setModeCv(1);
    runFor(20000);
    const unsigned int latchedMode = mode;

    setLogicalGate(17, true);
    runFor(20000);

    setModeCv(5);
    runFor(20000);

    const bool modeStayedLatched = (mode == latchedMode) && (modePosition == latchedMode);
    const bool dialStillTracksControl = (modeDialPosition == 5);
    const bool toggleRead = loopModulationMode;
    const bool fastExternalClockDefault = fastExternalSequencerClock;

    pulseGate(15, 5000, 100);
    pulseGate(14, 5000, 100);
    const bool recordingContinues = trigHistory[0][historyIndex][0];

    return modeStayedLatched && dialStillTracksControl && toggleRead && fastExternalClockDefault && recordingContinues;
  }, "front toggle gates mode mutation without freezing loop capture", "expected mode to latch, raw mode control to track, and rolling capture to continue");
}
#endif

int ballScenario() {
  return runModeScenario("ball", [] {
    establishClockInterval();
    configureAlwaysModify(3, 850, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseGate(14, 5000, 100);
    runFor(400000, 100);

    return countOutputOnsets(7) >= 2;
  }, "bouncing-ball mode produces repeated TR1 pulses", "expected multiple TR1 onsets from bouncing-ball mode");
}

int rotateScenario() {
  return runModeScenario("rotate", [] {
    establishClockInterval();
    configureAlwaysModify(4, 200, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseGate(14, 5000, 100); // Open TR1's choke before routing TR3 into it.
    runFor(10000);
    pulseGate(11, 5000, 100); // TR3 input should appear on TR1 output for rotation case 1.
    runFor(20000);

    return countOutputOnsets(7) >= 1;
  }, "TR3 input is routed to TR1 output", "expected TR3 input to appear on TR1 output");
}

int delayScenario() {
  return runModeScenario("delay", [] {
    establishClockInterval();
    establishTriggerIntervals();
    configureAlwaysModify(5, 300, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    const unsigned long triggerStart = simMicros;
    pulseTriggerInputs(5000, 100);
    runFor(200000);

    return hasWriteAfter(7, LOW, triggerStart + 10000);
  }, "TR1 output is delayed after input trigger", "expected delayed TR1 output after trigger input");
}

int breakScenario() {
  return runModeScenario("break", [] {
    establishClockInterval();
    configureAlwaysModify(6, 700, 520);

    pulseGate(15, 5000, 100);
    runFor(20000);
    pulseTriggerInputs(5000, 100);

    for (int i = 0; i < 6; ++i) {
      runFor(120000);
      pulseGate(15, 5000, 100);
    }

    return countOutputOnsets(7) >= 2 || countOutputOnsets(8) >= 2 || countOutputOnsets(6) >= 2 || countOutputOnsets(5) >= 2;
  }, "break mode emits preset rhythm activity", "expected repeated trigger output from break mode");
}

int clockStopBurstScenario() {
  resetSimulation();
  setup();
  writes.clear();

  establishClockInterval();
  configureAlwaysModify(1, 850, 1024);

  pulseGate(15, 5000, 100);
  runFor(20000);
  pulseTriggerInputs(5000, 100);

  const unsigned long stoppedClockTime = simMicros;

  runFor(700000);
  writes.clear();
  runFor(300000);

  const int tr1Onsets = countOutputOnsets(7);
  const int tr2Onsets = countOutputOnsets(8);
  const int tr3Onsets = countOutputOnsets(6);
  const int tr4Onsets = countOutputOnsets(5);

  std::cout << "clock stopped at " << stoppedClockTime << "us\n";
  std::cout << "generated output onsets after clock timeout: "
            << "TR1=" << tr1Onsets << ' '
            << "TR2=" << tr2Onsets << ' '
            << "TR3=" << tr3Onsets << ' '
            << "TR4=" << tr4Onsets << '\n';

  if (tr1Onsets == 0 && tr2Onsets == 0 && tr3Onsets == 0 && tr4Onsets == 0) {
    std::cout << "PASS clock-stop-burst\n";
    return 0;
  }

  printWrites();
  std::cerr << "FAIL clock-stop-burst: generated repeats continued after clock timeout\n";
  return 1;
}

char breakCellSymbol(int value) {
  if (value == 0) return '.';
  if (value == 1) return 'x';
  if (value >= 0 && value <= 9) return static_cast<char>('0' + value);
  return '*';
}

int breakPatternsScenario() {
  static const char* channelNames[4] = {"TR1", "TR2", "TR3", "TR4"};
  bool valid = true;

  std::cout << "Break mode preset rhythms\n";
  std::cout << "Legend: . rest, x trigger, 2/4 ratchet multiplier\n";
  std::cout << "Steps:  01 02 03 04 | 05 06 07 08 | 09 10 11 12 | 13 14 15 16\n\n";

  for (int pattern = 0; pattern < 16; ++pattern) {
    std::cout << "Pattern " << std::setw(2) << pattern << '\n';
    for (int channel = 0; channel < 4; ++channel) {
      int onsets = 0;
      int ratchets = 0;

      std::cout << "  " << channelNames[channel] << "  ";
      for (int step = 0; step < 16; ++step) {
        const int cell = breakBeat1[pattern][channel][step];
        valid = valid && (cell == 0 || cell == 1 || cell == 2 || cell == 4);
        onsets += cell > 0;
        ratchets += cell > 1;
        std::cout << breakCellSymbol(cell);
        if (step == 3 || step == 7 || step == 11) std::cout << " | ";
        else if (step < 15) std::cout << ' ';
      }
      std::cout << "   on=" << onsets << " ratchet=" << ratchets << '\n';
    }
    std::cout << '\n';
  }

  if (!valid) {
    std::cerr << "FAIL break-patterns: expected table cells to contain only 0, 1, 2, or 4\n";
    return 1;
  }

  std::cout << "PASS break-patterns\n";
  return 0;
}

int paramValueForBreakPattern(int pattern) {
  return std::min(1023, (pattern * 64) + 32);
}

void setAllTriggerInputs(bool high) {
  setLogicalGate(14, high);
  setLogicalGate(9, high);
  setLogicalGate(11, high);
  setLogicalGate(10, high);
}

int breakPlaybackScenario() {
  static const char* channelNames[4] = {"TR1", "TR2", "TR3", "TR4"};
  constexpr unsigned long clockInterval = 200000;
  constexpr unsigned long halfClock = clockInterval / 2;
  constexpr unsigned long pulseWidth = 6000;
  bool allPatternsMatched = true;

  for (int pattern = 0; pattern < 16; ++pattern) {
    resetSimulation();
    setup();

    establishClockInterval(clockInterval);
    configureAlwaysModify(6, paramValueForBreakPattern(pattern), 1024);
    writes.clear();

    const unsigned long playbackStart = simMicros;
    bool observed[4][16] {};

    for (int step = 0; step < 16; ++step) {
      const unsigned long stepStart = playbackStart + (step * halfClock);
      runUntil(stepStart);

      if ((step % 2) == 0) {
        setLogicalGate(15, true);
      }
      if (step == 0) {
        setAllTriggerInputs(true);
      }

      runFor(pulseWidth);

      if ((step % 2) == 0) {
        setLogicalGate(15, false);
      }
      if (step == 0) {
        setAllTriggerInputs(false);
      }

      const unsigned long stepEnd = simMicros;
      for (int channel = 0; channel < 4; ++channel) {
        observed[channel][step] = hasWriteInRange(outputPinForChannel(channel), LOW, stepStart, stepEnd);
      }
    }

    bool patternMatched = true;
    for (int channel = 0; channel < 4; ++channel) {
      for (int step = 0; step < 16; ++step) {
        const bool expected = (breakBeat1[pattern][channel][step] > 0)
                              || ((step > 0) && ((step % 2) == 1) && (breakBeat1[pattern][channel][step - 1] > 1));
        if (observed[channel][step] != expected) {
          patternMatched = false;
        }
      }
    }

    std::cout << (patternMatched ? "PASS" : "FAIL") << " break-playback pattern "
              << std::setw(2) << pattern << '\n';

    if (!patternMatched) {
      allPatternsMatched = false;
      for (int channel = 0; channel < 4; ++channel) {
        std::cout << "  " << channelNames[channel] << " table    ";
        for (int step = 0; step < 16; ++step) {
          std::cout << breakCellSymbol(breakBeat1[pattern][channel][step]);
          if (step == 3 || step == 7 || step == 11) std::cout << " | ";
          else if (step < 15) std::cout << ' ';
        }

        std::cout << "\n      expected ";
        for (int step = 0; step < 16; ++step) {
          const bool expected = (breakBeat1[pattern][channel][step] > 0)
                                || ((step > 0) && ((step % 2) == 1) && (breakBeat1[pattern][channel][step - 1] > 1));
          std::cout << (expected ? 'x' : '.');
          if (step == 3 || step == 7 || step == 11) std::cout << " | ";
          else if (step < 15) std::cout << ' ';
        }

        std::cout << "\n      observed ";
        for (int step = 0; step < 16; ++step) {
          std::cout << (observed[channel][step] ? 'x' : '.');
          if (step == 3 || step == 7 || step == 11) std::cout << " | ";
          else if (step < 15) std::cout << ' ';
        }
        std::cout << '\n';
      }
    }
  }

  if (!allPatternsMatched) {
    std::cerr << "FAIL break-playback: simulated Break mode output did not match the embedded table\n";
    return 1;
  }

  std::cout << "PASS break-playback\n";
  return 0;
}

int skipScenario() {
  return runModeScenario("skip", [] {
    establishClockInterval();
    configureAlwaysModify(7, 100, 520);

    pulseGate(15, 5000, 100);
    runFor(50000);

    return countOutputOnsets(13) >= 2;
  }, "clock skip mode sends multiple CL_OUT pulses", "expected clock skip mode to emit multiple clock pulses");
}

int allModesScenario() {
  const std::vector<std::string> scenarios = {
    "hold",
    "clock-ratchet",
    "clock-ratchet-divide",
    "trigger-ratchet",
    "trigger-ratchet-divide",
#ifdef IDUM_SIM_HAS_LOOP_MODULATION
    "loop-modulation-mode",
#endif
    "ball",
    "rotate",
    "delay",
    "break",
    "clock-stop-burst",
    "skip",
  };

  for (const auto& scenario : scenarios) {
    const std::string command = programPath + " " + scenario;
    const int result = std::system(command.c_str());
    if (result != 0) return result;
  }

  std::cout << "PASS all-modes\n";
  return 0;
}

int allRegressionsScenario() {
  const std::vector<std::string> scenarios = {
    "regression-length-1",
    "regression-rotate-split-param",
    "regression-rotate-silent-destination",
    "regression-ball-channel-3-reset",
    "regression-back-to-back-mode",
    "regression-removed-mode-boot",
    "regression-split-loop-empty-step",
    "regression-loop-phase",
    "regression-loop-ratchet-map",
    "regression-ratchet-zero-interval",
    "regression-rng-power-on",
  };

  for (const auto& scenario : scenarios) {
    const std::string command = programPath + " " + scenario;
    const int result = std::system(command.c_str());
    if (result != 0) return result;
  }

  std::cout << "PASS all-regressions\n";
  return 0;
}

int runScenario(const std::string& name) {
  if (name == "smoke") return smokeScenario();
  if (name == "regression-length-1") return lengthOneRegressionScenario();
  if (name == "regression-rotate-split-param") return splitRotateParamRegressionScenario();
  if (name == "regression-rotate-silent-destination") return rotateSilentDestinationRegressionScenario();
  if (name == "regression-ball-channel-3-reset") return ballChannelThreeResetRegressionScenario();
  if (name == "regression-back-to-back-mode") return backToBackModeRegressionScenario();
  if (name == "regression-removed-mode-boot") return removedModeBootRegressionScenario();
  if (name == "regression-split-loop-empty-step") return splitLoopEmptyStepRegressionScenario();
  if (name == "regression-loop-phase") return loopPhaseAlignmentRegressionScenario();
  if (name == "regression-loop-ratchet-map") return loopRatchetMappingRegressionScenario();
  if (name == "regression-ratchet-zero-interval") return ratchetZeroIntervalRegressionScenario();
  if (name == "regression-rng-power-on") return rngPowerOnRegressionScenario();
  if (name == "hold") return holdScenario();
  if (name == "clock-ratchet") return clockRatchetScenario();
  if (name == "clock-ratchet-divide") return clockRatchetDivideScenario();
  if (name == "clock-ratchet-sparse-min-length") return clockRatchetSparseMinLengthScenario();
  if (name == "trigger-ratchet") return triggerRatchetScenario();
  if (name == "trigger-ratchet-divide") return triggerRatchetDivideScenario();
  if (name == "trigger-ratchet-divide-steady") return triggerRatchetDivideSteadyScenario();
#ifdef IDUM_SIM_HAS_LOOP_MODULATION
  if (name == "loop-modulation-mode") return loopModulationModeScenario();
#endif
  if (name == "ratchet") return triggerRatchetScenario();
  if (name == "ball") return ballScenario();
  if (name == "rotate") return rotateScenario();
  if (name == "delay") return delayScenario();
  if (name == "break") return breakScenario();
  if (name == "clock-stop-burst") return clockStopBurstScenario();
  if (name == "break-patterns") return breakPatternsScenario();
  if (name == "break-playback") return breakPlaybackScenario();
  if (name == "skip") return skipScenario();
  if (name == "all-modes") return allModesScenario();
  if (name == "all-regressions") return allRegressionsScenario();

  std::cerr << "Unknown scenario: " << name << '\n';
  std::cerr << "Available scenarios: smoke, regression-length-1, regression-rotate-split-param, regression-rotate-silent-destination, regression-ball-channel-3-reset, regression-back-to-back-mode, regression-removed-mode-boot, regression-split-loop-empty-step, regression-loop-phase, regression-loop-ratchet-map, regression-ratchet-zero-interval, regression-rng-power-on, hold, clock-ratchet, clock-ratchet-divide, clock-ratchet-sparse-min-length, trigger-ratchet, trigger-ratchet-divide, trigger-ratchet-divide-steady, loop-modulation-mode, ball, rotate, delay, break, clock-stop-burst, break-patterns, break-playback, skip, all-modes, all-regressions\n";
  return 2;
}

}  // namespace

unsigned long micros() {
  return simMicros;
}

unsigned long millis() {
  return simMicros / 1000;
}

void delay(unsigned long ms) {
  advanceMicros(ms * 1000);
}

void delayMicroseconds(unsigned int us) {
  advanceMicros(us);
}

void pinMode(uint8_t pin, uint8_t mode) {
  pinModes[pin] = mode;
}

int digitalRead(uint8_t pin) {
  return digitalPins[pin];
}

void digitalWrite(uint8_t pin, uint8_t value) {
  digitalPins[pin] = value;
  if (tracePin(pin) && lastLoggedPins[pin] != value) {
    lastLoggedPins[pin] = value;
    writes.push_back({simMicros, pin, value});
  }
}

int analogRead(uint8_t pin) {
  return analogPins[pin];
}

void attachInterrupt(uint8_t, void (*)(), int) {}

long random(long max) {
  if (max <= 0) return 0;
  return std::rand() % max;
}

long random(long min, long max) {
  if (max <= min) return min;
  return min + random(max - min);
}

void randomSeed(unsigned long seed) {
  std::srand(static_cast<unsigned int>(seed));
}

void noInterrupts() {}
void interrupts() {}

void assignPins();
void displayFirmwareVersion(unsigned int firmwareNumber);
void initializeEEPROM();
void initializeHardwareInterrupts();
bool slowClockTimer(void *);
bool displayTimer(void *);
void readInputs();
void readGateInputs();
void readAnalogInputs();
void scaleCVInputs();
void handleFreezeParam();
void edgeDetect();
void detectEdges();
void slowCycleHelper();
void grabLooperVariables();
void debounceModeButton();
void calculateGateVariables();
void saveGateStates();
void multiplyClocks();
void handleChoke();
void handleLooper();
void determineLooperStatus();
void handleHistoryIndex();
void edgeDetectLoop();
void loopTriggers();
void calculateLooperVariables();
void sendIndexClocks();
void readLoopMemory();
void saveLoopVariables();
void recordTriggers();
void storeCurrentModificationVariables();
void storeCurrentTriggersOnClockEdge();
void storeTrigInterruptsInLoop();
int decideTickRecord(unsigned long risingEdgeTime);
void detectRatchets();
unsigned int chooseMode();
void checkCurrentModes();
unsigned int calculateCustomBoundary(unsigned int leftModeIndex, unsigned int rightModeIndex);
void calculateModeBoundaries();
unsigned int selectCurrentMode();
unsigned int CalculateCustomBoundary(unsigned int leftModeIndex, unsigned int rightModeIndex);
void SetBoundariesBetweenActiveModes();
void initializeIntervals();
void modifyTest();
void errorChecking();
void handleClockTimeout();
void calculateProbability();
void decideModificationStatus();
void activateModification();
void loopModificationHandler();
void handleDisplay();
void loopButtonDisplay();
void calculateDisplayCounters();
void calculateActiveLED();
void writeToModeMUX();
void handleMenu();
void changeMenuOptions();
void handleOutputs();
void calculateTriggerStates();
void outputGates();
void maintainCycle();
void loopCycle();
void skipSteps(unsigned int numSkips);
void hold();
void burst();
void ratchet();
void ball();
void rotate();
void trigDelay();
void breakBeat();
void skip();
void setClockSkipMode();
void trig1InterruptHandler();
void trig2InterruptHandler();
void trig3InterruptHandler();
void trig4InterruptHandler();
int wrapAround(int number, int range);
bool pulseWithinInterval(unsigned long elapsed, unsigned long interval, unsigned long pulseWidth);
byte getModeByte();
void setModeArray(byte modeByte);
byte getOptionByte();
void saveOptions();
void setOptions(byte optionByte);
void saveLoopData();

#ifdef IDUM_SIM_ID00M
unsigned long gatherEntropy();
void seedRNGOnFirstClock();
void displayCustomFirmwareID(unsigned long durationMs);
void flashVersion();
void writeEEPROMDefaults();
void setPriorityArray(byte b);
void setBreakSettings(byte b);
void setF9Settings(byte lo, byte hi);
void updateBreakClock();
bool modeOwnsClock(unsigned int m);
bool secondaryLEDOn(unsigned int pos);
void drawSecondaryMenu();
void drawF9Editor();
void drawLoopBlinkCount(unsigned int blinks);
void handleF9Editor();
void moveToFront(unsigned int ch);
unsigned int mergeOpForMode(unsigned int m);
void applyMerge();
bool channelIsFiring(unsigned int ch);
void setChannelState(unsigned int ch, bool on);
void applyLinearDrumming();
unsigned long f9ScalePeriod(unsigned long period, unsigned int ch);
unsigned long chanClockInterval(unsigned int ch);
unsigned long f9PulseWidth(unsigned long subInterval);
unsigned int randomOtherChannel(unsigned int cur);
void applyInfection(unsigned int ch, unsigned long t);
void spreadInfection(unsigned int source, unsigned long t, bool cascade, unsigned int prob);
void contaminate();
unsigned int scatterDivisor();
unsigned long biasedOffset(unsigned long gap, unsigned int bias);
unsigned int buildScatterSchedule(unsigned long gap, unsigned int param, unsigned long *offsets);
bool scatterFiring(unsigned long elapsed, unsigned long *offsets, unsigned int count);
void scatter();
#endif

#ifndef IDUM_SIM_MAIN
#define IDUM_SIM_MAIN "idum.ino"
#endif

#include IDUM_SIM_MAIN
#include "a0000_superSecretOptions.ino"
#include "a00_Variables.ino"
#include "a01_timers.ino"
#ifdef IDUM_SIM_HAS_ADC_DIAGNOSTIC
#include "a05_AdcDiagnostic.ino"
#endif
#include "a10_Setup.ino"
#include "a20_Loop.ino"
#include "a30_ReadInputs.ino"
#include "a31_EdgeDetect.ino"
#include "a32_PinInterrupts.ino"
#include "a40_handleLooper.ino"
#include "a41_LoopTriggers.ino"
#include "a42_RecordTriggers.ino"
#include "a50_ChooseMode.ino"
#include "a51_boundaryUtils.ino"
#include "a60_ModifyTest.ino"
#include "a70_handleDisplay.ino"
#include "a80_HandleMenu.ino"
#include "a90_handleOutputs.ino"
#include "a91_followUtils.ino"
#include "b10_holdMode.ino"
#include "b20_burstMode.ino"
#include "b30_multiplyMode.ino"
#include "b40_ballMode.ino"
#include "b50_rotateMode.ino"
#include "b60_delayMode.ino"
#include "b70_breakMode.ino"
#include "b80_skipMode.ino"
#ifdef IDUM_SIM_ID00M
#include "b81_contaminationMode.ino"
#include "b82_scatterMode.ino"
#endif
#include "x_utils.ino"

int main(int argc, char** argv) {
  programPath = argv[0];
  const std::string scenario = argc > 1 ? argv[1] : "smoke";
  return runScenario(scenario);
}
