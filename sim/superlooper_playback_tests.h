//Included after the firmware so tests can inspect state without duplicating
//its type declarations. Input-driven tests exercise the real loop dispatcher.

int superlooperPlaybackMappingScenario() {
  prepareSuperlooperTransportIO(0);
  bool forward = true, bidirectional = true, smooth = true, hysteresis = true;
  superlooperPlaybackOption = 1;
  superlooperSpeedPickup = false;
  for (int index = 0; index < 7; ++index) {
    superlooperSpeedBin = -1;
    forward &= superlooperSelectedLoopSpeed((index * 1025 + 512) / 7) == (32 << index);
  }
  superlooperPlaybackOption = 3;
  const int expected[15] = {-2048,-1024,-512,-256,-128,-64,-32,0,32,64,128,256,512,1024,2048};
  for (int index = 0; index < 15; ++index) {
    superlooperSpeedBin = -1;
    bidirectional &= superlooperSelectedLoopSpeed((index * 1025 + 512) / 15) == expected[index];
  }
  superlooperSpeedBin = -1;
  hysteresis &= superlooperSelectedLoopSpeed(512) == 0;
  hysteresis &= superlooperSelectedLoopSpeed(550) == 0;
  hysteresis &= superlooperSelectedLoopSpeed(556) == 32;
  hysteresis &= superlooperSelectedLoopSpeed(541) == 32;
  hysteresis &= superlooperSelectedLoopSpeed(536) == 0;
  for (int option : {0, 2}) {
    superlooperPlaybackOption = option;
    int previous = -2049;
    for (int control = 0; control <= 1024; ++control) {
      int speed = superlooperSelectedLoopSpeed(control);
      smooth &= speed >= previous;
      previous = speed;
    }
  }
  smooth &= superlooperSelectedLoopSpeed(0) == -2048
    && superlooperSelectedLoopSpeed(480) == 0
    && superlooperSelectedLoopSpeed(544) == 0
    && superlooperSelectedLoopSpeed(1024) == 2048;
  superlooperSpeedPickup = true;
  bool pickup = superlooperSelectedLoopSpeed(0) == 256
    && superlooperSelectedLoopSpeed(1024) == 256;
  return reportFirmwareRegression("superlooper-playback-mapping",
    forward && bidirectional && smooth && hysteresis && pickup,
    "forward/bidirectional/smooth/hysteresis/pickup=" + std::to_string(forward) + "/"
      + std::to_string(bidirectional) + "/" + std::to_string(smooth) + "/"
      + std::to_string(hysteresis) + "/" + std::to_string(pickup));
}

void playbackMenuHold() {
  setLogicalGate(modeBtn, true);
  runFor(1010000);
  setLogicalGate(modeBtn, false);
  runFor(2000);
}

int superlooperPlaybackMenuScenario() {
  prepareSuperlooperTransportIO(0);
  setLogicalAnalog(paramPin, 900);
  runFor(1000);
  unsigned int mutationParam = superlooperMutationParam;
  playbackMenuHold();
  bool guarded = menuMode && superlooperPlaybackOption == 0;
  setLogicalAnalog(paramPin, 905);
  runFor(1000);
  guarded &= superlooperPlaybackOption == 0;
  bool regions = true;
  for (int option = 0; option < 4; ++option) {
    setLogicalAnalog(paramPin, option * 256 + 128);
    runFor(1000);
    regions &= superlooperPlaybackOption == option && superlooperMenuDisplayLed == option * 2;
  }
  setLogicalAnalog(lengPin, 250);
  runFor(1000);
  bool independent = superlooperNOption == 2 && superlooperPlaybackOption == 3
    && superlooperMutationParam == mutationParam;
  playbackMenuHold();
  bool saved = !menuMode && EEPROM.read(8) == 3 && EEPROM.read(9) == 0x53
    && EEPROM.read(6) == 2 && superlooperParamPickup;
  setLogicalGate(cycleSw, false);
  runFor(1000);
  setLogicalGate(cycleSw, true);
  runFor(1000);
  saved &= superlooperPlaybackOption == 3 && superlooperClocksPerStep == 3;
  EEPROM.write(8, 255);
  superlooperLoadPlaybackSetting();
  bool fallback = superlooperPlaybackOption == 0;
  EEPROM.write(8, 3);
  EEPROM.write(9, 0);
  superlooperLoadPlaybackSetting();
  fallback &= superlooperPlaybackOption == 0;
  return reportFirmwareRegression("superlooper-playback-menu",
    guarded && regions && independent && saved && fallback,
    "guarded/regions/independent/saved/fallback=" + std::to_string(guarded) + "/"
      + std::to_string(regions) + "/" + std::to_string(independent) + "/"
      + std::to_string(saved) + "/" + std::to_string(fallback));
}

