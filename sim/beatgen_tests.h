// Simulator scenarios for the Beatgen alternate engine (ALT_ENGINE=beatgen).
// Included at the end of idum_sim.cpp.

#include <cmath>

const uint8_t BG_CLOCK = 15, BG_TR1 = 14, BG_TR2 = 9, BG_TR3 = 11, BG_TR4 = 10;
const uint8_t BG_LOOP_BUTTON = 16;
const uint8_t BG_PROB = 20, BG_LENGTH = 18, BG_PARAM = 19;

//One clock: rising edge, sample what fired on that edge, then finish the period.
uint8_t bgClock(unsigned long interval = 250000) {
  setLogicalGate(BG_CLOCK, true);
  loop();
  uint8_t fired = beatgenOnMask;
  advanceMicros(100);
  runFor(4900);
  setLogicalGate(BG_CLOCK, false);
  runFor(interval - 5000);
  return fired;
}

void bgControls(unsigned int slot, int prob, int length, int param) {
  setModeCv(slot);
  setLogicalAnalog(BG_PROB, prob);
  setLogicalAnalog(BG_LENGTH, length);
  setLogicalAnalog(BG_PARAM, param);
  runFor(2000);
}

//LENGTH readings that land on each length position (4,6,8,10,12,16,24,32).
const int bgLengthCv[8] = { 20, 90, 200, 420, 600, 780, 940, 1024 };

int bgCountBank(uint8_t pattern, uint8_t channel, uint8_t length, bool coreOnly) {
  int count = 0;
  for (uint8_t s = 0; s < length; s++) {
    uint8_t w = beatgenBankWeight(pattern, channel, s);
    if (coreOnly ? (w == 255) : (w > 0)) count++;
  }
  return count;
}

std::string bgCounts(const int* counts) {
  return std::to_string(counts[0]) + "/" + std::to_string(counts[1]) + "/"
       + std::to_string(counts[2]) + "/" + std::to_string(counts[3]);
}

//Clock-only house at PROB CCW plays exactly the core ('X') hits of the bank.
int bgClockOnlyScenario() {
  std::string detail;
  return runModeScenario("beatgen-clock-only", [&]() {
    bgControls(0, 0, bgLengthCv[7], 0);
    int expected[4], got[4] = {};
    for (uint8_t c = 0; c < 4; c++) expected[c] = bgCountBank(0, c, 32, true);
    for (int i = 0; i < 32; i++) {
      uint8_t m = bgClock();
      for (uint8_t c = 0; c < 4; c++) if (m & (1 << c)) got[c]++;
    }
    detail = "expected " + bgCounts(expected) + " got " + bgCounts(got)
           + " clock-out " + std::to_string(countOutputOnsets(13));
    bool same = true;
    for (uint8_t c = 0; c < 4; c++) same = same && got[c] == expected[c];
    return same && expected[0] == 8 && countOutputOnsets(13) == 1
        && countOutputOnsets(7) == expected[0] && countOutputOnsets(5) == expected[3];
  }, "no trigger inputs patched; core hits only", "wrong hits");
}

//PROB fully CW plays every non-rest cell.
int bgDensityScenario() {
  return runModeScenario("beatgen-density", [&]() {
    bgControls(1, 1024, bgLengthCv[7], 0);
    int expected[4], got[4] = {};
    for (uint8_t c = 0; c < 4; c++) expected[c] = bgCountBank(1, c, 32, false);
    for (int i = 0; i < 32; i++) {
      uint8_t m = bgClock();
      for (uint8_t c = 0; c < 4; c++) if (m & (1 << c)) got[c]++;
    }
    std::cout << "      techno full density " << bgCounts(got) << " (bank " << bgCounts(expected) << ")\n";
    for (uint8_t c = 0; c < 4; c++) if (got[c] != expected[c]) return false;
    return true;
  }, "PROB CW plays all optional hits", "density mismatch");
}

//LENGTH sets the loop; CLOCK OUT marks each loop start.
int bgLengthScenario() {
  return runModeScenario("beatgen-length", [&]() {
    bool ok = true;
    for (uint8_t position = 0; position < 8; position++) {
      bgControls(0, 0, bgLengthCv[position], 0);
      if (lengthPosition != position) {
        std::cout << "      length position " << (int)lengthPosition << " wanted " << (int)position << '\n';
        return false;
      }
      resetBeatgenEngine();
      writes.clear();
      uint8_t length = beatgenLengths[position];
      for (int i = 0; i < length * 3; i++) bgClock(100000);
      int marks = countOutputOnsets(13);
      if (marks != 3) {
        std::cout << "      length " << (int)length << ": " << marks << " loop marks\n";
        ok = false;
      }
    }
    return ok;
  }, "all 8 lengths loop on time", "loop length wrong");
}

