#define IDUM_SIM 1
#define ARDUINO 100

#include "Arduino.h"
#include "EEPROM.h"
#include "engine_config.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <functional>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

SerialStub Serial;
EEPROMClass EEPROM;

void setup();
void loop();
void runStockEngine();
void runSelectedAlternateEngine();
void resetSelectedAlternateEngine();
void runSuperlooperEngine();
void resetSuperlooperEngine();
void adcDiagnosticLoop();
void modifyTest();
void activateModification();
void decideModificationStatus();
void calculateModeBoundaries();
unsigned int calculateCustomBoundary(unsigned int leftModeIndex, unsigned int rightModeIndex);
unsigned int selectCurrentMode();
void loopModificationHandler();
void storeCurrentModificationVariables();
void storeTrigInterruptsInLoop();
void calculateLooperVariables();
void trig1InterruptHandler();
void ratchet();
void rotate();
void outputGates();
void writeModeMuxAddress(unsigned int muxAddress);
void writeModeLed(unsigned int ledIndex);
void displayFirmwareVersion(unsigned int firmwareNumber);
#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER
void superlooperReadInputs();
void superlooperArmControlPickup(bool enteringLoop);
void superlooperDetermineLooperStatus();
void superlooperSetNOption(uint8_t option);
void superlooperResetSequencerTiming();
void superlooperClearTape();
void superlooperAdvanceSequencerTiming();
void superlooperRecordRenderedOutputs(uint8_t stateMask);
void superlooperCaptureCompleteWindow();
void superlooperConfigureLoopLength(unsigned int requestedLength);
void superlooperInitializeLoopTransport();
uint8_t superlooperUpdateLoopPlayback();
void superlooperResumeTapeAfterLoop();
uint8_t superlooperRenderedLoopCellState(uint16_t phraseCell);
uint16_t superlooperCellsPerStage();
uint16_t superlooperLoopSpeedQ8ForParam(unsigned int control);
bool superlooperSequencerClockHigh();
void superlooperEnterLoop();
extern unsigned int superlooperPhysicalLength;
extern unsigned int superlooperLoopLength;
extern uint8_t superlooperClocksPerStep;
extern uint8_t superlooperSequencerClockPhase;
extern bool superlooperSequencerTimingStarted;
extern bool superlooperSequencerStepBoundary;
extern uint16_t superlooperTapeWriteIndex;
extern uint16_t superlooperCaptureStartIndex;
extern uint16_t superlooperLoopBaseCell;
extern uint16_t superlooperLoopBaseCellCount;
extern unsigned long superlooperLoopTransportLastTime;
extern uint32_t superlooperLoopRawClocksElapsed;
extern uint8_t superlooperLoopTransportStage;
extern uint8_t superlooperLoopNaturalStage;
extern uint32_t superlooperRenderedTape[72][4];
#endif
#ifdef IDUM_SIM_CONST_BREAK_TABLE
extern const int breakBeat1[16][4][16];
#else
extern int breakBeat1[16][4][16];
#endif
extern bool externalSequencerTracking;
extern const bool fastExternalSequencerClock;
#ifdef IDUM_SIM_HAS_LEGACY_LOOP_MODULATION
extern unsigned int mutationLengthScaled;
extern bool loopParamPickupActive;
extern unsigned int loopParamPickupReference;
extern unsigned int mutationParamHeld;
extern unsigned int loopParamPickupMovementThreshold;
void updateLoopModulationLengthLatch();
void updateLoopModulationParamPickup();
void captureMutationParamForLoop();
void armMutationParamPickupAfterLoop();
unsigned int getMutationParamReading();
unsigned int getMutationLength();
#endif
extern bool trigHistory[4][8][4];
extern unsigned int historyIndex;
extern unsigned int mode;
extern unsigned int modeDialPosition;
extern unsigned int modePosition;
extern unsigned int lengthPosition;
extern bool activeModes[8];
extern bool clockEdge;
extern bool clockIn;
extern bool loopEnable;
extern bool loopInputBehavior;
extern bool loopIndexChanged;
extern bool loopButtonActiveEdge;
extern bool loopClockMult;
extern bool loopClockMultOld;
extern bool menuMode;
extern bool modificationActive;
extern bool splitMode;
extern bool freezeParam;
extern bool startModification;
extern bool startModification1;
extern bool startModification2;
extern bool startModification3;
extern bool startModification4;
extern bool endModification;
extern bool carryOver;
extern bool cycleIn;
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_NONE
extern bool cycleSwitchIn;
extern bool alternateEngineActive;
#endif
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
extern unsigned int loopLength;
extern unsigned int loopCycleIndex;
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
extern unsigned int modifyParam;
extern unsigned int paramRead;
extern unsigned int modifyParam1;
extern unsigned int modifyParam2;
extern unsigned int modifyParam3;
extern unsigned int modifyParam4;
extern unsigned int originalModifyLength;
extern unsigned int paramHistory[5][8][4];
extern unsigned int paramResolution;
extern unsigned int probRead;
extern unsigned int probabilityModifier;
extern unsigned int ratchetAmount;
extern unsigned int ratchetAmount1;
extern unsigned int trig3Divide;
extern unsigned int unscaledParam;
extern unsigned int unscaledParam1;
extern unsigned int unscaledParam2;
extern unsigned int unscaledParam3;
extern unsigned int unscaledParam4;
extern unsigned long lastTrig1Interval;
extern unsigned long lastClockInterval;
extern unsigned long lastClockTime;
#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_ATOMIC_LOOP_RECORD)
extern volatile bool forceTrig1LoopRecord;
extern volatile unsigned long forceTrig1LoopRecordTime;
#else
extern bool forceTrig1LoopRecord;
extern unsigned long forceTrig1LoopRecordTime;
#endif
#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_RNG_SEEDING)
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
bool forceAlternateRunner = false;
bool simInterruptsEnabled = true;
std::function<void()> interruptEnableHook;

void resetSimulation();
void printWrites();
int runScenario(const std::string& name);
#if defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
int bootResetScenario();
#endif
#ifdef IDUM_SIM_ELI_OO
int eliRatchetChannelFourZeroScenario();
#endif