//Use actual LOOP requests and clocks: restoring the PARAM bank alone does
//not prove that the transport is running at the retained speed.
int superlooperPlaybackRecallScenario(int option) {
  prepareSuperlooperTransportIO(0);
  EEPROM.write(8, option);
  EEPROM.write(9, 0x53);
  superlooperLoadPlaybackSetting();
  unsigned long nextClock = micros() + 100000;
  for (int i = 0; i < 9; ++i) transportIOClock(nextClock);
  auto changeLoop = [&](bool active, bool gate) {
    loopInputBehavior = gate;
    if (gate) setLogicalGate(12, active);
    else pulseGate(loopButton, 4000, 100);
    transportIOClock(nextClock);
  };
  changeLoop(true, false);
  bool initial = loopEnable && superlooperLoopSpeedQ8 == 256;
  bool recalled = true, jitter = true, pickup = true, mutation = true;
  for (int control : {0, 1024, 512, option == 3 ? 550 : 590}) {
    setLogicalAnalog(paramPin, control);
    setLogicalAnalog(lengPin, 500);
    runFor(2000);
    int speed = superlooperLoopSpeedQ8;
    unsigned span = superlooperLoopLength;
    int bin = superlooperSpeedBin;
    bool gate = control >= 512;
    //A gate needs a rising edge before it can request its falling-edge exit.
    if (gate) { loopInputBehavior = true; setLogicalGate(12, true); runFor(2000); }
    unsigned mutationParam = superlooperMutationParam;
    unsigned mutationLength = superlooperMutationLength;
    changeLoop(false, gate);
    mutation &= !loopEnable && modifyParam == mutationParam && lengthScaled == mutationLength;
    setLogicalAnalog(paramPin, 200);
    setLogicalAnalog(lengPin, 100);
    runFor(2000);
    mutation &= modifyParam == 200 && lengthScaled == superlooperPhysicalLength;
    changeLoop(true, gate);
    recalled &= loopEnable && superlooperLoopSpeedQ8 == speed
      && superlooperSpeedBin == bin && loopLength == span;
    setLogicalAnalog(paramPin, 205);
    setLogicalAnalog(lengPin, 105);
    runFor(2000);
    jitter &= superlooperLoopSpeedQ8 == speed && loopLength == span
      && superlooperParamPickup && superlooperLengthPickup;
    setLogicalAnalog(paramPin, 800);
    setLogicalAnalog(lengPin, 900);
    runFor(2000);
    pickup &= !superlooperParamPickup && !superlooperLengthPickup
      && superlooperLoopParam == 800 && superlooperLoopLength == lengthPosition + 1;
    //Approach the pause/step boundary from below on the final case.
    if (control == 512) { setLogicalAnalog(paramPin, 512); runFor(2000); }
  }
  return reportFirmwareRegression("superlooper-playback-recall-" + std::to_string(option),
    initial && recalled && jitter && pickup && mutation,
    "initial/recall/jitter/pickup/mutation=" + std::to_string(initial) + "/"
      + std::to_string(recalled) + "/" + std::to_string(jitter) + "/"
      + std::to_string(pickup) + "/" + std::to_string(mutation));
}

