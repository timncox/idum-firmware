//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Superlooper: an independently selectable rendered-output tape engine.
//
// Raw CLOCK IN continues to drive the stock mutation state machine. N defines
// external-sequencer clock division and the number of raw clocks in a loop
// stage. With external tracking enabled, a separate event tape records the
// N-divided sequencer advances produced by mutations. While LOOP is active the
// four trigger outputs and the transport events are read from their tapes.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\

#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER

const uint8_t SUPERLOOPER_STAGE_COUNT = 8;
const uint8_t SUPERLOOPER_MAX_CLOCKS_PER_STEP = 8;
const uint8_t SUPERLOOPER_SUBDIVISIONS_PER_CLOCK = 32;
const uint16_t SUPERLOOPER_MAX_RECORD_CLOCKS =
  (SUPERLOOPER_STAGE_COUNT + 1) * SUPERLOOPER_MAX_CLOCKS_PER_STEP;
const uint8_t SUPERLOOPER_N_OPTION_COUNT = 6;
const uint8_t superlooperClocksPerStepOptions[SUPERLOOPER_N_OPTION_COUNT] =
  { 1, 2, 3, 4, 6, 8 };
const uint8_t superlooperLengthPositionToNOption[8] = { 0, 1, 2, 3, 4, 4, 5, 5 };
const uint8_t SUPERLOOPER_N_EEPROM_MARKER = 0x4e;
const unsigned int superlooperPickupThreshold = 12;

// Four 32-bit channel words per raw clock: one bit per sub-clock tape cell.
// Nine stages keep eight complete stages available while a ninth is recording.
uint32_t superlooperRenderedTape[SUPERLOOPER_MAX_RECORD_CLOCKS][4] = {};

//Transport is event data rather than a continuous gate. Two four-bit advance
//counts are packed into each byte, preserving the same 32-cell timing grid as
//the rendered trigger tape. A raw-clock checkpoint supports loop entry/wrap.
uint8_t superlooperTransportTape[SUPERLOOPER_MAX_RECORD_CLOCKS]
                                [SUPERLOOPER_SUBDIVISIONS_PER_CLOCK / 2] = {};
uint8_t superlooperTransportStageAtStart[SUPERLOOPER_MAX_RECORD_CLOCKS] = {};

uint8_t superlooperNOption = 0;
uint8_t superlooperClocksPerStep = 1;
uint8_t superlooperSequencerClockPhase = 0;
bool superlooperSequencerTimingStarted = 0;
bool superlooperSequencerStepBoundary = 0;
bool superlooperMutationTransportStarted = 0;
uint8_t superlooperMutationTransportPhase = 0;
bool superlooperMutationClockStateOld = 0;
bool superlooperMutationClockGate = 0;
uint8_t superlooperTrackedSequencerStage = SUPERLOOPER_STAGE_COUNT - 1;
bool superlooperExternalTrackingOld = 0;
uint16_t superlooperQueuedSequencerClocks = 0;
bool superlooperQueuedClockPulseHigh = 0;
bool superlooperQueuedClockGapStarted = 0;
unsigned long superlooperQueuedClockTransitionTime = 0;

uint16_t superlooperTapeWriteIndex = 0;
uint8_t superlooperCaptureTick = 0;
uint8_t superlooperCaptureStateMask = 0;
uint16_t superlooperCaptureStartIndex = 0;

uint16_t superlooperLoopBaseCell = 0;
uint16_t superlooperLoopBaseCellCount = 0;
uint8_t superlooperLoopBaseStartStage = 0;
uint8_t superlooperLoopPendingLength = 8;
bool superlooperLoopLengthChangePending = 0;
bool superlooperLoopStageBoundary = 0;
uint8_t superlooperLoopCrossedTriggerMask = 0;

unsigned long superlooperLoopTransportLastTime = 0;
uint32_t superlooperLoopTransportAccumulator = 0;
unsigned int superlooperLoopSpeedControl = 512;
uint16_t superlooperLoopSpeedQ8 = 256;

uint8_t superlooperLoopTransportStage = 0;
uint8_t superlooperLoopNaturalStage = 0;
uint8_t superlooperLoopCaptureNaturalStage = 0;
uint32_t superlooperLoopRawClocksElapsed = 0;
bool superlooperLoopTransportInitialized = 0;

bool superlooperMenuModeOld = 0;
bool superlooperNEditArmed = 0;
uint8_t superlooperNEditPosition = 0;
bool superlooperNDisplayActive = 0;
unsigned long superlooperNDisplayTime = 0;

// Param and Length are virtualized across the LOOP boundary. Chance remains
// the live stock control until a separate density design is introduced.
bool superlooperControlsInitialized = 0;
bool superlooperLoopLengthInitialized = 0;
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

uint16_t superlooperActiveRecordClockCount() {
  return (SUPERLOOPER_STAGE_COUNT + 1) * superlooperClocksPerStep;
}

void superlooperExitLoop();

uint16_t superlooperWrapRecordIndex(long index) {
  long count = superlooperActiveRecordClockCount();
  while (index < 0) index += count;
  return index % count;
}

