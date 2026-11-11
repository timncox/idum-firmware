// IDUM emulator core, compiled to WebAssembly for web/emulator.html.
//
// It builds the same firmware sources as the host simulator (sim/idum_sim.cpp) with the same
// IDUM_SIM stubs, but instead of running test scenarios it exposes a small C API that the
// page drives in real time: set knobs, gates and switches, run N audio samples, read back
// the four trigger outputs, CLOCK OUT and the LEDs.
//
// Build: make emulator   (needs a clang with the wasm32 target; see web/emulator/README.md)

#define IDUM_SIM 1
#define ARDUINO 100

#include "Arduino.h"
#include "EEPROM.h"
#include "engine_config.h"

#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_BEATGEN
#error "The emulator is built for ALT_ENGINE=beatgen"
#endif

#define EXPORT extern "C" __attribute__((visibility("default")))

SerialStub Serial;
EEPROMClass EEPROM;

void setup();
void loop();
void trig1InterruptHandler();
void trig2InterruptHandler();
void trig3InterruptHandler();
void trig4InterruptHandler();

//---------------------------------------------------------------- freestanding libc bits
extern "C" {
void* memset(void* dst, int value, size_t n) {
  unsigned char* d = static_cast<unsigned char*>(dst);
  while (n--) *d++ = static_cast<unsigned char>(value);
  return dst;
}
void* memcpy(void* dst, const void* src, size_t n) {
  unsigned char* d = static_cast<unsigned char*>(dst);
  const unsigned char* s = static_cast<const unsigned char*>(src);
  while (n--) *d++ = *s++;
  return dst;
}
void* memmove(void* dst, const void* src, size_t n) {
  unsigned char* d = static_cast<unsigned char*>(dst);
  const unsigned char* s = static_cast<const unsigned char*>(src);
  if (d < s) {
    while (n--) *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--) *--d = *--s;
  }
  return dst;
}
size_t strlen(const char* s) {
  size_t n = 0;
  while (s[n]) n++;
  return n;
}
}

//---------------------------------------------------------------- simulated hardware
namespace {
unsigned long simMicros = 0;
uint8_t digitalPins[32];
int analogPins[32];
void (*pinHandlers[32])() = {};
uint32_t rngState = 1;
}

unsigned long micros() { return simMicros; }
unsigned long millis() { return simMicros / 1000; }
void delay(unsigned long ms) { simMicros += ms * 1000; }
void delayMicroseconds(unsigned int us) { simMicros += us; }
void pinMode(uint8_t, uint8_t) {}
int digitalRead(uint8_t pin) { return digitalPins[pin & 31]; }
void digitalWrite(uint8_t pin, uint8_t value) { digitalPins[pin & 31] = value; }
int analogRead(uint8_t pin) { return analogPins[pin & 31]; }
void attachInterrupt(uint8_t pin, void (*handler)(), int) { pinHandlers[pin & 31] = handler; }
long random(long max) {
  if (max <= 0) return 0;
  rngState = rngState * 1103515245u + 12345u;
  return static_cast<long>((rngState >> 1) % static_cast<uint32_t>(max));
}
long random(long min, long max) { return max <= min ? min : min + random(max - min); }
void randomSeed(unsigned long seed) { rngState = static_cast<uint32_t>(seed) | 1u; }
void noInterrupts() {}
void interrupts() {}

// Prototypes the firmware sources expect from the Arduino IDE's auto-generated list (copied
// from the host simulator, sim/idum_sim.cpp).
void assignPins();
void displayFirmwareBootSignature();
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


void setup();
void loop();
void runStockEngine();
void runSelectedAlternateEngine();
void resetSelectedAlternateEngine();
void runSuperlooperEngine();
void resetSuperlooperEngine();
void runBeatgenEngine();
void resetBeatgenEngine();
bool beatgenDisplay();
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
void displayFirmwareBootSignature();

#include "idum.ino"
#include "a0000_superSecretOptions.ino"
#include "a00_Variables.ino"
#include "a01_timers.ino"
#include "a05_AdcDiagnostic.ino"
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
#include "c10_BeatgenEngine.ino"
#include "x_utils.ino"

//---------------------------------------------------------------- emulator API
namespace {
const uint8_t kOutPins[5] = {7, 8, 6, 5, 13};  // OUT1-OUT4, CLOCK OUT
const uint8_t kTrigPins[4] = {14, 9, 11, 10};

bool logicalGate[32];          // what the page asked for, before the input inverter
bool clockRunning = false;
unsigned long clockPeriod = 125000;
unsigned long clockWidth = 5000;
unsigned long clockStart = 0;
bool manualClock = false;

uint32_t sampleFracQ16 = 0;    // fractional microseconds carried between samples
uint32_t isrAccumQ16 = 0;      // time toward the next 32 us TCB2 tick, Q16 microseconds
double out4Phase = 0;
uint8_t ledAddress = 0;
uint8_t buffer[16384];

void writePin(uint8_t pin, bool logicalHigh) {
  const uint8_t physical = logicalHigh ? LOW : HIGH;  // every input is inverted on the board
  const uint8_t previous = digitalPins[pin];
  digitalPins[pin] = physical;
  // attachInterrupt(..., RISING) fires on the physical pin's rising edge.
  if (previous == LOW && physical == HIGH && pinHandlers[pin]) pinHandlers[pin]();
}

void updateClockPin() {
  bool high = manualClock;
  if (clockRunning) {
    const unsigned long elapsed = (simMicros - clockStart) % clockPeriod;
    high = high || elapsed < clockWidth;
  }
  if (high != logicalGate[15]) {
    logicalGate[15] = high;
    writePin(15, high);
  }
}

uint8_t decodeRing() {
  // Inverse of writeModeMuxAddress(): mode1LED/mode2LED/mode3LED levels -> mux address.
  const uint8_t code = (digitalPins[2] ? 1 : 0) | (digitalPins[3] ? 2 : 0) | (digitalPins[4] ? 4 : 0);
  static const uint8_t address[8] = {7, 0, 1, 6, 2, 5, 3, 4};
  return address[code];
}
}  // namespace