int reportFirmwareRegression(const std::string& name, bool fixed, const std::string& observation) {
  std::cout << (fixed ? "FIXED " : "BUG   ") << name << ": " << observation << '\n';
  return fixed ? 0 : 1;
}

std::string shellQuote(const std::string& value) {
  std::string quoted = "'";
  for (char character : value) {
    if (character == '\'') quoted += "'\\''";
    else quoted += character;
  }
  return quoted + "'";
}

int normalizedSystemResult(int result) {
  if (result == -1) return 1;
  if (WIFEXITED(result)) return WEXITSTATUS(result);
  if (WIFSIGNALED(result)) return 128 + WTERMSIG(result);
  return 1;
}

int runChildScenario(const std::string& scenario) {
  const std::string command = shellQuote(programPath) + " " + shellQuote(scenario);
  return normalizedSystemResult(std::system(command.c_str()));
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

#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_NONE
  if (forceAlternateRunner) {
    setLogicalGate(17, true);
    loop();
    advanceMicros(100);
    loop();
    advanceMicros(100);
    if (!alternateEngineActive) {
      std::cerr << "FAIL " << name << ": alternate runner did not activate\n";
      return 1;
    }
  }
#endif

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
  simInterruptsEnabled = true;
  interruptEnableHook = nullptr;
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

int alternateEngineBoundaryRegressionScenario() {
  resetSimulation();
  setup();

  loopEnable = true;
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 3;
  modificationActive = true;

  setLogicalGate(17, true);
  loop();

#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_NONE
  const bool enteredAsStockCycle = cycleIn;
  return reportFirmwareRegression(
    "alternate-engine-boundary",
    enteredAsStockCycle,
    "stock CYCLE=" + std::to_string(cycleIn)
  );
#else
  const bool enteredFresh = alternateEngineActive
                         && !loopEnable && !modificationActive
                         && modifyLength == 0 && modifyLength1 == 0
                         && modifyLength2 == 0 && modifyLength3 == 0
                         && modifyLength4 == 0;

  loopEnable = true;
  modifyLength = 2;
  modificationActive = true;
  setLogicalGate(17, false);
  loop();

  const bool leftFresh = !alternateEngineActive
                      && !loopEnable && !modificationActive
                      && modifyLength == 0;

  return reportFirmwareRegression(
    "alternate-engine-boundary",
    enteredFresh && leftFresh,
    "fresh enter/exit=" + std::to_string(enteredFresh)
      + "/" + std::to_string(leftFresh)
  );
#endif
}

int alternateClockTrackingContractRegressionScenario() {
#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_NONE
  return reportFirmwareRegression(
    "alternate-clock-tracking-contract",
    false,
    "no alternate engine compiled"
  );
#else
  resetSimulation();
  setup();

  // CYCLE down selects classic. Its legacy cycleIn checks must see setting 0,
  // not the physical switch.
  setLogicalGate(17, false);
  externalSequencerTracking = true;
  loop();
  const bool classicTracksWhenEnabled = !alternateEngineActive
                                      && !cycleSwitchIn && cycleIn;

  externalSequencerTracking = false;
  loop();
  const bool classicStopsWhenDisabled = !alternateEngineActive
                                     && !cycleSwitchIn && !cycleIn;

  // CYCLE up changes runners only. Superlooper owns a dedicated N-divided
  // transport, so stock cycle-maintenance writes are disabled there.
  externalSequencerTracking = true;
  setLogicalGate(17, true);
  loop();
  loop();
  const bool alternateOwnsTransportWhenTrackingEnabled = alternateEngineActive
                                                      && cycleSwitchIn && !cycleIn;

  externalSequencerTracking = false;
  loop();
  const bool alternateOwnsTransportWhenTrackingDisabled = alternateEngineActive
                                                       && cycleSwitchIn && !cycleIn;

  const bool fixedFastClockPolicy = fastExternalSequencerClock;

  return reportFirmwareRegression(
    "alternate-clock-tracking-contract",
    classicTracksWhenEnabled && classicStopsWhenDisabled
      && alternateOwnsTransportWhenTrackingEnabled
      && alternateOwnsTransportWhenTrackingDisabled
      && fixedFastClockPolicy,
    "classic=" + std::to_string(classicTracksWhenEnabled)
      + "/" + std::to_string(classicStopsWhenDisabled)
      + " alternate-dedicated=" + std::to_string(alternateOwnsTransportWhenTrackingEnabled)
      + "/" + std::to_string(alternateOwnsTransportWhenTrackingDisabled)
      + " fast=" + std::to_string(fixedFastClockPolicy)
  );
#endif
}

int superlooperControlPickupRegressionScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  return reportFirmwareRegression(
    "superlooper-control-pickup",
    false,
    "Superlooper is not compiled"
  );
#else
  resetSimulation();
  setup();
  resetSuperlooperEngine();

  setLogicalAnalog(19, 200);
  setLogicalAnalog(20, 321);
  setLogicalAnalog(18, 300);
  superlooperReadInputs();

  const unsigned int mutationParamBeforeLoop = paramRead;
  const unsigned int mutationLengthBeforeLoop = lengthScaled;
  const unsigned int firstPanelLoopLength = lengthPosition + 1;

  superlooperEnterLoop();
  const bool predictableLoopDefaults = unscaledParam == 8
                                    && lengthScaled == firstPanelLoopLength
                                    && probRead == 321;

  // Chance remains a live stock control in this trigger-recording looper.
  setLogicalAnalog(20, 111);
  superlooperReadInputs();
  const bool chanceStayedLive = probRead == 111;
  const bool loopControlsStayedLatched = unscaledParam == 8
                                      && lengthScaled == firstPanelLoopLength;

  setLogicalAnalog(19, 700);
  setLogicalAnalog(18, 880);
  superlooperReadInputs();
  const bool loopControlsPickedUp = paramRead == 700
                                 && lengthScaled == lengthPosition + 1
                                 && lengthScaled != firstPanelLoopLength;

  loopEnable = false;
  superlooperArmControlPickup(false);
  const bool mutationControlsRestored = paramRead == mutationParamBeforeLoop
                                     && lengthScaled == mutationLengthBeforeLoop
                                     && probRead == 111;

  superlooperReadInputs();
  const bool exitControlsStayedLatched = paramRead == mutationParamBeforeLoop
                                      && lengthScaled == mutationLengthBeforeLoop;

  setLogicalAnalog(19, 900);
  setLogicalAnalog(18, 50);
  superlooperReadInputs();
  const bool mutationControlsPickedUp = paramRead == 900
                                     && lengthScaled == superlooperPhysicalLength;

  return reportFirmwareRegression(
    "superlooper-control-pickup",
    predictableLoopDefaults && chanceStayedLive && loopControlsStayedLatched
      && loopControlsPickedUp && mutationControlsRestored
      && exitControlsStayedLatched && mutationControlsPickedUp,
    "loop-defaults=" + std::to_string(predictableLoopDefaults)
      + ", chance-live=" + std::to_string(chanceStayedLive)
      + ", loop-pickup=" + std::to_string(loopControlsPickedUp)
      + ", mutation-restore/pickup=" + std::to_string(mutationControlsRestored)
      + "/" + std::to_string(mutationControlsPickedUp)
  );
#endif
}

int superlooperAnchorRegressionScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  return reportFirmwareRegression(
    "superlooper-most-recent-anchor",
    false,
    "Superlooper is not compiled"
  );
#else
  resetSimulation();
  setup();
  resetSuperlooperEngine();

  // N=3 gives 96 tape cells per sequencer stage. The current partial stage is
  // raw-clock cells 24..26; the preceding eight complete stages are 0..23.
  superlooperSetNOption(2);
  superlooperSequencerTimingStarted = true;
  superlooperSequencerClockPhase = 2;
  superlooperTapeWriteIndex = 26;

  // Mark the beginnings of the newest four complete stages with four
  // different rendered output channels.
  superlooperRenderedTape[12][0] = 1;
  superlooperRenderedTape[15][1] = 1;
  superlooperRenderedTape[18][2] = 1;
  superlooperRenderedTape[21][3] = 1;

  superlooperCaptureCompleteWindow();
  superlooperConfigureLoopLength(4);
  const uint16_t stageCells = superlooperCellsPerStage();
  const std::vector<uint8_t> fourStageStates {
    superlooperRenderedLoopCellState(0),
    superlooperRenderedLoopCellState(stageCells),
    superlooperRenderedLoopCellState(stageCells * 2),
    superlooperRenderedLoopCellState(stageCells * 3),
  };
  const std::vector<uint8_t> expected {1, 2, 4, 8};
  const bool fourBeatWindow = fourStageStates == expected;
  const bool newestBeatIsAnchor = superlooperCaptureStartIndex == 0
                               && superlooperLoopBaseCellCount == 4 * stageCells;

  superlooperConfigureLoopLength(2);
  const bool shorterWindowsStayAnchored =
    superlooperRenderedLoopCellState(0) == 4
    && superlooperRenderedLoopCellState(stageCells) == 8;

  const bool oneToOneSpeed = superlooperLoopSpeedQ8ForParam(512) == 256;

  return reportFirmwareRegression(
    "superlooper-most-recent-anchor",
    fourBeatWindow && newestBeatIsAnchor && shorterWindowsStayAnchored
      && oneToOneSpeed,
    "N=3 L4=1,2,4,8:" + std::to_string(fourBeatWindow)
      + ", complete-anchor=" + std::to_string(newestBeatIsAnchor)
      + ", L2=4,8:" + std::to_string(shorterWindowsStayAnchored)
      + ", 1:1=" + std::to_string(oneToOneSpeed)
  );
#endif
}

int superlooperNTransportRegressionScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  return reportFirmwareRegression("superlooper-n-transport", false, "Superlooper is not compiled");
#else
  resetSimulation();
  setup();
  resetSuperlooperEngine();
  superlooperSetNOption(2);  //N=3

  std::vector<bool> sequencerClockStates;
  loopEnable = false;
  for (unsigned int rawClock = 0; rawClock < 7; ++rawClock) {
    clockIn = true;
    clockEdge = true;
    superlooperAdvanceSequencerTiming();
    sequencerClockStates.push_back(superlooperSequencerClockHigh());
    clockIn = false;
    clockEdge = false;
  }

  const std::vector<bool> expected {true, false, false, true, false, false, true};
  const bool dividedExactly = sequencerClockStates == expected;

  const unsigned long mutationClockInterval = 123456;
  lastClockInterval = mutationClockInterval;
  lengthScaled = 5;
  superlooperSetNOption(5);  //N=8 changes transport storage only.
  const bool mutationInputsUntouched = lastClockInterval == mutationClockInterval
                                    && lengthScaled == 5
                                    && superlooperClocksPerStep == 8;

  //With natural stage 2 and L=4, the captured window starts at stage 6.
  //Entering between boundaries advances an attached eight-step sequencer four
  //places; entering on a boundary advances five because that live edge has not
  //yet been emitted by the alternate runner.
  externalSequencerTracking = true;
  superlooperLoopLength = 4;
  superlooperLoopNaturalStage = 2;
  superlooperSequencerClockPhase = 1;
  superlooperSequencerStepBoundary = false;
  clockIn = false;
  loopEnable = true;
  writes.clear();
  superlooperInitializeLoopTransport();
  const bool midStageEntryAligned = countOutputOnsets(13) == 4
                                 && superlooperLoopTransportStage == 6;

  superlooperLoopNaturalStage = 2;
  superlooperSequencerClockPhase = 0;
  superlooperSequencerStepBoundary = true;
  clockIn = true;
  writes.clear();
  superlooperInitializeLoopTransport();
  const bool boundaryEntryAligned = countOutputOnsets(13) == 5
                                  && superlooperLoopTransportStage == 6;

  return reportFirmwareRegression(
    "superlooper-n-transport",
    dividedExactly && mutationInputsUntouched
      && midStageEntryAligned && boundaryEntryAligned,
    "N3 clocks=1,0,0,1,0,0,1:" + std::to_string(dividedExactly)
      + ", mutation timing/length untouched=" + std::to_string(mutationInputsUntouched)
      + ", entry mid/edge=" + std::to_string(midStageEntryAligned)
      + "/" + std::to_string(boundaryEntryAligned)
  );