uint16_t superlooperCellsPerStage() {
  return (uint16_t)superlooperClocksPerStep * SUPERLOOPER_SUBDIVISIONS_PER_CLOCK;
}

void superlooperClearTransportTape() {
  for (uint16_t rawClock = 0; rawClock < SUPERLOOPER_MAX_RECORD_CLOCKS; ++rawClock) {
    for (uint8_t packedCell = 0;
         packedCell < SUPERLOOPER_SUBDIVISIONS_PER_CLOCK / 2;
         ++packedCell) {
      superlooperTransportTape[rawClock][packedCell] = 0;
    }
    superlooperTransportStageAtStart[rawClock] = superlooperTrackedSequencerStage;
  }
}

void superlooperResetMutationTransport() {
  superlooperMutationTransportStarted = superlooperSequencerTimingStarted;
  superlooperMutationTransportPhase = superlooperSequencerClockPhase;
  superlooperMutationClockStateOld = clockIn;
  superlooperMutationClockGate = 0;
  superlooperQueuedSequencerClocks = 0;
  superlooperQueuedClockPulseHigh = 0;
  superlooperQueuedClockGapStarted = 0;
  superlooperTrackedSequencerStage = superlooperSequencerTimingStarted
    ? superlooperLoopNaturalStage
    : SUPERLOOPER_STAGE_COUNT - 1;
  superlooperClearTransportTape();
}

bool superlooperControlMoved(unsigned int reading, unsigned int reference) {
  if (reading > reference) return (reading - reference) > superlooperPickupThreshold;
  return (reference - reading) > superlooperPickupThreshold;
}

uint16_t superlooperLoopSpeedQ8ForParam(unsigned int control) {
  control = min((unsigned int)1024, control);

  //A broad exact-speed center with a continuous quadratic departure. The
  //extremes reach 1/8x and 8x without the stock integer-speed jumps.
  if (control > 640) {
    unsigned long distance = control - 640;
    return 256 + ((distance * distance * (2048UL - 256)) / (384UL * 384UL));
  }

  if (control < 448) {
    unsigned long distance = 448 - control;
    unsigned long divisorQ8 = 256
      + ((distance * distance * (2048UL - 256)) / (448UL * 448UL));
    return (256UL * 256UL) / divisorQ8;
  }

  return 256;
}

void superlooperClearTape() {
  for (uint16_t rawClock = 0; rawClock < SUPERLOOPER_MAX_RECORD_CLOCKS; ++rawClock) {
    for (uint8_t channel = 0; channel < 4; ++channel) {
      superlooperRenderedTape[rawClock][channel] = 0;
    }
    for (uint8_t packedCell = 0;
         packedCell < SUPERLOOPER_SUBDIVISIONS_PER_CLOCK / 2;
         ++packedCell) {
      superlooperTransportTape[rawClock][packedCell] = 0;
    }
    superlooperTransportStageAtStart[rawClock] = superlooperTrackedSequencerStage;
  }

  superlooperTapeWriteIndex = 0;
  superlooperCaptureTick = 0;
  superlooperCaptureStateMask = 0;
  superlooperCaptureStartIndex = 0;
  superlooperLoopBaseCell = 0;
  superlooperLoopBaseCellCount = 0;
  superlooperLoopTransportAccumulator = 0;
}

void superlooperClearTapeClock(uint16_t rawClockIndex) {
  for (uint8_t channel = 0; channel < 4; ++channel) {
    superlooperRenderedTape[rawClockIndex][channel] = 0;
  }
  for (uint8_t packedCell = 0;
       packedCell < SUPERLOOPER_SUBDIVISIONS_PER_CLOCK / 2;
       ++packedCell) {
    superlooperTransportTape[rawClockIndex][packedCell] = 0;
  }
  superlooperTransportStageAtStart[rawClockIndex] = superlooperTrackedSequencerStage;
}

void superlooperResetSequencerTiming() {
  superlooperSequencerClockPhase = 0;
  superlooperSequencerTimingStarted = 0;
  superlooperSequencerStepBoundary = 0;
  superlooperLoopNaturalStage = 0;
  superlooperMutationTransportStarted = 0;
  superlooperMutationTransportPhase = 0;
  superlooperMutationClockStateOld = 0;
  superlooperMutationClockGate = 0;
  superlooperQueuedSequencerClocks = 0;
  superlooperQueuedClockPulseHigh = 0;
  superlooperQueuedClockGapStarted = 0;
  superlooperQueuedClockTransitionTime = 0;
  superlooperTrackedSequencerStage = SUPERLOOPER_STAGE_COUNT - 1;
  superlooperClearTape();
}

void superlooperLoadNSetting() {
  uint8_t option = EEPROM.read(6);
  if ((EEPROM.read(7) != SUPERLOOPER_N_EEPROM_MARKER)
      || (option >= SUPERLOOPER_N_OPTION_COUNT)) {
    option = 0;
    EEPROM.write(6, option);
    EEPROM.write(7, SUPERLOOPER_N_EEPROM_MARKER);
  }

  superlooperNOption = option;
  superlooperClocksPerStep = superlooperClocksPerStepOptions[option];
}