EXPORT void emu_boot() {
  simMicros = 0;
  for (int i = 0; i < 32; i++) {
    digitalPins[i] = HIGH;
    analogPins[i] = 1024;
    pinHandlers[i] = nullptr;
    logicalGate[i] = false;
  }
  clockStart = 0;
  sampleFracQ16 = 0;
  isrAccumQ16 = 0;
  out4Phase = 0;
  setup();
}

// Gate, button or switch, as the patch sees it: true = high / pressed / CYCLE up.
EXPORT void emu_set_gate(int pin, int high) {
  if (pin < 0 || pin > 31) return;
  if (pin == 15) {
    manualClock = high != 0;
    updateClockPin();
    return;
  }
  logicalGate[pin] = high != 0;
  writePin(static_cast<uint8_t>(pin), high != 0);
}

// Knob plus CV as the firmware reads it after its own inversion: 0 (CCW) to 1024 (CW).
EXPORT void emu_set_analog(int pin, int value) {
  if (pin < 0 || pin > 31) return;
  if (value < 0) value = 0;
  if (value > 1024) value = 1024;
  analogPins[pin] = 1024 - value;
}

// Internal clock into CLOCK IN. periodUs 0 stops it.
EXPORT void emu_set_clock(unsigned long periodUs, unsigned long widthUs) {
  if (periodUs == 0) {
    clockRunning = false;
  } else {
    if (!clockRunning) clockStart = simMicros;
    clockRunning = true;
    clockPeriod = periodUs;
    clockWidth = widthUs < periodUs ? widthUs : periodUs / 2;
  }
  updateClockPin();
}

// Run the firmware for n samples at sampleRate. Each sample stores one byte: bits 0-3 are
// OUT1-OUT4 (1 = output high at the jack), bit 4 CLOCK OUT, bit 5 CLOCK IN.
EXPORT uint8_t* emu_run(int n, int sampleRate) {
  if (n > static_cast<int>(sizeof(buffer))) n = sizeof(buffer);
  const uint32_t stepQ16 = static_cast<uint32_t>((1000000ull << 16) / static_cast<unsigned>(sampleRate));
  for (int i = 0; i < n; i++) {
    sampleFracQ16 += stepQ16;
    simMicros += sampleFracQ16 >> 16;
    sampleFracQ16 &= 0xffff;
    updateClockPin();

    loop();

    uint8_t bits = 0;
    for (int c = 0; c < 5; c++) {
      if (digitalPins[kOutPins[c]] == LOW) bits |= 1 << c;  // outputs are inverted too
    }

    if (beatgenAudioOn) {
      // TCB2 fires every 32 us and steps three phase accumulators (OUT1-OUT3); TCA0 squares OUT4.
      isrAccumQ16 += stepQ16;
      while (isrAccumQ16 >= (32u << 16)) {
        isrAccumQ16 -= 32u << 16;
        for (int c = 0; c < 3; c++) beatgenPhase[c] += beatgenIncrement[c];
      }
      out4Phase += static_cast<double>(beatgenFreqQ8[3]) / 256.0 / sampleRate;
      if (out4Phase >= 1.0) out4Phase -= static_cast<int>(out4Phase);
      bits &= 0xf0;
      for (int c = 0; c < 3; c++) {
        if (!(beatgenPhase[c] & 0x8000)) bits |= 1 << c;
      }
      if (out4Phase < 0.5) bits |= 8;
    }

    if (logicalGate[15]) bits |= 0x20;
    buffer[i] = bits;
  }
  ledAddress = decodeRing();
  return buffer;
}

// bits 0-2: ring LED (mux address), bit 3: LOOP LED, bit 4: CYCLE (alternate engine) active,
// bit 5: VCO running.
EXPORT int emu_leds() {
  return ledAddress | (digitalPins[loopLED] ? 8 : 0) | (alternateEngineActive ? 16 : 0) | (beatgenAudioOn ? 32 : 0);
}

EXPORT unsigned long emu_micros() { return simMicros; }
EXPORT char* emu_bank() { return const_cast<char*>(beatgenBank); }
EXPORT int emu_bank_size() { return static_cast<int>(sizeof(beatgenBank)); }
EXPORT uint8_t* emu_eeprom() { return EEPROM.data; }
EXPORT int emu_out4_hz() { return static_cast<int>(beatgenFreqQ8[3] >> 8); }
