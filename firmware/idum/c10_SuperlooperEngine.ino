//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// Selected alternate runner.
//
// This file is the only runtime seam between stock IDUM and an alternate
// personality. Future alternates provide the same two functions in their own
// compile-time branch. They may duplicate and change core sequencing here
// without adding policy hooks to the classic sequencing functions.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER

// Superlooper treats Param and Length as two virtual control banks. On
// either side of a LOOP transition the newly active bank remains in charge
// until the corresponding physical control is deliberately moved.
const unsigned int superlooperPickupThreshold = 12;

bool superlooperControlsInitialized = 0;
bool superlooperParamPickup = 0;
bool superlooperLengthPickup = 0;

unsigned int superlooperPickupParamReference = 0;
unsigned int superlooperPickupLengthReference = 0;

unsigned int superlooperPhysicalParam = 0;
unsigned int superlooperPhysicalUnscaledParam = 0;
unsigned int superlooperPhysicalRatchetAmount = 0;
unsigned int superlooperPhysicalLengthReading = 0;
unsigned int superlooperPhysicalLength = 1;

unsigned int superlooperMutationParam = 0;
unsigned int superlooperMutationUnscaledParam = 0;
unsigned int superlooperMutationRatchetAmount = 0;
unsigned int superlooperMutationLength = 1;

unsigned int superlooperLoopParam = 512;
unsigned int superlooperLoopUnscaledParam = 8;
unsigned int superlooperLoopRatchetAmount = 0;
unsigned int superlooperLoopLength = 8;

unsigned int superlooperAnchorHistoryIndex = 0;
unsigned int superlooperLoopLengthOld = 0;
bool superlooperFirstPlaybackPass = 0;

bool superlooperControlMoved(unsigned int reading, unsigned int reference) {
  if (reading > reference) return (reading - reference) > superlooperPickupThreshold;
  return (reference - reading) > superlooperPickupThreshold;
}

void superlooperApplyActiveControls() {
  if (loopEnable) {
    paramRead = modifyParam = superlooperLoopParam;
    unscaledParam = superlooperLoopUnscaledParam;
    ratchetAmount = superlooperLoopRatchetAmount;
    lengthScaled = superlooperLoopLength;
  }

  else {
    paramRead = modifyParam = superlooperMutationParam;
    unscaledParam = superlooperMutationUnscaledParam;
    ratchetAmount = superlooperMutationRatchetAmount;
    lengthScaled = superlooperMutationLength;
  }
}

void superlooperInitializeControls() {
  superlooperMutationParam = superlooperPhysicalParam;
  superlooperMutationUnscaledParam = superlooperPhysicalUnscaledParam;
  superlooperMutationRatchetAmount = superlooperPhysicalRatchetAmount;
  superlooperMutationLength = superlooperPhysicalLength;

  // A fresh alternate engine begins with a full eight-beat loop and exact
  // original-speed playback.
  superlooperLoopParam = 512;
  superlooperLoopUnscaledParam = 8;
  superlooperLoopRatchetAmount = ratchetAmountFromParam(8);
  superlooperLoopLength = 8;

  superlooperControlsInitialized = 1;
}

void superlooperUpdateControlBank() {
  if (!superlooperControlsInitialized) superlooperInitializeControls();

  if (superlooperParamPickup
      && superlooperControlMoved(superlooperPhysicalParam, superlooperPickupParamReference)) {
    superlooperParamPickup = 0;
  }
  if (superlooperLengthPickup
      && superlooperControlMoved(superlooperPhysicalLengthReading, superlooperPickupLengthReference)) {
    superlooperLengthPickup = 0;
  }

  if (loopEnable) {
    if (!superlooperParamPickup) {
      superlooperLoopParam = superlooperPhysicalParam;
      superlooperLoopUnscaledParam = superlooperPhysicalUnscaledParam;
      superlooperLoopRatchetAmount = superlooperPhysicalRatchetAmount;
    }
    if (!superlooperLengthPickup) superlooperLoopLength = superlooperPhysicalLength;
  }

  else {
    if (!superlooperParamPickup) {
      superlooperMutationParam = superlooperPhysicalParam;
      superlooperMutationUnscaledParam = superlooperPhysicalUnscaledParam;
      superlooperMutationRatchetAmount = superlooperPhysicalRatchetAmount;
    }
    if (!superlooperLengthPickup) superlooperMutationLength = superlooperPhysicalLength;
  }

  superlooperApplyActiveControls();
}