void superlooperSetNOption(uint8_t option) {
  if (option >= SUPERLOOPER_N_OPTION_COUNT) option = 0;
  if ((option == superlooperNOption)
      && (superlooperClocksPerStep == superlooperClocksPerStepOptions[option])) return;

  if (loopEnable) superlooperExitLoop();
  superlooperNOption = option;
  superlooperClocksPerStep = superlooperClocksPerStepOptions[option];
  superlooperResetSequencerTiming();
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

  superlooperLoopParam = 512;
  superlooperLoopUnscaledParam = 8;
  superlooperLoopRatchetAmount = ratchetAmountFromParam(8);
  superlooperLoopLength = 8;
  superlooperLoopPendingLength = 8;
  superlooperControlsInitialized = 1;
}

void superlooperUpdateControlBank() {
  if (!superlooperControlsInitialized) superlooperInitializeControls();

  //Length is an N editor while the menu is open, not a performance control.
  if (menuMode) {
    superlooperApplyActiveControls();
    return;
  }

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
    if (!superlooperLengthPickup) superlooperLoopLength = lengthPosition + 1;
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
  (void)enteringLoop;
  superlooperPickupParamReference = superlooperPhysicalParam;
  superlooperPickupLengthReference = superlooperPhysicalLengthReading;
  superlooperParamPickup = superlooperLengthPickup = 1;

  superlooperApplyActiveControls();
  handleFreezeParam();
}

void superlooperReadInputs() {
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

  //The saved tracking option enables stock cycle maintenance. Bulk advances
  //from that path are intercepted and translated into the N-divided transport.
  cycleIn = externalSequencerTracking;
}

uint8_t superlooperSubClockPhase(unsigned long currentMicros) {
  if (lastClockInterval == 0) return 0;
  unsigned long elapsed = currentMicros - lastClockTime;
  if (elapsed >= lastClockInterval) return SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - 1;

  uint8_t phase = (elapsed * (unsigned long)SUPERLOOPER_SUBDIVISIONS_PER_CLOCK)
                / lastClockInterval;
  if (phase >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK) {
    phase = SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - 1;
  }
  return phase;
}

void superlooperWriteCaptureCell(uint16_t rawClockIndex, uint8_t tick, uint8_t stateMask) {
  if (tick >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK) return;
  uint32_t tickBit = (uint32_t)1 << tick;
  for (uint8_t channel = 0; channel < 4; ++channel) {
    if (stateMask & (1 << channel)) {
      superlooperRenderedTape[rawClockIndex][channel] |= tickBit;
    }
  }
}

void superlooperFillCaptureCells(
  uint16_t rawClockIndex,
  uint8_t firstTick,
  uint8_t lastTick,
  uint8_t stateMask
) {
  if (firstTick > lastTick) return;
  if (lastTick >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK) {
    lastTick = SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - 1;
  }
  for (uint8_t tick = firstTick; tick <= lastTick; ++tick) {
    superlooperWriteCaptureCell(rawClockIndex, tick, stateMask);
  }
}

void superlooperFinalizeCaptureClock() {
  if (!superlooperSequencerTimingStarted
      || (superlooperCaptureTick >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - 1)) return;
  superlooperFillCaptureCells(
    superlooperTapeWriteIndex,
    superlooperCaptureTick + 1,
    SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - 1,
    superlooperCaptureStateMask
  );
}

void superlooperAdvanceSequencerTiming() {
  superlooperSequencerStepBoundary = 0;
  if (!clockEdge) return;

  bool timingWasStarted = superlooperSequencerTimingStarted;
  if (!superlooperSequencerTimingStarted) {
    superlooperSequencerTimingStarted = 1;
    superlooperSequencerClockPhase = 0;
  }
  else {
    superlooperSequencerClockPhase =
      (superlooperSequencerClockPhase + 1) % superlooperClocksPerStep;
  }
  superlooperSequencerStepBoundary = (superlooperSequencerClockPhase == 0);

  if (timingWasStarted && superlooperSequencerStepBoundary) {
    superlooperLoopNaturalStage =
      (superlooperLoopNaturalStage + 1) % SUPERLOOPER_STAGE_COUNT;
  }

  if (!loopEnable) {
    if (timingWasStarted) {
      superlooperFinalizeCaptureClock();
      superlooperTapeWriteIndex =
        (superlooperTapeWriteIndex + 1) % superlooperActiveRecordClockCount();
    }
    superlooperClearTapeClock(superlooperTapeWriteIndex);
    superlooperCaptureTick = 0;
    superlooperCaptureStateMask = 0;
  }
  else {
    superlooperLoopRawClocksElapsed += 1;
  }
}