#endif
}

int superlooperRenderedTapeRegressionScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  return reportFirmwareRegression("superlooper-rendered-tape", false, "Superlooper is not compiled");
#else
  resetSimulation();
  setup();
  resetSuperlooperEngine();
  superlooperSetNOption(0);

  superlooperSequencerTimingStarted = true;
  superlooperTapeWriteIndex = 0;
  lastClockTime = micros();
  lastClockInterval = 1600;

  superlooperRecordRenderedOutputs(1);  //TR1 high in cell 0.
  advanceMicros(400);
  superlooperRecordRenderedOutputs(2);  //TR2 rises in cell 4.
  advanceMicros(400);
  superlooperRecordRenderedOutputs(0);  //Both low in cell 8.

  const bool heldStateFilled =
    (superlooperRenderedTape[0][0] & UINT32_C(0x000000ff)) == UINT32_C(0x000000ff);
  const bool secondPulseCaptured =
    (superlooperRenderedTape[0][1] & UINT32_C(0x0000ff00)) == UINT32_C(0x0000ff00);
  const bool postFallStayedLow =
    (superlooperRenderedTape[0][0] & UINT32_C(0xffff0000)) == 0
    && (superlooperRenderedTape[0][1] & UINT32_C(0xffff0000)) == 0;

  const bool gradualSpeed = superlooperLoopSpeedQ8ForParam(0) == 32
                         && superlooperLoopSpeedQ8ForParam(448) == 256
                         && superlooperLoopSpeedQ8ForParam(640) == 256
                         && superlooperLoopSpeedQ8ForParam(700) > 256
                         && superlooperLoopSpeedQ8ForParam(700) < 2048
                         && superlooperLoopSpeedQ8ForParam(1024) == 2048;

  //At 1:1, 400us advances eight of the 32 cells in a 1600us raw clock.
  //A rising edge crossed along the way must still be emitted even if the
  //main loop did not visit that cell at exactly the right instant.
  superlooperClearTape();
  superlooperCaptureStartIndex = 0;
  superlooperConfigureLoopLength(1);
  superlooperRenderedTape[7][1] = (uint32_t)1 << 8;
  loopEnable = true;
  modifyParam = 512;
  superlooperLoopTransportLastTime = micros();
  advanceMicros(400);
  const uint8_t crossedPlaybackState = superlooperUpdateLoopPlayback();
  const bool crossedOnsetPreserved = superlooperLoopBaseCell == 8
                                  && (crossedPlaybackState & 2);

  //The frozen read tape must resume at the raw-clock position that elapsed
  //during playback, with no stale pre-loop cells left in that elapsed span.
  loopEnable = false;
  superlooperSetNOption(2);
  superlooperTapeWriteIndex = 8;
  superlooperLoopRawClocksElapsed = 5;
  for (uint16_t rawClock = 8; rawClock <= 13; ++rawClock) {
    superlooperRenderedTape[rawClock][0] = UINT32_MAX;
  }
  superlooperResumeTapeAfterLoop();
  bool elapsedSpanCleared = superlooperTapeWriteIndex == 13;
  for (uint16_t rawClock = 8; rawClock <= 13; ++rawClock) {
    elapsedSpanCleared = elapsedSpanCleared
                      && (superlooperRenderedTape[rawClock][0] == 0);
  }

  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 3;
  modificationActive = true;
  superlooperEnterLoop();
  const bool mutationBypassedOnEntry = loopEnable && !modificationActive
                                    && modifyLength == 0 && modifyLength1 == 0
                                    && modifyLength2 == 0 && modifyLength3 == 0
                                    && modifyLength4 == 0;

  return reportFirmwareRegression(
    "superlooper-rendered-tape",
    heldStateFilled && secondPulseCaptured && postFallStayedLow
      && gradualSpeed && crossedOnsetPreserved && elapsedSpanCleared
      && mutationBypassedOnEntry,
    "32-cell fill/onset/fall=" + std::to_string(heldStateFilled)
      + "/" + std::to_string(secondPulseCaptured)
      + "/" + std::to_string(postFallStayedLow)
      + ", gradual-speed=" + std::to_string(gradualSpeed)
      + ", crossed-onset=" + std::to_string(crossedOnsetPreserved)
      + ", resume-head=" + std::to_string(elapsedSpanCleared)
      + ", bypass=" + std::to_string(mutationBypassedOnEntry)
  );
#endif
}

int superlooperSkipTransportRegressionScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  return reportFirmwareRegression("superlooper-skip-transport", false, "Superlooper is not compiled");
#else
  return runModeScenario("superlooper-skip-transport", [] {
    establishClockInterval(50000);
    configureAlwaysModify(7, 100, 520);
    runFor(10000);
    superlooperSetNOption(2);  //N=3 and a fresh transport phase.
    writes.clear();
    runClockTrain(6, 50000);

    return countOutputOnsets(13) == 2;
  }, "Skip leaves CLOCK OUT on the exact N=3 transport",
     "Skip added or removed pulses from the dedicated N=3 transport");
#endif
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

int runnerExitStatusRegressionScenario() {
  const int result = runChildScenario("regression-intentional-failure");
  return reportFirmwareRegression(
    "runner-propagates-failure",
    result == 1,
    "child exit 1 became aggregate result=" + std::to_string(result)
  );
}