void superlooperArmControlPickup(bool enteringLoop) {
  superlooperPickupParamReference = superlooperPhysicalParam;
  superlooperPickupLengthReference = superlooperPhysicalLengthReading;
  superlooperParamPickup = superlooperLengthPickup = 1;

  if (enteringLoop) {
    // Loop speed begins at 1:1. Length recalls the last loop span until the
    // Length control moves.
    superlooperLoopParam = 512;
    superlooperLoopUnscaledParam = 8;
    superlooperLoopRatchetAmount = ratchetAmountFromParam(8);
  }

  superlooperApplyActiveControls();
  handleFreezeParam();
}

void superlooperReadInputs() {
  // Use the stock calibrated reads and scaling, then route the resulting
  // physical values through Superlooper's independent control banks.
  readGateInputs();
  readAnalogInputs();
  scaleCVInputs();

  superlooperPhysicalParam = paramRead;
  superlooperPhysicalUnscaledParam = unscaledParam;
  superlooperPhysicalRatchetAmount = ratchetAmount;
  superlooperPhysicalLengthReading = lengRead;
  superlooperPhysicalLength = lengthScaled;

  superlooperUpdateControlBank();
  handleFreezeParam();

  cycleSwitchIn = cycleIn;

  // Every engine shares the same control contract: menu setting 0 enables
  // external-sequencer tracking; CYCLE itself only selects the engine.
  cycleIn = externalSequencerTracking;
}

void superlooperDetermineLooperStatus() {
  bool loopEnableAtStart = loopEnable;

  if ((loopButtonActiveEdge == 1) && !menuMode) loopEnable = !loopEnable;

  if (loopInputBehavior) {
    if (loopGateActiveEdge == 1) loopEnable = !loopEnable;
    if (loopGateFallingEdge) loopEnable = !loopEnable;
  }
  else if (loopGateActiveEdge == 1) loopEnable = !loopEnable;

  if (!loopEnableAtStart && loopEnable) {
    if (cycleIn && (modifyLength != 0)) maintainCycle();

    superlooperArmControlPickup(1);

    // Offset zero is always the newest captured beat. A loop of length L
    // therefore begins at 1-L and advances through zero before wrapping.
    superlooperAnchorHistoryIndex = historyIndex;
    originalHistoryIndex = superlooperAnchorHistoryIndex;

    loopDivide = 0;
    loopCycleCount = 0;
    loopCycleIndex = 0;
    loopOffsetOld = 0;
    loopClockMult = clockIn;
    loopClockMultOld = clockIn;
    loopIndex = wrapAround(1 - (int)superlooperLoopLength, 8);
    loopIndexOld = 0;
    superlooperLoopLengthOld = 0;
    superlooperFirstPlaybackPass = 1;

    saveLoopData();
  }

  if (loopEnableAtStart && !loopEnable) {
    if (cycleIn) loopCycle();

    modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
    superlooperArmControlPickup(0);
    saveLoopData();
  }
}