void superlooperRecordRenderedOutputs(uint8_t stateMask) {
  if (loopEnable || !superlooperSequencerTimingStarted) return;

  //Most passes through the stock renderer do not change an output. Avoid a
  //32-bit phase division on those passes: ball timing intentionally depends
  //on loop cadence, so the tape recorder must make the common path tiny.
  if (stateMask == superlooperCaptureStateMask) return;

  uint8_t currentTick = superlooperSubClockPhase(micros());
  if (currentTick > superlooperCaptureTick + 1) {
    superlooperFillCaptureCells(
      superlooperTapeWriteIndex,
      superlooperCaptureTick + 1,
      currentTick - 1,
      superlooperCaptureStateMask
    );
  }

  superlooperWriteCaptureCell(superlooperTapeWriteIndex, currentTick, stateMask);
  superlooperCaptureTick = currentTick;
  superlooperCaptureStateMask = stateMask;
}

bool superlooperReadRenderedTrigger(uint16_t rawClockIndex, uint8_t tick, uint8_t channel) {
  if ((tick >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK) || (channel >= 4)) return 0;
  return superlooperRenderedTape[rawClockIndex][channel] & ((uint32_t)1 << tick);
}

void superlooperCaptureCompleteWindow() {
  //Skip the current partial sequencer stage and retain the eight preceding
  //complete N-clock stages.
  superlooperCaptureStartIndex = superlooperWrapRecordIndex(
    (long)superlooperTapeWriteIndex
    - superlooperSequencerClockPhase
    - ((long)SUPERLOOPER_STAGE_COUNT * superlooperClocksPerStep)
  );
}

uint8_t superlooperRenderedLoopCellState(uint16_t phraseCell) {
  if (superlooperLoopBaseCellCount == 0) return 0;

  uint16_t stageCells = superlooperCellsPerStage();
  uint16_t capturedCell =
    ((uint32_t)superlooperLoopBaseStartStage * stageCells + phraseCell)
    % ((uint32_t)SUPERLOOPER_STAGE_COUNT * stageCells);
  uint16_t historyClock = superlooperWrapRecordIndex(
    (long)superlooperCaptureStartIndex
    + (capturedCell / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK)
  );
  uint8_t historyTick = capturedCell % SUPERLOOPER_SUBDIVISIONS_PER_CLOCK;

  uint8_t state = 0;
  for (uint8_t channel = 0; channel < 4; ++channel) {
    if (superlooperReadRenderedTrigger(historyClock, historyTick, channel)) state |= (1 << channel);
  }
  return state;
}

void superlooperSendSequencerClocks(unsigned int clockCount) {
  if (clockCount > 0) sendClockStepsDirect(clockCount);
}

uint8_t superlooperTransportAdvanceAt(uint16_t rawClockIndex, uint8_t tick) {
  if (tick >= SUPERLOOPER_SUBDIVISIONS_PER_CLOCK) return 0;
  uint8_t packed = superlooperTransportTape[rawClockIndex][tick >> 1];
  return (tick & 1) ? (packed >> 4) : (packed & 0x0f);
}

void superlooperRecordTransportAdvance(unsigned int clockCount) {
  uint8_t tick = superlooperSubClockPhase(micros());
  uint8_t packedIndex = tick >> 1;
  uint8_t packed = superlooperTransportTape[superlooperTapeWriteIndex][packedIndex];
  uint8_t previous = (tick & 1) ? (packed >> 4) : (packed & 0x0f);
  uint8_t accumulated = min((unsigned int)15, previous + clockCount);
  if (tick & 1) packed = (packed & 0x0f) | (accumulated << 4);
  else packed = (packed & 0xf0) | accumulated;
  superlooperTransportTape[superlooperTapeWriteIndex][packedIndex] = packed;
}

void superlooperTrackSequencerAdvance(unsigned int clockCount, bool recordEvent) {
  if (clockCount == 0) return;

  superlooperTrackedSequencerStage =
    (superlooperTrackedSequencerStage + (clockCount % SUPERLOOPER_STAGE_COUNT))
    % SUPERLOOPER_STAGE_COUNT;

  if (!recordEvent || loopEnable || !externalSequencerTracking) return;
  superlooperRecordTransportAdvance(clockCount);
}

void superlooperQueueTrackedSequencerClocks(unsigned int clockCount) {
  if (clockCount == 0) return;
  uint32_t queued = (uint32_t)superlooperQueuedSequencerClocks + clockCount;
  superlooperQueuedSequencerClocks = min(queued, (uint32_t)UINT16_MAX);
  superlooperTrackSequencerAdvance(clockCount, 0);
}