int superlooperReverseEntryScenario(int nOption, int phase, int span, int option, bool tracking) {
  prepareSuperlooperTransportIO(nOption);
  externalSequencerTracking = tracking;
  EEPROM.write(8, option);
  EEPROM.write(9, 0x53);
  superlooperLoadPlaybackSetting();
  setLogicalAnalog(lengPin, span == 1 ? 0 : span == 4 ? 400 : 1024);
  runFor(1000);
  int n = superlooperClocksPerStep;
  unsigned long nextClock = micros() + 100000;
  for (int i = 0; i < 9 * n; ++i) transportIOClock(nextClock);
  //Select reverse during a first LOOP, then test the real recalled entry.
  pulseGate(loopButton, 4000, 100);
  transportIOClock(nextClock);
  setLogicalAnalog(paramPin, 0);
  runFor(1000);
  pulseGate(loopButton, 4000, 100);
  transportIOClock(nextClock);
  //Record a short trigger on every raw clock, followed by silence. Fill all
  //nine stages so no fixture tape or transport state is injected by the test.
  for (int i = 0; i < 10 * n; ++i) {
    runUntil(nextClock);
    setLogicalGate(14, true);
    setLogicalGate(15, true);
    runFor(10000);
    setLogicalGate(14, false);
    setLogicalGate(15, false);
    runFor(1000);
    nextClock += 100000;
  }
  while ((superlooperSequencerClockPhase + 1) % n != phase) {
    runUntil(nextClock);
    setLogicalGate(14, true);
    setLogicalGate(15, true);
    runFor(10000);
    setLogicalGate(14, false);
    setLogicalGate(15, false);
    runFor(1000);
    nextClock += 100000;
  }
  pulseGate(loopButton, 4000, 100);
  runUntil(nextClock);
  unsigned beforeStage = superlooperTrackedSequencerStage;
  int clockOnsets = countOutputOnsets(clockOut);
  int triggerOnsets = countOutputOnsets(trig1Out);
  setLogicalGate(15, true);
  runFor(100); //Observe the first output before even one reverse cell elapses.
  unsigned startStage = span - 1 - ((phase == 0 && span > 1) ? 1 : 0);
  unsigned wantedStage = (superlooperLoopNaturalStage + 8 - span + startStage) % 8;
  unsigned wantedClocks = (wantedStage + 8 - beforeStage) % 8;
  if (!wantedClocks) wantedClocks = 8; //L=1 retriggers the one captured stage.
  bool start = loopEnable && superlooperLoopSpeedQ8 == -2048 && loopLength == span
    && superlooperLoopBaseCell == startStage * n * 32;
  bool aligned = !tracking || (superlooperTrackedSequencerStage == wantedStage
    && superlooperQueuedSequencerClocks + countOutputOnsets(clockOut) - clockOnsets == wantedClocks);
  bool entryHit = countOutputOnsets(trig1Out) == triggerOnsets + 1;
  runFor(12400); //No second hit before the next raw-clock beat at -8x.
  bool singleHit = countOutputOnsets(trig1Out) == triggerOnsets + 1;
  runFor(100);
  singleHit &= countOutputOnsets(trig1Out) == triggerOnsets + 2;
  setLogicalGate(15, false);
  nextClock += 100000;
  pulseGate(loopButton, 4000, 100);
  transportIOClock(nextClock);
  runFor(40000);
  bool exit = !loopEnable && (!tracking ||
    (superlooperTrackedSequencerStage == superlooperLoopNaturalStage
      && (countOutputOnsets(clockOut) + 7) % 8 == superlooperLoopNaturalStage));
  return reportFirmwareRegression("superlooper-reverse-entry",
    start && aligned && entryHit && singleHit && exit,
    "N/phase/L/style/tracking=" + std::to_string(n) + "/" + std::to_string(phase)
      + "/" + std::to_string(span) + "/" + std::to_string(option) + "/" + std::to_string(tracking)
      + ", start/aligned/entry-hit/beat-spacing/exit=" + std::to_string(start) + "/"
      + std::to_string(aligned) + "/" + std::to_string(entryHit) + "/"
      + std::to_string(singleHit) + "/" + std::to_string(exit));
}