//TR1 in resets: the next clock is step 0 again.
int bgResetScenario() {
  return runModeScenario("beatgen-reset", [&]() {
    bgControls(0, 0, bgLengthCv[7], 0);
    for (int i = 0; i < 5; i++) bgClock();
    bool notAtTop = beatgenStep == 4;
    pulseGate(BG_TR1, 2000);
    writes.clear();
    uint8_t m = bgClock();
    return notAtTop && beatgenStep == 0 && (m & 0x10) && countOutputOnsets(13) == 1;
  }, "TR1 restarts the pattern", "reset did not restart");
}

//With PARAM up the variation changes loop to loop; LOOP locks it.
int bgLockScenario() {
  return runModeScenario("beatgen-lock", [&]() {
    bgControls(2, 520, bgLengthCv[5], 900);
    auto loopPattern = [&]() {
      std::vector<uint8_t> pattern;
      for (int i = 0; i < 16; i++) pattern.push_back(bgClock());
      return pattern;
    };
    std::vector<uint8_t> first = loopPattern();
    bool changed = false;
    for (int i = 0; i < 6 && !changed; i++) changed = loopPattern() != first;

    pulseGate(BG_LOOP_BUTTON, 2000);  //lock
    bool lockedLed = loopEnable;
    std::vector<uint8_t> a = loopPattern();  //the loop that started before the lock
    a = loopPattern();
    std::vector<uint8_t> b = loopPattern();
    std::vector<uint8_t> c = loopPattern();
    bool repeats = a == b && b == c;

    pulseGate(BG_LOOP_BUTTON, 2000);  //unlock
    std::cout << "      unlocked varies=" << changed << " locked repeats=" << repeats
              << " led=" << lockedLed << "/" << loopEnable << '\n';
    return changed && repeats && lockedLed && !loopEnable;
  }, "variation moves, LOOP freezes it", "lock failed");
}

//TR2 held: fill with a ratcheted second voice (two hits per clock).
int bgFillScenario() {
  return runModeScenario("beatgen-fill", [&]() {
    bgControls(0, 0, bgLengthCv[7], 0);
    bgClock();
    setLogicalGate(BG_TR2, true);
    writes.clear();
    for (int i = 0; i < 8; i++) bgClock();
    setLogicalGate(BG_TR2, false);
    int snare = countOutputOnsets(8);
    std::cout << "      fill snare hits over 8 clocks: " << snare << '\n';
    return snare == 16;
  }, "TR2 rolls the snare", "fill wrong");
}

//TR4 held: breakdown, only OUT1 plays.
int bgBreakdownScenario() {
  return runModeScenario("beatgen-breakdown", [&]() {
    bgControls(1, 1024, bgLengthCv[7], 0);
    setLogicalGate(BG_TR4, true);
    for (int i = 0; i < 32; i++) bgClock();
    setLogicalGate(BG_TR4, false);
    return countOutputOnsets(7) > 0 && countOutputOnsets(8) == 0
        && countOutputOnsets(6) == 0 && countOutputOnsets(5) == 0;
  }, "TR4 mutes OUT2-4", "breakdown leaked");
}

//Euclid slot: evenly spread hits; counts follow PROB.
int bgEuclidScenario() {
  return runModeScenario("beatgen-euclid", [&]() {
    bgControls(4, 0, bgLengthCv[5], 0);
    int got[4] = {};
    for (int i = 0; i < 16; i++) {
      uint8_t m = bgClock();
      for (uint8_t c = 0; c < 4; c++) if (m & (1 << c)) got[c]++;
    }
    int full[4] = {};
    bgControls(4, 1024, bgLengthCv[5], 0);
    for (int i = 0; i < 16; i++) {
      uint8_t m = bgClock();
      for (uint8_t c = 0; c < 4; c++) if (m & (1 << c)) full[c]++;
    }
    std::cout << "      euclid PROB CCW " << bgCounts(got) << ", CW " << bgCounts(full) << '\n';
    return got[0] == 4 && got[1] == 2 && got[2] == 8 && got[3] == 1
        && full[0] == 8 && full[1] == 6 && full[2] == 16 && full[3] == 7;
  }, "4/2/8/1 hits at PROB CCW", "euclid counts wrong");
}

//Generative slot: plays something from the clock alone and re-rolls with PARAM.
int bgGenerativeScenario() {
  return runModeScenario("beatgen-generative", [&]() {
    bgControls(5, 600, bgLengthCv[5], 0);
    std::vector<uint8_t> first, later;
    for (int i = 0; i < 16; i++) first.push_back(bgClock());
    for (int i = 0; i < 16; i++) later.push_back(bgClock());
    bool steady = first == later;  //PARAM CCW: keeps the same pattern
    int hits = 0;
    for (uint8_t m : first) hits += __builtin_popcount(m & 0x0f);
    bgControls(5, 600, bgLengthCv[5], 1024);
    bool rerolled = false;
    for (int loopIndex = 0; loopIndex < 4 && !rerolled; loopIndex++) {
      std::vector<uint8_t> next;
      for (int i = 0; i < 16; i++) next.push_back(bgClock());
      rerolled = next != first;
    }
    std::cout << "      generative hits/loop " << hits << " steady=" << steady << " rerolled=" << rerolled << '\n';
    return hits > 4 && steady && rerolled;
  }, "steady at PARAM CCW, re-rolls at CW", "generative wrong");
}