void superlooperServiceQueuedClock() {
  //Ball timing intentionally follows main-loop cadence. Keep the common idle
  //path free of micros() and transport work so enabling external tracking does
  //not change the established Ball feel when no burst is being emitted.
  if ((superlooperQueuedSequencerClocks == 0)
      && !superlooperQueuedClockPulseHigh
      && !superlooperQueuedClockGapStarted) return;

  unsigned long currentMicros = micros();
  unsigned long elapsed = currentMicros - superlooperQueuedClockTransitionTime;

  if (superlooperQueuedClockPulseHigh) {
    if (elapsed >= externalSequencerBurstHalfPeriod) {
      superlooperQueuedClockPulseHigh = 0;
      superlooperQueuedClockGapStarted = 1;
      superlooperQueuedClockTransitionTime = currentMicros;
    }
    return;
  }

  if (superlooperQueuedClockGapStarted) {
    if (elapsed < externalSequencerBurstHalfPeriod) return;
    superlooperQueuedClockGapStarted = 0;
    if (superlooperQueuedSequencerClocks == 0) return;
  }

  if (superlooperQueuedSequencerClocks > 0) {
    superlooperQueuedSequencerClocks -= 1;
    superlooperQueuedClockPulseHigh = 1;
    superlooperQueuedClockGapStarted = 0;
    superlooperQueuedClockTransitionTime = currentMicros;
  }
}

unsigned int superlooperTranslateLogicalClockAdvances(unsigned int logicalClockCount) {
  if (logicalClockCount == 0) return 0;

  unsigned int sequencerAdvances = 0;
  if (!superlooperMutationTransportStarted) {
    superlooperMutationTransportStarted = 1;
    superlooperMutationTransportPhase = 0;
    sequencerAdvances = 1;
    logicalClockCount -= 1;
  }

  unsigned int totalPhase = superlooperMutationTransportPhase + logicalClockCount;
  sequencerAdvances += totalPhase / superlooperClocksPerStep;
  superlooperMutationTransportPhase = totalPhase % superlooperClocksPerStep;
  return sequencerAdvances;
}

void superlooperAdvanceLogicalTransport(unsigned int logicalClockCount) {
  unsigned int sequencerAdvances =
    superlooperTranslateLogicalClockAdvances(logicalClockCount);
  if (sequencerAdvances == 0) return;

  superlooperSendSequencerClocks(sequencerAdvances);
  superlooperTrackSequencerAdvance(sequencerAdvances, 1);
}

void superlooperUpdateMutationClockGate() {
  if (clockState && !superlooperMutationClockStateOld) {
    unsigned int sequencerAdvances = superlooperTranslateLogicalClockAdvances(1);
    superlooperMutationClockGate = sequencerAdvances > 0;
    superlooperTrackSequencerAdvance(sequencerAdvances, 1);
  }
  else if (!clockState) {
    superlooperMutationClockGate = 0;
  }
  superlooperMutationClockStateOld = clockState;
}

uint16_t superlooperCapturedCellForPhraseCell(uint16_t phraseCell) {
  uint16_t stageCells = superlooperCellsPerStage();
  return ((uint32_t)superlooperLoopBaseStartStage * stageCells + phraseCell)
    % ((uint32_t)SUPERLOOPER_STAGE_COUNT * stageCells);
}

void superlooperPlayTransportCell(uint16_t phraseCell) {
  uint16_t capturedCell = superlooperCapturedCellForPhraseCell(phraseCell);
  uint16_t rawClockIndex = superlooperWrapRecordIndex(
    (long)superlooperCaptureStartIndex
    + (capturedCell / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK)
  );
  uint8_t tick = capturedCell % SUPERLOOPER_SUBDIVISIONS_PER_CLOCK;

  if (tick == 0) {
    uint8_t targetStage = superlooperTransportStageAtStart[rawClockIndex];
    unsigned int alignmentClocks =
      (targetStage + SUPERLOOPER_STAGE_COUNT - superlooperTrackedSequencerStage)
      % SUPERLOOPER_STAGE_COUNT;
    superlooperQueueTrackedSequencerClocks(alignmentClocks);
  }

  superlooperQueueTrackedSequencerClocks(
    superlooperTransportAdvanceAt(rawClockIndex, tick)
  );
  superlooperLoopTransportStage = superlooperTrackedSequencerStage;
}

void superlooperSendLoopStage(uint8_t capturedStage, bool repeatedStage) {
  if (!superlooperLoopTransportInitialized) {
    superlooperLoopTransportStage = capturedStage;
    superlooperLoopTransportInitialized = 1;
    return;
  }

  unsigned int clocksToSend = 1;
  if (externalSequencerTracking) {
    clocksToSend = (capturedStage + SUPERLOOPER_STAGE_COUNT
                    - superlooperLoopTransportStage) % SUPERLOOPER_STAGE_COUNT;
    if ((clocksToSend == 0) && repeatedStage) clocksToSend = SUPERLOOPER_STAGE_COUNT;
  }
  superlooperSendSequencerClocks(clocksToSend);
  superlooperLoopTransportStage = capturedStage;
}

void superlooperSetLoopPositionVariables() {
  uint16_t stageCells = superlooperCellsPerStage();
  uint8_t relativeStage = (stageCells > 0) ? superlooperLoopBaseCell / stageCells : 0;
  loopIndex = (superlooperLoopBaseStartStage + relativeStage) % SUPERLOOPER_STAGE_COUNT;
}