int superlooperReverseEntriesScenario() {
  for (int nOption : {0, 2, 5}) {
    int n = nOption == 0 ? 1 : nOption == 2 ? 3 : 8;
    for (int phase = 0; phase < n; ++phase)
      for (int span : {1, 4, 8})
        for (int option : {2, 3})
          for (int tracking : {0, 1}) {
            std::string name = "reverse-entry-case-" + std::to_string(nOption) + "-"
              + std::to_string(phase) + "-" + std::to_string(span) + "-"
              + std::to_string(option) + "-" + std::to_string(tracking);
            if (runChildScenario(name) != 0) return 1;
          }
  }
  return 0;
}

int superlooperReverseRhythmScenario() {
  prepareSuperlooperTransportIO(0);
  EEPROM.write(8, 3);
  EEPROM.write(9, 0x53);
  superlooperLoadPlaybackSetting();
  unsigned long nextClock = micros() + 100000;
  for (int i = 0; i < 9; ++i) transportIOClock(nextClock);
  pulseGate(loopButton, 4000, 100);
  transportIOClock(nextClock);
  setLogicalAnalog(paramPin, 0); //Recall -8x with L=1.
  runFor(1000);
  pulseGate(loopButton, 4000, 100);
  transportIOClock(nextClock);
  const int inputs[4] = {14,9,11,10}, outputs[4] = {7,8,6,5};
  const std::vector<std::pair<int,int>> pulses[4] = {
    {{0,1},{16,1},{24,1},{28,1}}, //Accelerating bounce.
    {{0,3}},                     //Plain on-beat trigger.
    {{0,10},{16,6},{24,2},{28,3}},//Unequal widths overlap when reversed.
    {{3,2},{11,2},{26,2}}        //Off-grid hits must stay off grid.
  };
  for (int raw = 0; raw < 10; ++raw) {
    runUntil(nextClock);
    for (int tick = 0; tick < 32; ++tick) {
      setLogicalGate(15, tick == 0);
      for (int ch = 0; ch < 4; ++ch) {
        bool high = false;
        for (auto pulse : pulses[ch]) high |= tick >= pulse.first && tick < pulse.first + pulse.second;
        setLogicalGate(inputs[ch], high);
      }
      runFor(raw == 9 && tick == 31 ? 1000 : 10000, 50);
    }
    nextClock += 320000;
  }
  pulseGate(loopButton, 4000, 50);
  runUntil(nextClock, 50);
  const unsigned long entry = micros();
  setLogicalGate(15, true);
  runFor(10000, 50);
  setLogicalGate(15, false);
  runUntil(entry + 80000, 50); //Two complete reversed phrases, endpoint excluded.
  bool lanes[4] = {};
  for (int ch = 0; ch < 4; ++ch) {
    std::vector<unsigned long> expected, actual;
    for (int repeat = 0; repeat < 2; ++repeat)
      for (auto pulse : pulses[ch]) expected.push_back(repeat * 40000 + ((32-pulse.first)%32) * 1250);
    std::sort(expected.begin(), expected.end());
    for (auto write : writes)
      if (write.pin == outputs[ch] && write.value == LOW && write.time >= entry)
        actual.push_back(write.time - entry);
    lanes[ch] = actual.size() == expected.size();
    for (size_t j = 0; lanes[ch] && j < expected.size(); ++j)
      lanes[ch] &= actual[j] >= expected[j] && actual[j] <= expected[j] + 100;
    std::cout << "reverse-onsets channel " << ch+1 << " expected/actual:";
    for (auto t : expected) std::cout << ' ' << t;
    std::cout << " /";
    for (auto t : actual) std::cout << ' ' << t;
    std::cout << '\n';
  }
  return reportFirmwareRegression("superlooper-reverse-rhythm", lanes[0] && lanes[1] && lanes[2] && lanes[3],
    "bounce/steady/unequal-widths/off-grid=" + std::to_string(lanes[0]) + "/"
      + std::to_string(lanes[1]) + "/" + std::to_string(lanes[2]) + "/" + std::to_string(lanes[3]));
}