//Software oscillator check: run the TCB2 interrupt maths for one second.
int bgCountDdsCycles(uint16_t increment) {
  uint16_t phase = 0;
  int rising = 0;
  bool old = false;
  for (uint32_t i = 0; i < BEATGEN_DDS_RATE; i++) {
    phase += increment;
    bool high = phase & 0x8000;
    if (high && !old) rising++;
    old = high;
  }
  return rising;
}

double bgHz(uint8_t output) {
  return beatgenFreqQ8[output] / 256.0;
}

//VCO slot: PARAM sweeps pitch over 8 octaves; LENGTH makes chords.
int bgVcoScenario() {
  return runModeScenario("beatgen-vco", [&]() {
    bgControls(7, 0, bgLengthCv[2], 0);
    bool on = beatgenAudioOn;
    double low = bgHz(3);
    setLogicalAnalog(BG_PARAM, 512);
    runFor(5000);
    double mid = bgHz(3);
    double fifth = bgHz(0) / bgHz(3);
    double octave = bgHz(1) / bgHz(3);
    int measured = bgCountDdsCycles(beatgenIncrement[0]);
    double expectedOut1 = bgHz(0);
    setLogicalAnalog(BG_PARAM, 1024);
    runFor(5000);
    double high = bgHz(3);
    bool quietTriggers = countOutputOnsets(7) == 0 && countOutputOnsets(13) == 0;

    bgControls(0, 0, bgLengthCv[7], 0);  //back to a beat slot
    bool off = !beatgenAudioOn;
    bgClock();
    bool beatsBack = countOutputOnsets(7) == 1;
    std::cout << "      vco " << low << " Hz / " << mid << " Hz / " << high << " Hz; fifth x" << fifth
              << ", octave x" << octave << ", OUT1 software osc " << measured << " Hz\n";
    return on && low > 15 && low < 17 && mid > 250 && mid < 262 && high > 4000 && high < 4200
        && fifth > 1.49 && fifth < 1.51 && octave > 1.99 && octave < 2.01
        && std::abs(measured - expectedOut1) < 2 && quietTriggers && off && beatsBack;
  }, "16 Hz to 4 kHz, chords, clean hand-off back to beats", "vco wrong");
}

//Flipping CYCLE back to stock while the VCO runs must stop the oscillators.
int bgVcoExitScenario() {
  return runModeScenario("beatgen-vco-exit", [&]() {
    bgControls(7, 0, bgLengthCv[0], 512);
    bool on = beatgenAudioOn;
    setLogicalGate(17, false);
    runFor(2000);
    return on && !beatgenAudioOn && !alternateEngineActive;
  }, "CYCLE to stock stops the VCO", "VCO kept running");
}

int beatgenScenario(const std::string& name) {
  forceAlternateRunner = true;
  if (name == "beatgen-clock-only") return bgClockOnlyScenario();
  if (name == "beatgen-density") return bgDensityScenario();
  if (name == "beatgen-length") return bgLengthScenario();
  if (name == "beatgen-reset") return bgResetScenario();
  if (name == "beatgen-lock") return bgLockScenario();
  if (name == "beatgen-fill") return bgFillScenario();
  if (name == "beatgen-breakdown") return bgBreakdownScenario();
  if (name == "beatgen-euclid") return bgEuclidScenario();
  if (name == "beatgen-generative") return bgGenerativeScenario();
  if (name == "beatgen-vco") return bgVcoScenario();
  if (name == "beatgen-vco-exit") return bgVcoExitScenario();
  if (name == "beatgen-all") {
    static const char* all[] = {
      "beatgen-clock-only", "beatgen-density", "beatgen-length", "beatgen-reset",
      "beatgen-lock", "beatgen-fill", "beatgen-breakdown", "beatgen-euclid",
      "beatgen-generative", "beatgen-vco", "beatgen-vco-exit",
      "regression-alternate-engine-boundary"
    };
    int failures = 0;
    for (const char* scenario : all) failures += runChildScenario(scenario) != 0;
    std::cout << (failures ? "FAIL" : "PASS") << " beatgen-all (" << failures << " failing)\n";
    return failures ? 1 : 0;
  }
  std::cerr << "Unknown beatgen scenario: " << name << '\n';
  return 2;
}