void superlooperConfigureLoopLength(unsigned int requestedLength) {
  loopLength = max((unsigned int)1,
    min((unsigned int)SUPERLOOPER_STAGE_COUNT, requestedLength));
  superlooperLoopBaseStartStage = SUPERLOOPER_STAGE_COUNT - loopLength;
  superlooperLoopBaseCellCount = (uint16_t)loopLength * superlooperCellsPerStage();
  superlooperLoopBaseCell = 0;
  superlooperSetLoopPositionVariables();
}

void superlooperAdvanceLoopCell() {
  uint8_t previousState = superlooperRenderedLoopCellState(superlooperLoopBaseCell);
  superlooperLoopBaseCell =
    (superlooperLoopBaseCell + 1) % superlooperLoopBaseCellCount;

  uint8_t currentState = superlooperRenderedLoopCellState(superlooperLoopBaseCell);
  superlooperLoopCrossedTriggerMask |= currentState & ~previousState;

  if (externalSequencerTracking) {
    superlooperPlayTransportCell(superlooperLoopBaseCell);
  }

  uint16_t stageCells = superlooperCellsPerStage();
  if ((superlooperLoopBaseCell % stageCells) != 0) return;

  superlooperLoopStageBoundary = 1;
  uint8_t relativeStage = superlooperLoopBaseCell / stageCells;
  uint8_t capturedStage =
    (superlooperLoopCaptureNaturalStage + superlooperLoopBaseStartStage + relativeStage)
    % SUPERLOOPER_STAGE_COUNT;
  if (!externalSequencerTracking) superlooperSendLoopStage(capturedStage, 1);
}

void superlooperAdvanceLoopTransport(unsigned long currentMicros) {
  unsigned long elapsed = currentMicros - superlooperLoopTransportLastTime;
  superlooperLoopTransportLastTime = currentMicros;
  if ((elapsed == 0) || (lastClockInterval == 0) || (superlooperLoopBaseCellCount == 0)) return;

  superlooperLoopSpeedQ8 = superlooperLoopSpeedQ8ForParam(superlooperLoopSpeedControl);
  //Accumulator units are microseconds * Q8 speed. Fold the subdivision into
  //the per-cell threshold while keeping this hot path in 32-bit AVR math.
  superlooperLoopTransportAccumulator += (uint32_t)elapsed * superlooperLoopSpeedQ8;
  uint32_t threshold = (uint32_t)lastClockInterval
    * (256 / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK);
  if (superlooperLoopTransportAccumulator < threshold) return;

  uint32_t cellsToAdvance = superlooperLoopTransportAccumulator / threshold;
  superlooperLoopTransportAccumulator %= threshold;
  if (cellsToAdvance >= superlooperLoopBaseCellCount) {
    cellsToAdvance %= superlooperLoopBaseCellCount;
  }
  while (cellsToAdvance-- > 0) superlooperAdvanceLoopCell();
}

void superlooperInitializeLoopTransport() {
  superlooperConfigureLoopLength(superlooperLoopLength);
  superlooperLoopPendingLength = loopLength;
  superlooperLoopLengthChangePending = 0;
  superlooperLoopSpeedControl = superlooperLoopParam;
  superlooperLoopSpeedQ8 = superlooperLoopSpeedQ8ForParam(superlooperLoopSpeedControl);
  superlooperLoopTransportLastTime = micros();
  superlooperLoopTransportAccumulator = 0;
  superlooperLoopCrossedTriggerMask = 0;
  superlooperLoopRawClocksElapsed = 0;
  //The natural stage tracks the N-divided input whether or not LOOP is
  //active. If LOOP is engaged on a stage edge, that edge has not yet reached
  //CLOCK OUT, so the attached sequencer is still on the preceding stage.
  superlooperLoopCaptureNaturalStage = superlooperLoopNaturalStage;
  superlooperLoopTransportStage = superlooperLoopNaturalStage;
  if (superlooperSequencerStepBoundary && clockIn) {
    superlooperLoopTransportStage =
      (superlooperLoopTransportStage + SUPERLOOPER_STAGE_COUNT - 1)
      % SUPERLOOPER_STAGE_COUNT;
  }
  superlooperLoopTransportInitialized = 1;

  uint8_t firstCapturedStage =
    (superlooperLoopCaptureNaturalStage + superlooperLoopBaseStartStage)
    % SUPERLOOPER_STAGE_COUNT;
  if (externalSequencerTracking) {
    superlooperPlayTransportCell(0);
  }
  else superlooperLoopTransportStage = firstCapturedStage;
}

void superlooperFinishLoopTransport() {
  if (!superlooperLoopTransportInitialized || !externalSequencerTracking) return;

  uint8_t stageBeforeLive = superlooperLoopNaturalStage;
  if ((superlooperSequencerClockPhase == 0) && clockIn) {
    stageBeforeLive =
      (stageBeforeLive + SUPERLOOPER_STAGE_COUNT - 1) % SUPERLOOPER_STAGE_COUNT;
  }
  uint8_t clocksToSend = (stageBeforeLive + SUPERLOOPER_STAGE_COUNT
                          - superlooperTrackedSequencerStage) % SUPERLOOPER_STAGE_COUNT;
  superlooperQueueTrackedSequencerClocks(clocksToSend);
  superlooperLoopTransportStage = stageBeforeLive;
}