void superlooperCalculateLooperVariables() {
  unsigned long currentMicros = micros();

  if (!menuMode) {
    loopLength = lengthScaled;
    loopRatchetAmount = ratchetAmount;
    loopClockSpeed = unscaledParam;
  }

  if (loopLength < 1) loopLength = 1;
  if (loopLength > 8) loopLength = 8;

  if (loopLength != superlooperLoopLengthOld) {
    loopCycleIndex %= loopLength;
  }

  if (clockEdge) {
    if (loopRatchetAmount > 0) loopDivide = (loopDivide + 1) % loopRatchetAmount;
    loopCycleCount += 1;
  }

  if (loopClockSpeed > 9) {
    unsigned long loopMultInterval = (loopRatchetAmount > 0) ? lastClockInterval / loopRatchetAmount : 0;
    loopClockMult = pulseWithinInterval(currentMicros - lastClockTime, loopMultInterval, loopClockPulseWidth);
  }
  else if ((loopClockSpeed == 9) || (loopClockSpeed == 8) || (loopClockSpeed == 7)) {
    loopClockMult = clockIn;
    tickIndex = clockTick;
  }
  else {
    loopClockMult = (loopDivide == 0) && ((currentMicros - lastClockTime) < loopClockPulseWidth);
  }

  if (loopClockMult && !loopClockMultOld) {
    loopCycleIndex = (loopCycleIndex + 1) % loopLength;
    loopClockMultInterval = currentMicros - loopClockMultTime;
    loopClockMultTime = currentMicros;
  }

  if ((loopClockSpeed != 7) && (loopClockSpeed != 8) && (loopClockSpeed != 9)) {
    if (loopClockMultInterval > 0) {
      tickIndex = (4 * (currentMicros - loopClockMultTime)) / loopClockMultInterval;
    }
  }

  unsigned int loopStartOffset = wrapAround(1 - (int)loopLength, 8);
  loopOffset = loopStartOffset;
  loopIndex = (loopStartOffset + loopCycleIndex) % 8;

  bool loopStageChanged = superlooperFirstPlaybackPass
                       || (loopIndex != loopIndexOld)
                       || (loopLength != superlooperLoopLengthOld);

  if (loopStageChanged) {
    loopIndexChanged = 1;
    loopRandom = random(950);

    if (splitMode) {
      loopRandom1 = random(950);
      loopRandom2 = random(950);
      loopRandom3 = random(950);
      loopRandom4 = random(950);
    }
    else loopRandom1 = loopRandom2 = loopRandom3 = loopRandom4 = loopRandom;
  }
  else loopIndexChanged = 0;

  superlooperLoopLengthOld = loopLength;
  superlooperFirstPlaybackPass = 0;
}

void superlooperLoopTriggers() {
  superlooperCalculateLooperVariables();
  sendIndexClocks();
  readLoopMemory();
  saveLoopVariables();
}

void superlooperHandleLooper() {
  superlooperDetermineLooperStatus();
  handleHistoryIndex();

  if (loopEnable) {
    superlooperLoopTriggers();
    edgeDetectLoop();
  }
  else recordTriggers();
}

void runSuperlooperEngine() {

#if IDUM_ADC_DIAGNOSTIC
  adcDiagnosticLoop();
  return;
#endif

  cTimer.tick();
  dTimer.tick();

  superlooperReadInputs();

  edgeDetect();
  seedRNGOnFirstClock();
  chooseMode();
  superlooperHandleLooper();
  modifyTest();

  if (!loopEnable) storeCurrentModificationVariables();

  handleMenu();
  handleOutputs();

  if (printEnable && clockEdge) {
  }
}

void resetSuperlooperEngine() {
  // Treat CYCLE as a conceptual reboot. Neither direction promises musical
  // continuity, and no loop or modification state survives the boundary.
  loopEnable = 0;
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  carryOverActive = 0;

  loopCycleIndex = 0;
  loopCycleCount = 0;
  loopDivide = 0;
  loopClockMult = 0;
  loopClockMultOld = 0;
  loopIndexChanged = 0;

  superlooperControlsInitialized = 0;
  superlooperParamPickup = 0;
  superlooperLengthPickup = 0;
  superlooperAnchorHistoryIndex = 0;
  superlooperLoopLengthOld = 0;
  superlooperFirstPlaybackPass = 0;
}

#endif