int interruptRecordRaceRegressionScenario() {
  resetSimulation();
  setup();

  historyIndex = 0;
  lastClockTime = 1000;
  lastClockInterval = 1000;
  simMicros = 1600;
  forceTrig1LoopRecord = true;
  forceTrig1LoopRecordTime = 1200;  // Tick 1.

  // Inject a second edge at the instant the firmware re-enables interrupts
  // after consuming the first timestamp.
  interruptEnableHook = [] { trig1InterruptHandler(); };
  storeTrigInterruptsInLoop();

  const bool secondEventPreserved = forceTrig1LoopRecord && forceTrig1LoopRecordTime == 1600;
  storeTrigInterruptsInLoop();

  const bool fixed = trigHistory[0][0][1]
                     && trigHistory[0][0][2]
                     && secondEventPreserved
                     && !forceTrig1LoopRecord;
  return reportFirmwareRegression(
    "interrupt-record-consume-race",
    fixed,
    "first tick=" + std::to_string(trigHistory[0][0][1])
      + ", second tick=" + std::to_string(trigHistory[0][0][2])
      + ", second event preserved=" + std::to_string(secondEventPreserved)
  );
}

int loopRecordDecisionPhaseRegressionScenario() {
  resetSimulation();
  setup();
  configureAlwaysModify(4, 256, 0);  // Guaranteed one-step ROTATE modification.

  setLogicalGate(15, true);
  loop();

  const unsigned int recordedLength = paramHistory[0][historyIndex][2];
  const bool fixed = modificationActive && modifyLength == 1 && recordedLength == modifyLength;
  return reportFirmwareRegression(
    "loop-records-post-decision-state",
    fixed,
    "active length=" + std::to_string(modifyLength)
      + ", recorded length=" + std::to_string(recordedLength)
  );
}

int splitFrozenParamRecordingRegressionScenario() {
  resetSimulation();
  setup();

  splitMode = true;
  freezeParam = true;
  clockEdge = true;
  historyIndex = 3;
  modifyParam = 999;
  modifyParam1 = 128;
  modifyParam2 = 896;
  modifyParam3 = 320;
  modifyParam4 = 704;
  modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 2;

  storeCurrentModificationVariables();

  const bool fixed = paramHistory[1][3][1] == modifyParam1
                     && paramHistory[2][3][1] == modifyParam2
                     && paramHistory[3][3][1] == modifyParam3
                     && paramHistory[4][3][1] == modifyParam4;
  return reportFirmwareRegression(
    "split-loop-records-frozen-params",
    fixed,
    "recorded channel params=" + std::to_string(paramHistory[1][3][1]) + "/"
      + std::to_string(paramHistory[2][3][1]) + "/"
      + std::to_string(paramHistory[3][3][1]) + "/"
      + std::to_string(paramHistory[4][3][1])
  );
}