void superlooperResumeTapeAfterLoop() {
  //Playback freezes the tape. On exit, move its write head by the number of
  //raw clocks that really elapsed and blank that span before recording again.
  uint16_t recordClockCount = superlooperActiveRecordClockCount();
  uint32_t clocksToClear = superlooperLoopRawClocksElapsed + 1;
  if (clocksToClear > recordClockCount) clocksToClear = recordClockCount;
  for (uint16_t offset = 0; offset < clocksToClear; ++offset) {
    uint16_t rawClockIndex =
      (superlooperTapeWriteIndex + offset) % recordClockCount;
    superlooperClearTapeClock(rawClockIndex);
  }
  superlooperTapeWriteIndex =
    (superlooperTapeWriteIndex + (superlooperLoopRawClocksElapsed % recordClockCount))
    % recordClockCount;
  superlooperCaptureTick = superlooperSubClockPhase(micros());
  superlooperCaptureStateMask = 0;
  superlooperLoopRawClocksElapsed = 0;
}

void superlooperEnterLoop() {
  //The first loop after selecting Superlooper should honor the panel instead
  //of recalling an arbitrary default. Later entries recall the previous loop
  //span until Length moves, preserving the useful lazy-pickup behavior.
  if (!superlooperLoopLengthInitialized) {
    superlooperLoopLength = lengthPosition + 1;
    superlooperLoopPendingLength = superlooperLoopLength;
    superlooperLoopLengthInitialized = 1;
  }
  loopEnable = 1;
  superlooperMutationClockGate = 0;
  superlooperArmControlPickup(1);
  superlooperCaptureCompleteWindow();
  superlooperInitializeLoopTransport();

  //The tape already contains the mutation result. No recorded output is sent
  //through the mutation state machine a second time.
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  carryOverActive = 0;
  saveLoopData();
}

void superlooperExitLoop() {
  superlooperFinishLoopTransport();
  loopEnable = 0;
  superlooperMutationTransportStarted = superlooperSequencerTimingStarted;
  superlooperMutationTransportPhase = superlooperSequencerClockPhase;
  if (clockEdge && superlooperMutationTransportStarted) {
    superlooperMutationTransportPhase =
      (superlooperMutationTransportPhase + superlooperClocksPerStep - 1)
      % superlooperClocksPerStep;
  }
  superlooperMutationClockStateOld = clockEdge ? 0 : clockIn;
  superlooperMutationClockGate = 0;
  superlooperResumeTapeAfterLoop();
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  superlooperArmControlPickup(0);
  saveLoopData();
}

void superlooperDetermineLooperStatus() {
  bool requested = loopEnable;
  if (loopButtonActiveEdge && !menuMode) requested = !requested;

  if (loopInputBehavior) {
    if (loopGateActiveEdge) requested = 1;
    if (loopGateFallingEdge) requested = 0;
  }
  else if (loopGateActiveEdge) requested = !requested;

  if (!loopEnable && requested) superlooperEnterLoop();
  else if (loopEnable && !requested) superlooperExitLoop();
}

uint8_t superlooperUpdateLoopPlayback() {
  superlooperLoopCrossedTriggerMask = 0;
  superlooperLoopStageBoundary = 0;

  uint8_t requestedLength = max((unsigned int)1,
    min((unsigned int)SUPERLOOPER_STAGE_COUNT, lengthScaled));
  if (requestedLength != superlooperLoopPendingLength) {
    superlooperLoopPendingLength = requestedLength;
    superlooperLoopLengthChangePending = (requestedLength != loopLength);
  }

  superlooperLoopSpeedControl = modifyParam;
  superlooperAdvanceLoopTransport(micros());

  if (superlooperLoopLengthChangePending && superlooperLoopStageBoundary) {
    superlooperConfigureLoopLength(superlooperLoopPendingLength);
    superlooperLoopLengthChangePending = 0;
    uint8_t firstCapturedStage =
      (superlooperLoopCaptureNaturalStage + superlooperLoopBaseStartStage)
      % SUPERLOOPER_STAGE_COUNT;
    if (externalSequencerTracking) {
      superlooperPlayTransportCell(0);
    }
    else superlooperSendLoopStage(firstCapturedStage, 0);
  }

  superlooperSetLoopPositionVariables();
  return superlooperRenderedLoopCellState(superlooperLoopBaseCell)
       | superlooperLoopCrossedTriggerMask;
}

uint8_t superlooperResolvedMutationTriggerMask() {
  bool output1Choke = ((modifyLength1 > 0) && (mode1 == 4)) ? rotate1Choke : trig1Choke;
  bool output2Choke = ((modifyLength2 > 0) && (mode2 == 4)) ? rotate2Choke : trig2Choke;
  bool output3Choke = ((modifyLength3 > 0) && (mode3 == 4)) ? rotate3Choke : trig3Choke;
  bool output4Choke = ((modifyLength4 > 0) && (mode4 == 4)) ? rotate4Choke : trig4Choke;

  uint8_t mask = 0;
  if (trig1State && output1Choke) mask |= 1;
  if (trig2State && output2Choke) mask |= 2;
  if (trig3State && output3Choke) mask |= 4;
  if (trig4State && output4Choke) mask |= 8;
  return mask;
}