int superlooperReverseTapeScenario() {
  prepareSuperlooperTransportIO(2); //N=3, four-stage phrase inside nine-stage history.
  loopEnable = true;
  superlooperCaptureStartIndex = 9;
  superlooperConfigureLoopLength(4);
  for (int raw = 0; raw < 27; ++raw) {
    superlooperTransportStageAtStart[raw] = (raw * 3) % 8;
    for (int packed = 0; packed < 16; ++packed) {
      superlooperTransportTape[raw][packed] = ((raw + packed) % 4) | (((raw + packed + 1) % 4) << 4);
    }
    for (int channel = 0; channel < 4; ++channel) {
      superlooperRenderedTape[raw][channel] = UINT32_C(0xa500005a) ^ ((uint32_t)raw << (channel + 2));
    }
  }
  //Independent oracle: checkpoint plus all events up to the destination cell.
  auto stageAt = [](unsigned cell) {
    unsigned raw = (9 + (4 * 96 + cell) / 32) % 27;
    unsigned tick = cell % 32;
    unsigned stage = superlooperTransportStageAtStart[raw];
    for (unsigned t = 0; t <= tick; ++t) {
      unsigned packed = superlooperTransportTape[raw][t / 2];
      stage += (t & 1) ? packed >> 4 : packed & 15;
    }
    return stage % 8;
  };
  auto maskAt = [](unsigned cell) {
    unsigned raw = (9 + (4 * 96 + cell) / 32) % 27;
    uint8_t mask = 0;
    for (int ch = 0; ch < 4; ++ch) {
      if ((superlooperRenderedTape[raw][ch] >> (cell % 32)) & 1) mask |= 1 << ch;
    }
    return mask;
  };
  superlooperPlayTransportCell(0);
  bool transport = true, triggers = true;
  for (int direction : {-1, 1, -1}) {
    superlooperLoopSpeedQ8 = direction * 256;
    superlooperLoopReverse = direction < 0;
    superlooperResetReverseGates();
    superlooperAlignTransportCell(superlooperLoopBaseCell, false);
    unsigned expectedStage = stageAt(superlooperLoopBaseCell);
    for (int i = 0; i < 400; ++i) {
      unsigned before = superlooperLoopBaseCell;
      unsigned expected = (before + (direction > 0 ? 1 : 383)) % 384;
      superlooperLoopCrossedTriggerMask = 0;
      superlooperAdvanceLoopCell();
      unsigned raw = (9 + (4 * 96 + expected) / 32) % 27;
      unsigned tick = expected % 32;
      unsigned packed = superlooperTransportTape[raw][tick / 2];
      unsigned event = (tick & 1) ? packed >> 4 : packed & 15;
      if (direction > 0 || event) expectedStage = stageAt(expected);
      transport &= superlooperLoopBaseCell == expected
        && superlooperTrackedSequencerStage == expectedStage;
      unsigned previousForOnset = direction < 0 ? (expected + 383) % 384 : before;
      triggers &= superlooperRenderedLoopCellState(expected) == maskAt(expected)
        && superlooperLoopCrossedTriggerMask == (maskAt(expected) & ~maskAt(previousForOnset));
    }
  }
  //Length changes restart at the correct end and reconstruct every transport
  //event in the destination raw clock, including mutations within that clock.
  superlooperLoopSpeedQ8 = -256;
  superlooperLoopReverse = true;
  superlooperConfigureLoopLength(2);
  superlooperPlayLoopStartTransport();
  bool resize = superlooperLoopBaseCell == 96
    && superlooperTrackedSequencerStage == stageAt(288)
    && superlooperRenderedLoopCellState(96) == maskAt(288);
  superlooperLoopSpeedQ8 = 256;
  superlooperLoopReverse = false;
  superlooperConfigureLoopLength(4);
  superlooperPlayLoopStartTransport();
  resize &= superlooperLoopBaseCell == 0
    && superlooperTrackedSequencerStage == stageAt(0);
  unsigned beforePause = superlooperLoopBaseCell;
  superlooperPlaybackOption = 3;
  superlooperSpeedPickup = false;
  superlooperSpeedBin = -1;
  modifyParam = 512;
  lastClockInterval = 100000;
  superlooperLoopTransportLastTime = micros();
  advanceMicros(10000);
  bool paused = superlooperUpdateLoopPlayback() == 0 && superlooperLoopBaseCell == beforePause;
  return reportFirmwareRegression("superlooper-reverse-tape", transport && triggers && paused && resize,
    "transport/triggers/pause/resize=" + std::to_string(transport) + "/"
      + std::to_string(triggers) + "/" + std::to_string(paused) + "/" + std::to_string(resize));
}