int removedModeHysteresisRegressionScenario() {
  resetSimulation();
  setup();

  for (bool &activeMode : activeModes) activeMode = false;
  activeModes[2] = true;
  activeModes[6] = true;
  modificationActive = false;
  menuMode = false;
  calculateModeBoundaries();
  const unsigned int boundary = calculateCustomBoundary(2, 6);

  mode = modePosition = 2;
  modeRead = boundary + 10;
  selectCurrentMode();
  const bool heldLowerMode = modePosition == 2;

  mode = modePosition = 6;
  modeRead = boundary - 10;
  selectCurrentMode();
  const bool heldUpperMode = modePosition == 6;

  return reportFirmwareRegression(
    "removed-mode-boundary-hysteresis",
    heldLowerMode && heldUpperMode,
    "held lower/upper=" + std::to_string(heldLowerMode) + "/" + std::to_string(heldUpperMode)
      + " around boundary=" + std::to_string(boundary)
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
#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_RNG_SEEDING)
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

#ifdef IDUM_SIM_HAS_LEGACY_LOOP_MODULATION
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

int loopModulationLengthScenario() {
  return runModeScenario("loop-modulation-length", [] {
    setLogicalAnalog(20, 0);  //keep probability from starting unrelated modifications
    setLogicalAnalog(18, 300);
    runFor(20000);
    const unsigned int mutationLengthAtEntry = lengthScaled;

    setLogicalGate(17, true);
    runFor(20000);
    const bool capturedEntryLength = mutationLengthScaled == mutationLengthAtEntry;

    setLogicalAnalog(18, 1000);
    runFor(20000);
    const unsigned int liveLoopLength = lengthScaled;

    loopEnable = true;
    calculateLooperVariables();
    const bool loopTracksLiveLength = (liveLoopLength != mutationLengthAtEntry)
                                      && (loopLength == liveLoopLength);
    loopEnable = false;

    splitMode = false;
    carryOver = false;
    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
    startModification = true;
    activateModification();
    startModification = false;
    const bool mutationUsesLatchedLength = modifyLength == mutationLengthAtEntry;

    splitMode = true;
    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
    startModification1 = startModification2 = startModification3 = startModification4 = true;
    activateModification();
    startModification1 = startModification2 = startModification3 = startModification4 = false;
    const bool splitMutationsUseLatchedLength = (modifyLength1 == mutationLengthAtEntry)
                                                && (modifyLength2 == mutationLengthAtEntry)
                                                && (modifyLength3 == mutationLengthAtEntry)
                                                && (modifyLength4 == mutationLengthAtEntry);
    splitMode = false;

    setLogicalGate(17, false);
    runFor(20000);
    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
    startModification = true;
    activateModification();
    startModification = false;
    const bool releaseRestoresLiveLength = modifyLength == liveLoopLength;

    return capturedEntryLength && loopTracksLiveLength && mutationUsesLatchedLength
           && splitMutationsUseLatchedLength && releaseRestoresLiveLength;
  }, "CYCLE latches mutation length while live LENGTH edits the loop", "expected latched mutation length, live loop length, and normal control after release");
}

int loopModulationParamPickupScenario() {
  return runModeScenario("loop-modulation-param-pickup", [] {
    setLogicalAnalog(20, 0);  //avoid unrelated modification starts
    setLogicalAnalog(19, 220);
    setLogicalGate(17, true);
    runFor(20000);
    const unsigned int preLoopParam = modifyParam;

    pulseGate(16, 5000, 100);
    runFor(20000);
    const bool enteredLoop = loopEnable;

    setLogicalAnalog(19, 800);
    runFor(20000);
    const bool loopParamStayedLive = modifyParam != preLoopParam;

    pulseGate(16, 5000, 100);
    runFor(5000);
    const bool exitArmedPickup = !loopEnable && loopParamPickupActive
                               && modifyParam == preLoopParam;

    const int withinThreshold = std::min(
      1024,
      static_cast<int>(loopParamPickupReference + loopParamPickupMovementThreshold)
    );
    setLogicalAnalog(19, withinThreshold);
    runFor(5000);
    const bool jitterIgnored = loopParamPickupActive && modifyParam == preLoopParam;

    freezeParam = true;
    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
    startModification = true;
    activateModification();
    startModification = false;
    const bool frozenParamPathRespectsPickup = modifyParam == preLoopParam
                                            && modifyParam1 == preLoopParam
                                            && modifyParam2 == preLoopParam
                                            && modifyParam3 == preLoopParam
                                            && modifyParam4 == preLoopParam;
    freezeParam = false;
    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;

    const int deliberateMove = std::min(
      1024,
      static_cast<int>(loopParamPickupReference + loopParamPickupMovementThreshold + 40)
    );
    setLogicalAnalog(19, deliberateMove);
    runFor(5000);
    const bool movementReleased = !loopParamPickupActive
                               && modifyParam == static_cast<unsigned int>(deliberateMove);

    setLogicalGate(17, false);
    runFor(5000);
    pulseGate(16, 5000, 100);
    runFor(5000);
    setLogicalAnalog(19, 360);
    runFor(5000);
    pulseGate(16, 5000, 100);
    runFor(5000);
    const bool normalExitDoesNotArm = !loopEnable && !loopParamPickupActive
                                   && modifyParam == 360;

    setLogicalGate(17, true);
    setLogicalAnalog(19, 240);
    loopInputBehavior = true;
    runFor(5000);
    setLogicalGate(12, true);
    runFor(5000);
    const bool gateEnteredLoop = loopEnable;
    setLogicalAnalog(19, 700);
    runFor(5000);
    setLogicalGate(12, false);
    runFor(5000);
    const unsigned int gateExitHeldValue = mutationParamHeld;
    const unsigned int gateExitCurrentValue = modifyParam;
    const bool gateExitPickupState = loopParamPickupActive;
    const bool gateExitCycleState = loopModulationMode;
    const bool gateExitArmedPickup = !loopEnable && loopParamPickupActive
                                  && mutationParamHeld == 240
                                  && modifyParam == 240;
    setLogicalAnalog(19, 760);
    runFor(5000);
    const bool gatePickupReleased = !loopParamPickupActive && modifyParam == 760;
    loopInputBehavior = false;

    const bool passed = enteredLoop && loopParamStayedLive && exitArmedPickup && jitterIgnored
                     && frozenParamPathRespectsPickup
                     && movementReleased && normalExitDoesNotArm && gateEnteredLoop
                     && gateExitArmedPickup && gatePickupReleased;
    if (!passed) {
      std::cout << "pickup checks: button=" << enteredLoop << '/' << loopParamStayedLive
                << '/' << exitArmedPickup << '/' << jitterIgnored
                << '/' << frozenParamPathRespectsPickup << '/' << movementReleased
                << ", normal=" << normalExitDoesNotArm
                << ", gate=" << gateEnteredLoop << '/' << gateExitArmedPickup
                << '/' << gatePickupReleased
                << " exit-held/current=" << gateExitHeldValue << '/'
                << gateExitCurrentValue << " active/cycle="
                << gateExitPickupState << '/' << gateExitCycleState << '\n';
    }
    return passed;
  }, "post-loop Param waits for real movement only in the CYCLE layer",
     "expected one-shot Param hold, jitter rejection, movement release, and normal non-CYCLE exit");
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
    "ball",
    "rotate",
    "delay",
    "break",
    "clock-stop-burst",
    "skip",
  };

  for (const auto& scenario : scenarios) {
    const int result = runChildScenario(scenario);
    if (result != 0) return result;
  }

  std::cout << "PASS all-modes\n";
  return 0;
}

int allAlternateModesScenario() {
#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_NONE
  std::cerr << "FAIL all-alternate-modes: no alternate engine is compiled\n";
  return 1;
#else
  const std::vector<std::string> scenarios = {
    "alternate-hold",
    "alternate-clock-ratchet",
    "alternate-clock-ratchet-divide",
    "alternate-trigger-ratchet",
    "alternate-trigger-ratchet-divide",
    "alternate-ball",
    "alternate-rotate",
    "alternate-delay",
    "alternate-break",
  };

  for (const auto& scenario : scenarios) {
    const int result = runChildScenario(scenario);
    if (result != 0) return result;
  }

  std::cout << "PASS all-alternate-modes\n";
  return 0;
#endif
}

uint64_t mixFingerprintByte(uint64_t hash, uint8_t value) {
  return (hash ^ value) * UINT64_C(1099511628211);
}

uint64_t mixFingerprintWord(uint64_t hash, uint64_t value) {
  for (unsigned int i = 0; i < 8; ++i) {
    hash = mixFingerprintByte(hash, static_cast<uint8_t>(value & 0xff));
    value >>= 8;
  }
  return hash;
}

int stockEngineFingerprintScenario() {
  resetSimulation();
  setup();

  setLogicalGate(17, false);  //the stock runner must own this entire trace
  setLogicalAnalog(20, 1024);
  setLogicalAnalog(18, 520);
  setLogicalAnalog(19, 700);
  setModeCv(0);
  runFor(20000);
  writes.clear();

  establishClockInterval();
  establishTriggerIntervals();

  for (unsigned int selectedMode = 0; selectedMode < 8; ++selectedMode) {
    setModeCv(selectedMode);
    setLogicalAnalog(19, 100 + selectedMode * 110);
    setLogicalAnalog(18, 120 + selectedMode * 100);
    runFor(20000);
    runClockAndTriggerTrain(3, 125000);
  }

  //Include ordinary loop entry/playback/exit in the shared-stock contract.
  pulseGate(16, 5000, 100);
  runClockAndTriggerTrain(5, 125000);
  pulseGate(16, 5000, 100);
  runClockAndTriggerTrain(2, 125000);

  uint64_t fingerprint = UINT64_C(1469598103934665603);
  for (const auto& write : writes) {
    fingerprint = mixFingerprintWord(fingerprint, write.time);
    fingerprint = mixFingerprintByte(fingerprint, write.pin);
    fingerprint = mixFingerprintByte(fingerprint, write.value);
  }

  fingerprint = mixFingerprintWord(fingerprint, mode);
  fingerprint = mixFingerprintWord(fingerprint, loopIndex);
  fingerprint = mixFingerprintWord(fingerprint, historyIndex);
  fingerprint = mixFingerprintWord(fingerprint, modifyLength);
  fingerprint = mixFingerprintByte(fingerprint, loopEnable);
  fingerprint = mixFingerprintByte(fingerprint, modificationActive);

  std::cout << "stock-fingerprint=" << std::hex << fingerprint << std::dec
            << " writes=" << writes.size() << '\n';
  return 0;
}

int allSuperlooperRegressionsScenario() {
#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_SUPERLOOPER
  std::cerr << "FAIL all-superlooper-regressions: Superlooper is not compiled\n";
  return 1;
#else
  const std::vector<std::string> scenarios = {
    "regression-alternate-engine-boundary",
    "regression-alternate-clock-tracking-contract",
    "regression-superlooper-control-pickup",
    "regression-superlooper-anchor",
    "regression-superlooper-n-transport",
    "regression-superlooper-rendered-tape",
    "alternate-regression-superlooper-skip-transport",
    "all-alternate-modes",
  };

  for (const auto& scenario : scenarios) {
    const int result = runChildScenario(scenario);
    if (result != 0) return result;
  }

  std::cout << "PASS all-superlooper-regressions\n";
  return 0;
#endif
}

int allRegressionsScenario() {
  const std::vector<std::string> scenarios = {
    "regression-runner-status",
    "regression-interrupt-record-race",
    "regression-loop-record-phase",
    "regression-split-frozen-param-recording",
    "regression-removed-mode-hysteresis",
    "regression-length-1",
    "regression-rotate-split-param",
    "regression-rotate-silent-destination",
    "regression-ball-channel-3-reset",
    "regression-back-to-back-mode",
    "regression-alternate-engine-boundary",
    "regression-removed-mode-boot",
    "regression-split-loop-empty-step",
    "regression-loop-phase",
    "regression-loop-ratchet-map",
    "regression-ratchet-zero-interval",
    "regression-rng-power-on",
#if defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
    "boot-led-eeprom",
#endif
  };

  for (const auto& scenario : scenarios) {
    const int result = runChildScenario(scenario);
    if (result != 0) return result;
  }

  std::cout << "PASS all-regressions\n";
  return 0;
}

int runScenario(const std::string& name) {
  const std::string alternatePrefix = "alternate-";
  if (name.compare(0, alternatePrefix.size(), alternatePrefix) == 0) {
    forceAlternateRunner = true;
    return runScenario(name.substr(alternatePrefix.size()));
  }

  if (name == "smoke") return smokeScenario();
#if defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
  if (name == "boot-led-eeprom") return bootResetScenario();
#endif
#ifdef IDUM_SIM_ELI_OO
  if (name == "eli-ratchet-channel-4-zero") return eliRatchetChannelFourZeroScenario();
#endif
  if (name == "regression-intentional-failure") return 1;
  if (name == "regression-runner-status") return runnerExitStatusRegressionScenario();
  if (name == "regression-interrupt-record-race") return interruptRecordRaceRegressionScenario();
  if (name == "regression-loop-record-phase") return loopRecordDecisionPhaseRegressionScenario();
  if (name == "regression-split-frozen-param-recording") return splitFrozenParamRecordingRegressionScenario();
  if (name == "regression-removed-mode-hysteresis") return removedModeHysteresisRegressionScenario();
  if (name == "regression-length-1") return lengthOneRegressionScenario();
  if (name == "regression-rotate-split-param") return splitRotateParamRegressionScenario();
  if (name == "regression-rotate-silent-destination") return rotateSilentDestinationRegressionScenario();
  if (name == "regression-ball-channel-3-reset") return ballChannelThreeResetRegressionScenario();
  if (name == "regression-back-to-back-mode") return backToBackModeRegressionScenario();
  if (name == "regression-alternate-engine-boundary") return alternateEngineBoundaryRegressionScenario();
  if (name == "regression-alternate-clock-tracking-contract") return alternateClockTrackingContractRegressionScenario();
  if (name == "regression-superlooper-control-pickup") return superlooperControlPickupRegressionScenario();
  if (name == "regression-superlooper-anchor") return superlooperAnchorRegressionScenario();
  if (name == "regression-superlooper-n-transport") return superlooperNTransportRegressionScenario();
  if (name == "regression-superlooper-rendered-tape") return superlooperRenderedTapeRegressionScenario();
  if (name == "regression-superlooper-skip-transport") return superlooperSkipTransportRegressionScenario();
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
#ifdef IDUM_SIM_HAS_LEGACY_LOOP_MODULATION
  if (name == "loop-modulation-mode") return loopModulationModeScenario();
  if (name == "loop-modulation-length") return loopModulationLengthScenario();
  if (name == "loop-modulation-param-pickup") return loopModulationParamPickupScenario();
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
  if (name == "all-alternate-modes") return allAlternateModesScenario();
  if (name == "stock-fingerprint") return stockEngineFingerprintScenario();
  if (name == "all-superlooper-regressions") return allSuperlooperRegressionsScenario();
  if (name == "all-regressions") return allRegressionsScenario();

  std::cerr << "Unknown scenario: " << name << '\n';
  std::cerr << "Available scenarios: smoke, regression-length-1, regression-rotate-split-param, regression-rotate-silent-destination, regression-ball-channel-3-reset, regression-back-to-back-mode, regression-removed-mode-boot, regression-split-loop-empty-step, regression-loop-phase, regression-loop-ratchet-map, regression-ratchet-zero-interval, regression-rng-power-on, hold, clock-ratchet, clock-ratchet-divide, clock-ratchet-sparse-min-length, trigger-ratchet, trigger-ratchet-divide, trigger-ratchet-divide-steady, loop-modulation-mode, loop-modulation-length, loop-modulation-param-pickup, ball, rotate, delay, break, clock-stop-burst, break-patterns, break-playback, skip, all-modes, all-regressions\n";
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

void noInterrupts() {
  simInterruptsEnabled = false;
}

void interrupts() {
  const bool wasDisabled = !simInterruptsEnabled;
  simInterruptsEnabled = true;
  if (wasDisabled && interruptEnableHook) {
    auto hook = std::move(interruptEnableHook);
    interruptEnableHook = nullptr;
    hook();
  }
}

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

#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
unsigned long gatherEntropy();
void seedRNGOnFirstClock();
void writeEEPROMDefaults();
#endif

#if defined(IDUM_SIM_ID00M) || defined(IDUM_SIM_ELI_OO)
unsigned long f9PulseWidth(unsigned long subInterval);
#endif

#if defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
void readStartupButtonStates();
void startupAnimation();
#endif

#ifdef IDUM_SIM_ID00M
void displayCustomFirmwareID(unsigned long durationMs);
void flashVersion();
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
#include "c00_AlternateEngine.ino"
#include "c10_SuperlooperEngine.ino"
#ifdef IDUM_SIM_ID00M
#include "b81_contaminationMode.ino"
#include "b82_scatterMode.ino"
#endif
#include "x_utils.ino"

#if defined(IDUM_SIM_ELI_OO) || defined(IDUM_SIM_HAS_BOOT_RESET)
namespace {
int bootResetScenario() {
  resetSimulation();
  factoryResetHeld = false;
  changeLEDOrder = false;
  startupAnimationNumber = 0;
  LEDTweak = LEDTweakSetEEPROM;
  initializeEEPROM();

  const bool freshInitialized = EEPROM.read(0) == 'R'
                             && EEPROM.read(1) == 0
                             && EEPROM.read(2) == 255
                             && EEPROM.read(3) == 0
                             && EEPROM.read(4) == 0
                             && EEPROM.read(5) == static_cast<byte>(LEDTweakSetEEPROM)
                             && LEDTweak == LEDTweakSetEEPROM
                             && startupAnimationNumber == 1;

  EEPROM.write(0, 'R');
  EEPROM.write(1, 255);
  EEPROM.write(2, 0);
  EEPROM.write(3, 2);
  EEPROM.write(4, 1);
  EEPROM.write(5, 0);
  factoryResetHeld = true;
  changeLEDOrder = false;
  startupAnimationNumber = 0;
  LEDTweak = true;
  initializeEEPROM();

  const bool resetPreservedHardware = EEPROM.read(0) == 'R'
                                   && EEPROM.read(1) == 0
                                   && EEPROM.read(2) == 255
                                   && EEPROM.read(3) == 0
                                   && EEPROM.read(4) == 0
                                   && EEPROM.read(5) == 0
                                   && !LEDTweak
                                   && startupAnimationNumber == 2;

  EEPROM.write(0, 'R');
  EEPROM.write(1, 0);
  EEPROM.write(2, 255);
  EEPROM.write(3, 0);
  EEPROM.write(4, 0);
  EEPROM.write(5, 0);
  factoryResetHeld = false;
  changeLEDOrder = true;
  startupAnimationNumber = 0;
  LEDTweak = true;
  initializeEEPROM();
  const bool loopToggledHardware = EEPROM.read(5) == 1
                                && LEDTweak
                                && startupAnimationNumber == 3;

#ifdef IDUM_SIM_HAS_BOOT_RESET
  LEDTweak = false;
  writeModeLed(0);
  const bool legacyAddress = digitalRead(mode1LED) == HIGH
                          && digitalRead(mode2LED) == LOW
                          && digitalRead(mode3LED) == LOW;
  LEDTweak = true;
  writeModeLed(0);
  const bool v13Address = digitalRead(mode1LED) == HIGH
                       && digitalRead(mode2LED) == HIGH
                       && digitalRead(mode3LED) == HIGH;
#else
  const bool legacyAddress = true;
  const bool v13Address = true;
#endif

  const bool passed = freshInitialized && resetPreservedHardware
                   && loopToggledHardware && legacyAddress && v13Address;
  std::ostringstream detail;
  detail << "fresh=" << freshInitialized
         << ", reset-preserves-led=" << resetPreservedHardware
         << ", loop-toggles-led=" << loopToggledHardware
         << ", mux-orders=" << (legacyAddress && v13Address);
  return reportFirmwareRegression(
    "boot-led-eeprom",
    passed,
    detail.str()
  );
}
}  // namespace
#endif

#ifdef IDUM_SIM_ELI_OO
namespace {
int eliRatchetChannelFourZeroScenario() {
  resetSimulation();
  clockChan = iCLOCK {};
  trigChan1 = iTRIG {};
  trigChan2 = iTRIG {};
  trigChan3 = iTRIG {};
  trigChan4 = iTRIG {};
  alternateMultiplyMode = false;

  trigChan2.modifyLength = 1;
  trigChan2.mode = 2;
  trigChan2.unscaledParam = 15;
  trigChan2.ratchetAmount = 2;
  trigChan2.lastInterval = 100000;

  trigChan4.modifyLength = 1;
  trigChan4.mode = 2;
  trigChan4.unscaledParam = 15;
  trigChan4.ratchetAmount = 2;
  trigChan4.lastInterval = 0;

  std::cout << "RUN   eli-ratchet-channel-4-zero: channel 2 valid, channel 4 unmeasured\n";
  ratchet();
  return reportFirmwareRegression(
    "eli-ratchet-channel-4-zero",
    !trigChan4.stateOUT,
    "returned safely with channel 4 off"
  );
}
}  // namespace
#endif

int main(int argc, char** argv) {
  programPath = argv[0];
  const std::string scenario = argc > 1 ? argv[1] : "smoke";
  return runScenario(scenario);
}