void superlooperCalculateRenderedMutationOutputs() {
  if (externalSequencerTracking) {
    calculateTriggerStates();
    superlooperUpdateMutationClockGate();
    return;
  }

  //Without external tracking, keep CLOCK OUT on the plain N-divided input
  //while rendering trigger-channel mutations at their raw-clock timing.
  unsigned int savedClockModifyLength = modifyLength;
  modifyLength = 0;
  calculateTriggerStates();
  modifyLength = savedClockModifyLength;
}

void superlooperWriteOutputs(uint8_t triggerMask, bool sequencerClockHigh) {
  digitalWrite(clockOut, sequencerClockHigh ? LOW : HIGH);
  digitalWriteFast(trig1Out, (triggerMask & 1) ? LOW : HIGH);
  digitalWriteFast(trig2Out, (triggerMask & 2) ? LOW : HIGH);
  digitalWriteFast(trig3Out, (triggerMask & 4) ? LOW : HIGH);
  digitalWriteFast(trig4Out, (triggerMask & 8) ? LOW : HIGH);
}

bool superlooperSequencerClockHigh() {
  superlooperServiceQueuedClock();
  if (externalSequencerTracking) {
    return superlooperQueuedClockPulseHigh
        || (!loopEnable && superlooperMutationClockGate);
  }
  return !loopEnable && superlooperSequencerTimingStarted
      && (superlooperSequencerClockPhase == 0) && clockIn;
}

void superlooperHandleNMenu() {
  handleMenu();

  if (externalSequencerTracking != superlooperExternalTrackingOld) {
    superlooperResetMutationTransport();
    superlooperExternalTrackingOld = externalSequencerTracking;
  }

  if (menuMode && !superlooperMenuModeOld) {
    superlooperNEditPosition = lengthPosition;
    superlooperNEditArmed = 1;
    superlooperNDisplayActive = 0;
  }

  if (menuMode && superlooperNEditArmed
      && (lengthPosition != superlooperNEditPosition)) {
    superlooperNEditPosition = lengthPosition;
    uint8_t requestedNOption = superlooperLengthPositionToNOption[lengthPosition];
    if (requestedNOption != superlooperNOption) {
      superlooperSetNOption(requestedNOption);
      superlooperNDisplayTime = micros();
      superlooperNDisplayActive = 1;
    }
  }

  if (!menuMode && superlooperMenuModeOld) {
    EEPROM.write(6, superlooperNOption);
    EEPROM.write(7, SUPERLOOPER_N_EEPROM_MARKER);
    superlooperNEditArmed = 0;
    superlooperNDisplayActive = 0;
    superlooperPickupLengthReference = superlooperPhysicalLengthReading;
    superlooperLengthPickup = 1;
  }

  superlooperMenuModeOld = menuMode;
  if (menuMode && superlooperNDisplayActive) {
    if ((micros() - superlooperNDisplayTime) < 1000000UL) {
      writeModeLed(superlooperNOption);
    }
    else superlooperNDisplayActive = 0;
  }
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
  superlooperAdvanceSequencerTiming();
  seedRNGOnFirstClock();
  chooseMode();
  superlooperDetermineLooperStatus();

  uint8_t renderedMask = 0;
  if (loopEnable) {
    renderedMask = superlooperUpdateLoopPlayback();
  }
  else {
    modifyTest();
    superlooperCalculateRenderedMutationOutputs();
    renderedMask = superlooperResolvedMutationTriggerMask();
    superlooperRecordRenderedOutputs(renderedMask);
  }

  superlooperHandleNMenu();
  superlooperWriteOutputs(renderedMask, superlooperSequencerClockHigh());

  if (printEnable && clockEdge) {
  }
}

void resetSuperlooperEngine() {
  loopEnable = 0;
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  carryOverActive = 0;
  cycleActive = 0;
  cycleCount = 0;

  superlooperLoadNSetting();
  superlooperResetSequencerTiming();
  superlooperLoopBaseCell = 0;
  superlooperLoopBaseCellCount = 0;
  superlooperLoopPendingLength = 8;
  superlooperLoopLengthChangePending = 0;
  superlooperLoopTransportAccumulator = 0;
  superlooperLoopTransportInitialized = 0;
  superlooperLoopCaptureNaturalStage = 0;
  superlooperLoopRawClocksElapsed = 0;

  superlooperControlsInitialized = 0;
  superlooperLoopLengthInitialized = 0;
  superlooperParamPickup = 0;
  superlooperLengthPickup = 0;
  superlooperMenuModeOld = menuMode;
  superlooperNEditArmed = 0;
  superlooperNDisplayActive = 0;
  superlooperExternalTrackingOld = externalSequencerTracking;
}

#endif