int superlooperPlaybackExitScenario(int nOption, int phase, int option, int control) {
  prepareSuperlooperTransportIO(nOption);
  EEPROM.write(8, option);
  EEPROM.write(9, 0x53);
  superlooperLoadPlaybackSetting();
  int n = superlooperClocksPerStep;
  unsigned long step = 2000; //Deliberately slow service stresses reverse clock backlog.
  setLogicalAnalog(lengPin, 1024);
  unsigned long nextClock = micros() + 100000;
  for (int i = 0; i < 9 * n; ++i) transportIOClock(nextClock, step);
  pulseGate(loopButton, 4000, step);
  transportIOClock(nextClock, step);
  bool enteredAtUnity = loopEnable && superlooperLoopSpeedQ8 == 256;
  if (control == 512) {
    setLogicalAnalog(paramPin, 1024);
    runFor(2000, step);
  }
  setLogicalAnalog(paramPin, control);
  runFor(2000, step);
  int wanted = control == 0 ? -2048 : control == 512 ? 0 : 2048;
  bool selected = superlooperLoopSpeedQ8 == wanted;
  for (int i = 0; i < n + phase - 1; ++i) transportIOClock(nextClock, step);
  pulseGate(loopButton, 4000, step);
  unsigned queuedBefore = superlooperQueuedSequencerClocks;
  transportIOClock(nextClock, step);
  runFor(40000, step);
  int physicalStage = (countOutputOnsets(clockOut) + 7) % 8;
  //9*N dry edges, one entry edge, then N+phase elapsed edges: reference step 2.
  bool aligned = !loopEnable && physicalStage == 2
    && superlooperTrackedSequencerStage == 2 && superlooperLoopNaturalStage == 2
    && superlooperSequencerClockPhase == phase && superlooperQueuedSequencerClocks == 0;
  int before = countOutputOnsets(clockOut);
  for (int i = 0; i < n - phase - 1; ++i) transportIOClock(nextClock, step);
  bool phaseKept = countOutputOnsets(clockOut) == before;
  transportIOClock(nextClock, step);
  phaseKept &= countOutputOnsets(clockOut) == before + 1;
  return reportFirmwareRegression("superlooper-playback-exit",
    enteredAtUnity && selected && aligned && phaseKept,
    "N/phase/option/param=" + std::to_string(n) + "/" + std::to_string(phase) + "/"
      + std::to_string(option) + "/" + std::to_string(control)
      + ", physical=" + std::to_string(physicalStage) + ", queued=" + std::to_string(queuedBefore)
      + ", unity/selected/aligned/phase=" + std::to_string(enteredAtUnity) + "/"
      + std::to_string(selected) + "/" + std::to_string(aligned) + "/" + std::to_string(phaseKept));
}

int superlooperPlaybackExitsScenario() {
  const int ns[] = {1,2,3,4,6,8};
  for (int option : {2,3}) {
    for (int n = 0; n < 6; ++n) {
      for (int phase = 0; phase < ns[n]; ++phase) {
        for (int control : {0,512,1024}) {
          std::string name = "playback-exit-case-" + std::to_string(n) + "-"
            + std::to_string(phase) + "-" + std::to_string(option) + "-" + std::to_string(control);
          if (runChildScenario(name) != 0) return 1;
        }
      }
    }
  }
  return 0;
}
