//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Superlooper: an independently selectable rendered-output tape engine.
//
// Raw CLOCK IN opens Chance windows; CLOCK/TRIGGER lane edges launch mutations.
// N defines external-sequencer clock division, raw clocks per loop stage, and
// therefore the L*N raw-clock lifetime of a mutation. With external tracking
// enabled, a separate event tape records sequencer advances produced by those
// mutations. While LOOP is active the four trigger outputs and transport events
// are read from their tapes.
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
//Ignore a little ADC noise at the bottom of Chance. Keep the curve above this
//floor unchanged and honor a larger shared threshold in custom builds.
const unsigned int superlooperChanceDeadZone = 10;
//0: smooth forward, 1: stepped forward, 2: smooth bidirectional,
//3: stepped bidirectional. Separate EEPROM bytes preserve existing settings.
uint8_t superlooperPlaybackOption = 0;
const uint8_t SUPERLOOPER_PLAYBACK_EEPROM_MARKER = 0x53;
int8_t superlooperSpeedBin = -1;
//Use unity until the first loop PARAM gesture; retain that gesture on re-entry.
bool superlooperSpeedPickup = 0;
unsigned int superlooperPlaybackEditReference = 0;
bool superlooperPlaybackEditArmed = 0;
uint8_t superlooperMenuDisplayLed = 0;

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
bool superlooperLoopRequestPending = 0;
bool superlooperLoopRequestedState = 0;

struct SuperlooperMutationLaneState {
  bool active;
  uint8_t activeMode;
  uint8_t sampledLength;
  uint8_t clocksRemaining;
  unsigned long startMicros;
  unsigned long durationMicros;
};

//Lane zero is CLOCK; lanes one through four are TR1-TR4. A clock edge opens
//eligibility windows, while the corresponding lane edge owns the mutation.
SuperlooperMutationLaneState superlooperMutationLanes[5] = {};
bool superlooperMutationWindows[5] = {};

const unsigned long SUPERLOOPER_BALL_FAST_LOW_MICROS = 4000UL;
const uint32_t SUPERLOOPER_BALL_PHASE_ONE = 256000000UL;
const unsigned long SUPERLOOPER_BALL_CONTROL_SLEW_MICROS = 8000UL;

struct SuperlooperBallState {
  bool prepared;
  bool started;
  bool speeding;
  uint8_t sampledLength;
  uint8_t liveLength;
  uint16_t strengthQ8;
  unsigned long mutationStartMicros;
  unsigned long mutationDurationMicros;
  unsigned long lastUpdateMicros;
  unsigned long pulseStartMicros;
  uint32_t phase;
  uint32_t slowRateQ8;
  uint32_t fastRateQ8;
  uint32_t currentRateQ8;
};

SuperlooperBallState superlooperBallStates[4] = {};

#if IDUM_BALL_TIMING_DIAGNOSTIC
const uint8_t SUPERLOOPER_BALL_DIAG_EEPROM_BASE = 32;
const uint8_t SUPERLOOPER_BALL_DIAG_MAGIC = 0xb7;
bool superlooperBallDiagClockSeen = 0;
bool superlooperBallDiagIntervalActive = 0;
bool superlooperBallDiagCaptureWritten = 0;
uint32_t superlooperBallDiagLoopCount = 0;
unsigned int superlooperBallDiagParam = 0;
uint8_t superlooperBallDiagSampledLength = 0;
uint8_t superlooperBallDiagLiveLength = 0;
uint8_t superlooperBallDiagExpanding = 0;
unsigned int superlooperBallDiagStartDivider = 0;
unsigned long superlooperBallDiagLastPrint = 0;
#endif

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
int16_t superlooperLoopSpeedQ8 = 256;
bool superlooperLoopReverse = 0;
uint16_t superlooperReverseGateRemaining[4] = {};
uint8_t superlooperReverseLastOutputMask = 0;
uint8_t superlooperReverseRetriggerMask = 0;

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
void superlooperResetMutationScheduler();

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

int16_t superlooperSelectedLoopSpeed(unsigned int control) {
  if (superlooperSpeedPickup) return 256;
  control = min((unsigned int)1024, control);
  bool bidirectional = superlooperPlaybackOption & 2;
  if (superlooperPlaybackOption & 1) {
    uint8_t count = bidirectional ? 15 : 7;
    int8_t requested = ((uint32_t)control * count) / 1025;
    //Eight ADC counts of hysteresis at each boundary prevents adjacent
    //musical ratios from chattering while the control is stationary.
    int lower = ((int)superlooperSpeedBin * 1025) / count - 8;
    int upper = (((int)superlooperSpeedBin + 1) * 1025) / count + 8;
    if (superlooperSpeedBin < 0 || (int)control < lower || (int)control >= upper) {
      superlooperSpeedBin = requested;
    }
    if (!bidirectional) return (int16_t)32 << superlooperSpeedBin;
    if (superlooperSpeedBin == 7) return 0;
    if (superlooperSpeedBin < 7) return -((int16_t)32 << (6 - superlooperSpeedBin));
    return (int16_t)32 << (superlooperSpeedBin - 8);
  }
  if (!bidirectional) return superlooperLoopSpeedQ8ForParam(control);
  if (control >= 480 && control <= 544) return 0;
  unsigned long distance = control < 480 ? 480 - control : control - 544;
  int16_t speed = (distance * distance * 2048UL) / (480UL * 480UL);
  return control < 480 ? -speed : speed;
}

void superlooperLoadPlaybackSetting() {
  uint8_t option = EEPROM.read(8);
  superlooperPlaybackOption = (EEPROM.read(9) == SUPERLOOPER_PLAYBACK_EEPROM_MARKER
    && option < 4) ? option : 0;
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
      superlooperSpeedPickup = 0;
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

  //Most passes through the renderer do not change an output. Avoid a 32-bit
  //phase division on those passes so time-domain modes retain prompt service.
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

bool superlooperClockQueueBusy() {
  return (superlooperQueuedSequencerClocks > 0)
      || superlooperQueuedClockPulseHigh || superlooperQueuedClockGapStarted;
}

void superlooperQueueSequencerClocks(unsigned int clockCount) {
  if (clockCount == 0) return;
  uint32_t queued = (uint32_t)superlooperQueuedSequencerClocks + clockCount;
  superlooperQueuedSequencerClocks = min(queued, (uint32_t)UINT16_MAX);
}

void superlooperQueueTrackedSequencerClocks(unsigned int clockCount) {
  superlooperQueueSequencerClocks(clockCount);
  superlooperTrackSequencerAdvance(clockCount, 0);
}

void superlooperDiscardQueuedClocks() {
  //The tracked stage includes scheduled advances. Remove only pulses that
  //have not started; preserve an in-flight pulse and its mandatory low gap.
  superlooperTrackedSequencerStage = (superlooperTrackedSequencerStage
    + SUPERLOOPER_STAGE_COUNT - (superlooperQueuedSequencerClocks % SUPERLOOPER_STAGE_COUNT))
    % SUPERLOOPER_STAGE_COUNT;
  superlooperQueuedSequencerClocks = 0;
}

void superlooperServiceQueuedClock() {
  //Keep the common idle path free of micros() and transport work so generated
  //trigger timing retains prompt service when no burst is being emitted.
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

  //A mutation can launch on the LOOP exit edge while catch-up is still
  //pending. Its clocks must follow that burst rather than overwrite it.
  if (superlooperClockQueueBusy()) superlooperQueueSequencerClocks(sequencerAdvances);
  else superlooperSendSequencerClocks(sequencerAdvances);
  superlooperTrackSequencerAdvance(sequencerAdvances, 1);
}

void superlooperUpdateMutationClockGate() {
  if (clockState && !superlooperMutationClockStateOld) {
    unsigned int sequencerAdvances = superlooperTranslateLogicalClockAdvances(1);
    superlooperMutationClockGate = sequencerAdvances > 0;
    if (superlooperMutationClockGate && superlooperClockQueueBusy()) {
      //Serialize this live edge after the catch-up clocks. Leave its gate low
      //for the whole input pulse, even if the queue drains before it falls.
      superlooperQueueSequencerClocks(sequencerAdvances);
      superlooperMutationClockGate = 0;
    }
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

//Reverse the times of recorded attacks, not gate voltage edges. Pulse widths
//still run forward in playback time so a short gate attacks on its beat.
uint16_t superlooperReverseGateWidth(uint16_t cell, uint8_t channel) {
  uint16_t width = 0;
  while (width < superlooperLoopBaseCellCount) {
    uint16_t captured = superlooperCapturedCellForPhraseCell(cell);
    uint16_t raw = superlooperWrapRecordIndex((long)superlooperCaptureStartIndex
      + captured / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK);
    uint8_t tick = captured % SUPERLOOPER_SUBDIVISIONS_PER_CLOCK;
    uint8_t available = SUPERLOOPER_SUBDIVISIONS_PER_CLOCK - tick;
    uint32_t bits = superlooperRenderedTape[raw][channel] >> tick;
    uint8_t run = 0;
    if (bits == (0xffffffffUL >> tick)) run = available;
    else while (bits & 1) { ++run; bits >>= 1; }
    width += run;
    if (width >= superlooperLoopBaseCellCount) return 0xffff; //Held for the whole phrase.
    if (run < available) return width;
    cell = (cell + run) % superlooperLoopBaseCellCount;
  }
  return width;
}

uint8_t superlooperReverseGateMask() {
  uint8_t mask = 0;
  for (uint8_t channel = 0; channel < 4; ++channel)
    if (superlooperReverseGateRemaining[channel]) mask |= 1 << channel;
  return mask;
}

void superlooperStartReverseGates(uint8_t mask) {
  //Reversing unequal-width gates can make their tails overlap. Separate a
  //new attack from an already-high output by one service pass, then reassert
  //it even if the recorded pulse is shorter than that service interval.
  superlooperReverseRetriggerMask |= mask & superlooperReverseLastOutputMask;
  for (uint8_t channel = 0; channel < 4; ++channel)
    if (mask & (1 << channel)) superlooperReverseGateRemaining[channel] =
      superlooperReverseGateWidth(superlooperLoopBaseCell, channel);
}

void superlooperResetReverseGates() {
  superlooperReverseLastOutputMask = superlooperReverseRetriggerMask = 0;
  for (uint8_t channel = 0; channel < 4; ++channel) superlooperReverseGateRemaining[channel] = 0;
  if (superlooperLoopReverse)
    superlooperStartReverseGates(superlooperRenderedLoopCellState(superlooperLoopBaseCell));
}

void superlooperAlignTransportCell(uint16_t cell, bool retrigger) {
  uint16_t captured = superlooperCapturedCellForPhraseCell(cell);
  uint16_t raw = superlooperWrapRecordIndex((long)superlooperCaptureStartIndex
    + captured / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK);
  uint8_t lastTick = captured % SUPERLOOPER_SUBDIVISIONS_PER_CLOCK;
  uint8_t target = superlooperTransportStageAtStart[raw];
  for (uint8_t tick = 0; tick <= lastTick; ++tick)
    target = (target + superlooperTransportAdvanceAt(raw, tick)) % SUPERLOOPER_STAGE_COUNT;
  uint8_t clocks = (target + SUPERLOOPER_STAGE_COUNT - superlooperTrackedSequencerStage)
    % SUPERLOOPER_STAGE_COUNT;
  if (!clocks && retrigger) clocks = SUPERLOOPER_STAGE_COUNT;
  superlooperQueueTrackedSequencerClocks(clocks);
  superlooperLoopTransportStage = superlooperTrackedSequencerStage;
}

void superlooperReverseTransportCell(uint16_t cell) {
  uint16_t captured = superlooperCapturedCellForPhraseCell(cell);
  uint16_t raw = superlooperWrapRecordIndex((long)superlooperCaptureStartIndex
    + captured / SUPERLOOPER_SUBDIVISIONS_PER_CLOCK);
  //Hold the last event's position until the next recorded onset is reached.
  //Restoring checkpoints at clock tails would advance a stage one beat early.
  if (superlooperTransportAdvanceAt(raw, captured % SUPERLOOPER_SUBDIVISIONS_PER_CLOCK))
    superlooperAlignTransportCell(cell, 1);
}

void superlooperPlayLoopStartTransport() {
  if (superlooperLoopReverse) superlooperAlignTransportCell(superlooperLoopBaseCell, 1);
  else superlooperPlayTransportCell(superlooperLoopBaseCell);
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
  uint8_t relativeStage = (stageCells > 0) ? (superlooperLoopBaseCell
    + (superlooperLoopReverse ? stageCells - 1 : 0)) / stageCells : 0;
  relativeStage %= loopLength;
  loopIndex = (superlooperLoopBaseStartStage + relativeStage) % SUPERLOOPER_STAGE_COUNT;
}

void superlooperConfigureLoopLength(unsigned int requestedLength) {
  loopLength = max((unsigned int)1,
    min((unsigned int)SUPERLOOPER_STAGE_COUNT, requestedLength));
  superlooperLoopBaseStartStage = SUPERLOOPER_STAGE_COUNT - loopLength;
  superlooperLoopBaseCellCount = (uint16_t)loopLength * superlooperCellsPerStage();
  //Anchor reverse to a stage attack. Subsequent cells still run backward.
  superlooperLoopBaseCell = superlooperLoopReverse
    ? (loopLength - 1) * superlooperCellsPerStage() : 0;
  superlooperSetLoopPositionVariables();
}

void superlooperAdvanceLoopCell() {
  uint16_t previousCell = superlooperLoopBaseCell;
  uint8_t previousState = superlooperRenderedLoopCellState(superlooperLoopBaseCell);
  bool reverse = superlooperLoopReverse;
  superlooperLoopBaseCell = reverse
    ? (superlooperLoopBaseCell == 0 ? superlooperLoopBaseCellCount - 1 : superlooperLoopBaseCell - 1)
    : (superlooperLoopBaseCell + 1) % superlooperLoopBaseCellCount;

  uint8_t currentState = superlooperRenderedLoopCellState(superlooperLoopBaseCell);
  if (reverse) {
    for (uint8_t channel = 0; channel < 4; ++channel)
      if (superlooperReverseGateRemaining[channel] && superlooperReverseGateRemaining[channel] != 0xffff)
        --superlooperReverseGateRemaining[channel];
    uint16_t beforeAttack = superlooperLoopBaseCell == 0
      ? superlooperLoopBaseCellCount - 1 : superlooperLoopBaseCell - 1;
    uint8_t attacks = currentState & ~superlooperRenderedLoopCellState(beforeAttack);
    superlooperStartReverseGates(attacks);
    superlooperLoopCrossedTriggerMask |= attacks;
  }
  else superlooperLoopCrossedTriggerMask |= currentState & ~previousState;

  if (externalSequencerTracking) {
    if (reverse) superlooperReverseTransportCell(superlooperLoopBaseCell);
    else superlooperPlayTransportCell(superlooperLoopBaseCell);
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

  int16_t speed = superlooperSelectedLoopSpeed(superlooperLoopSpeedControl);
  if (speed && ((speed < 0) != superlooperLoopReverse)) {
    superlooperLoopReverse = speed < 0;
    superlooperResetReverseGates();
    if (externalSequencerTracking) superlooperAlignTransportCell(superlooperLoopBaseCell, 0);
  }
  superlooperLoopSpeedQ8 = speed;
  if (speed == 0) return;
  //Accumulator units are microseconds * Q8 speed. Fold the subdivision into
  //the per-cell threshold while keeping this hot path in 32-bit AVR math.
  superlooperLoopTransportAccumulator += (uint32_t)elapsed * (speed < 0 ? -speed : speed);
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
  superlooperLoopSpeedControl = superlooperLoopParam;
  superlooperLoopSpeedQ8 = superlooperSelectedLoopSpeed(superlooperLoopSpeedControl);
  if (superlooperLoopSpeedQ8) superlooperLoopReverse = superlooperLoopSpeedQ8 < 0;
  superlooperConfigureLoopLength(superlooperLoopLength);
  //At phase zero the last complete stage just played: start one stage before
  //it. Mid-stage entry uses the most recent complete stage. L=1 repeats itself.
  if (superlooperLoopReverse && superlooperSequencerClockPhase == 0 && loopLength > 1)
    superlooperLoopBaseCell -= superlooperCellsPerStage();
  superlooperSetLoopPositionVariables();
  superlooperResetReverseGates();
  superlooperLoopPendingLength = loopLength;
  superlooperLoopLengthChangePending = 0;
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
    (superlooperLoopCaptureNaturalStage + loopIndex)
    % SUPERLOOPER_STAGE_COUNT;
  if (externalSequencerTracking) {
    superlooperPlayLoopStartTransport();
  }
  else if (superlooperLoopReverse && superlooperLoopSpeedQ8) superlooperSendLoopStage(firstCapturedStage, 1);
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
    superlooperSpeedPickup = 1;
    superlooperLoopLength = lengthPosition + 1;
    superlooperLoopPendingLength = superlooperLoopLength;
    superlooperLoopLengthInitialized = 1;
  }
  loopEnable = 1;
  //Keep speed, direction, and quantizer hysteresis from the previous loop.
  //The control-bank pickup below protects them until PARAM moves.
  superlooperMutationClockGate = 0;
  superlooperArmControlPickup(1);
  superlooperCaptureCompleteWindow();
  superlooperInitializeLoopTransport();

  //The tape already contains the mutation result. No recorded output is sent
  //through the mutation state machine a second time.
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  carryOverActive = 0;
  superlooperResetMutationScheduler();
  saveLoopData();
}

void superlooperExitLoop() {
  //Slow external sequencers can leave playback pulses queued at high speeds.
  //Exit catches up from the position actually emitted, without draining that
  //obsolete playback backlog first. N's independent raw-clock phase survives.
  superlooperDiscardQueuedClocks();
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
  superlooperResetMutationScheduler();
  superlooperArmControlPickup(0);
  saveLoopData();
}

void superlooperDetermineLooperStatus() {
  bool requested = superlooperLoopRequestPending
    ? superlooperLoopRequestedState
    : loopEnable;
  bool requestChanged = 0;
  if (loopButtonActiveEdge && !menuMode) {
    requested = !requested;
    requestChanged = 1;
  }

  if (loopInputBehavior) {
    if (loopGateActiveEdge) {
      requested = 1;
      requestChanged = 1;
    }
    if (loopGateFallingEdge) {
      requested = 0;
      requestChanged = 1;
    }
  }
  else if (loopGateActiveEdge) {
    requested = !requested;
    requestChanged = 1;
  }

  if (requestChanged) {
    superlooperLoopRequestedState = requested;
    superlooperLoopRequestPending = (requested != loopEnable);
  }

  //Loop changes are musical transport events. Latch controls immediately but
  //commit both entry and exit only on a raw CLOCK edge.
  if (!superlooperLoopRequestPending || !clockEdge) return;
  superlooperLoopRequestPending = 0;
  if (!loopEnable && superlooperLoopRequestedState) superlooperEnterLoop();
  else if (loopEnable && !superlooperLoopRequestedState) superlooperExitLoop();
}

uint8_t superlooperUpdateLoopPlayback() {
  uint8_t deferredReverseMask = superlooperReverseRetriggerMask;
  superlooperReverseRetriggerMask = 0;
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
    superlooperResetReverseGates();
    superlooperLoopLengthChangePending = 0;
    uint8_t firstCapturedStage =
      (superlooperLoopCaptureNaturalStage + loopIndex)
      % SUPERLOOPER_STAGE_COUNT;
    if (externalSequencerTracking) {
      superlooperPlayLoopStartTransport();
    }
    else superlooperSendLoopStage(firstCapturedStage, 0);
  }

  superlooperSetLoopPositionVariables();
  if (superlooperLoopSpeedQ8 == 0) {
    superlooperReverseLastOutputMask = 0;
    return 0; //Pause freezes tape and silences gates.
  }
  uint8_t mask = superlooperLoopCrossedTriggerMask;
  if (superlooperLoopReverse) mask |= superlooperReverseGateMask() | deferredReverseMask;
  else mask |= superlooperRenderedLoopCellState(superlooperLoopBaseCell);
  mask &= ~superlooperReverseRetriggerMask;
  superlooperReverseLastOutputMask = mask;
  return mask;
}

unsigned int superlooperMutationRawClockCount(unsigned int sampledLength) {
  sampledLength = max((unsigned int)1, sampledLength);
  return sampledLength * superlooperClocksPerStep;
}

unsigned long superlooperMutationDurationMicros(unsigned int sampledLength) {
  unsigned long period = lastClockInterval > 0 ? lastClockInterval : 125000UL;
  unsigned int rawClockCount = superlooperMutationRawClockCount(sampledLength);
  if (period > (ULONG_MAX / rawClockCount)) return ULONG_MAX;
  return period * rawClockCount;
}

void superlooperResetMutationScheduler() {
  for (uint8_t lane = 0; lane < 5; ++lane) {
    superlooperMutationLanes[lane] = SuperlooperMutationLaneState {};
    superlooperMutationWindows[lane] = 0;
  }
  startModification = startModification1 = startModification2
                    = startModification3 = startModification4 = 0;
  endModification = endModification1 = endModification2
                  = endModification3 = endModification4 = 0;
  carryOver = carryOver1 = carryOver2 = carryOver3 = carryOver4 = 0;
  carryOverActive = 0;
}

void superlooperSetTriggerLaneLength(uint8_t channel, unsigned int value) {
  if (channel == 0) modifyLength1 = value;
  if (channel == 1) modifyLength2 = value;
  if (channel == 2) modifyLength3 = value;
  if (channel == 3) modifyLength4 = value;
}

void superlooperSetTriggerLaneOriginalLength(uint8_t channel, unsigned int value) {
  if (channel == 0) originalModifyLength1 = value;
  if (channel == 1) originalModifyLength2 = value;
  if (channel == 2) originalModifyLength3 = value;
  if (channel == 3) originalModifyLength4 = value;
}

void superlooperSetTriggerLaneMode(uint8_t channel, unsigned int value) {
  if (channel == 0) mode1 = value;
  if (channel == 1) mode2 = value;
  if (channel == 2) mode3 = value;
  if (channel == 3) mode4 = value;
}

void superlooperSetTriggerLaneParam(uint8_t channel) {
  if (channel == 0) {
    modifyParam1 = modifyParam;
    unscaledParam1 = unscaledParam;
    ratchetAmount1 = ratchetAmount;
  }
  if (channel == 1) {
    modifyParam2 = modifyParam;
    unscaledParam2 = unscaledParam;
    ratchetAmount2 = ratchetAmount;
  }
  if (channel == 2) {
    modifyParam3 = modifyParam;
    unscaledParam3 = unscaledParam;
    ratchetAmount3 = ratchetAmount;
  }
  if (channel == 3) {
    modifyParam4 = modifyParam;
    unscaledParam4 = unscaledParam;
    ratchetAmount4 = ratchetAmount;
  }
}

void superlooperSetTriggerLaneStartFlag(uint8_t channel) {
  if (channel == 0) startModification1 = 1;
  if (channel == 1) startModification2 = 1;
  if (channel == 2) startModification3 = 1;
  if (channel == 3) startModification4 = 1;
}

void superlooperResetTriggerLaneModeState(uint8_t channel, uint8_t selectedMode) {
  if (channel == 0) {
    trig1Divide = (unsigned int)-1;
    bounce1Time = 0;
    breakIndex1 = 0;
    forceBounce1 = (selectedMode == 3) && trig1In;
  }
  if (channel == 1) {
    trig2Divide = (unsigned int)-1;
    bounce2Time = 0;
    breakIndex2 = 0;
    forceBounce2 = (selectedMode == 3) && trig2In;
  }
  if (channel == 2) {
    trig3Divide = (unsigned int)-1;
    bounce3Time = 0;
    breakIndex3 = 0;
    forceBounce3 = (selectedMode == 3) && trig3In;
  }
  if (channel == 3) {
    trig4Divide = (unsigned int)-1;
    bounce4Time = 0;
    breakIndex4 = 0;
    forceBounce4 = (selectedMode == 3) && trig4In;
  }
}

void superlooperApplyLaunchProbabilityPenalty(uint8_t lane, uint8_t sampledLength) {
  unsigned int penalty = sampledLength - 1;
  if (!splitMode || (lane == 0)) {
    probabilityModifier = max(probabilityModifier, penalty);
    return;
  }
  if (lane == 1) probabilityModifier1 = max(probabilityModifier1, penalty);
  if (lane == 2) probabilityModifier2 = max(probabilityModifier2, penalty);
  if (lane == 3) probabilityModifier3 = max(probabilityModifier3, penalty);
  if (lane == 4) probabilityModifier4 = max(probabilityModifier4, penalty);
}

void superlooperEndClockMutation() {
  SuperlooperMutationLaneState &state = superlooperMutationLanes[0];
  if (!state.active) return;

  if (((originalModifyLength > 1) || (mode == 7))
      && externalSequencerTracking) {
    if (mode == 7) {
      //Skip counts logical raw clocks. One complete external sequence spans
      //8*N of those clocks; stock's modulo-eight correction loses N's phase.
      unsigned int cycleClocks = SUPERLOOPER_STAGE_COUNT * superlooperClocksPerStep;
      unsigned int elapsedClocks = originalModifyLength - modifyLength
                                + (modifyLength > 0);
      unsigned int emittedClocks = (clockSkipCycleCount + clockRatchetCycleCount)
                                % cycleClocks;
      unsigned int correction = ((elapsedClocks % cycleClocks)
                                  + cycleClocks - emittedClocks) % cycleClocks;
      superlooperAdvanceLogicalTransport(correction);
    }
    else maintainCycle();
  }
  state.active = 0;
  state.clocksRemaining = 0;
  modifyLength = 0;
  endModification = 1;
}

void superlooperEndTriggerMutation(uint8_t channel) {
  SuperlooperMutationLaneState &state = superlooperMutationLanes[channel + 1];
  state.active = 0;
  superlooperSetTriggerLaneLength(channel, 0);
  if (channel == 0) endModification1 = 1;
  if (channel == 1) endModification2 = 1;
  if (channel == 2) endModification3 = 1;
  if (channel == 3) endModification4 = 1;
}

void superlooperLaunchClockMutation(uint8_t selectedMode, unsigned long currentMicros) {
  SuperlooperMutationLaneState &state = superlooperMutationLanes[0];
  if (state.active) superlooperEndClockMutation();

  uint8_t sampledLength = max((unsigned int)1, lengthScaled);
  unsigned int rawClockCount = superlooperMutationRawClockCount(sampledLength);
  state.active = 1;
  state.activeMode = selectedMode;
  state.sampledLength = sampledLength;
  state.clocksRemaining = rawClockCount;
  state.startMicros = currentMicros;
  state.durationMicros = superlooperMutationDurationMicros(sampledLength);

  mode = selectedMode;
  modifyLength = originalModifyLength = rawClockCount;
  firstStepTime = currentMicros;
  startModification = 1;
  clockDivide = (unsigned int)-1;
  if (selectedMode == 7) {
    setClockSkipMode();
    forceSkip = clockIn;
    clockSkipCycleCount = 0;
    clockRatchetCycleCount = 0;
    clockRatchetStateOld = 0;
  }
  superlooperApplyLaunchProbabilityPenalty(0, sampledLength);
}

void superlooperLaunchTriggerMutation(
  uint8_t channel,
  uint8_t selectedMode,
  unsigned long currentMicros
) {
  SuperlooperMutationLaneState &state = superlooperMutationLanes[channel + 1];
  uint8_t sampledLength = max((unsigned int)1, lengthScaled);
  unsigned int rawClockCount = superlooperMutationRawClockCount(sampledLength);
  state.active = 1;
  state.activeMode = selectedMode;
  state.sampledLength = sampledLength;
  state.clocksRemaining = 0;
  state.startMicros = currentMicros;
  state.durationMicros = superlooperMutationDurationMicros(sampledLength);

  superlooperSetTriggerLaneMode(channel, selectedMode);
  superlooperSetTriggerLaneLength(channel, rawClockCount);
  superlooperSetTriggerLaneOriginalLength(channel, rawClockCount);
  superlooperSetTriggerLaneParam(channel);
  superlooperSetTriggerLaneStartFlag(channel);
  superlooperResetTriggerLaneModeState(channel, selectedMode);
  superlooperApplyLaunchProbabilityPenalty(channel + 1, sampledLength);
}

unsigned int superlooperProbabilityRoll(unsigned int penalty) {
  return min(random(probabilityUpperThreshold)
             + (penalty * probabilityLengthPenalty),
             probabilityUpperThreshold);
}

bool superlooperChanceIsOff() {
  return probRead <= max(superlooperChanceDeadZone, probabilityLowerThreshold);
}

bool superlooperWindowFromRoll(unsigned int roll) {
  return (roll < probRead) && !superlooperChanceIsOff() && !menuMode;
}

void superlooperOpenMutationWindows() {
  //Bottom means no new mutations immediately, including off-grid hits that
  //would otherwise use a window opened before the slider moved down.
  if (superlooperChanceIsOff()) {
    for (uint8_t lane = 0; lane < 5; ++lane) superlooperMutationWindows[lane] = 0;
  }
  if (!clockEdge) return;

  if (probabilityModifier > 0) probabilityModifier -= 1;
  if (probabilityModifier1 > 0) probabilityModifier1 -= 1;
  if (probabilityModifier2 > 0) probabilityModifier2 -= 1;
  if (probabilityModifier3 > 0) probabilityModifier3 -= 1;
  if (probabilityModifier4 > 0) probabilityModifier4 -= 1;

  randomValue = superlooperProbabilityRoll(probabilityModifier);
  superlooperMutationWindows[0] = superlooperWindowFromRoll(randomValue);
  if (!splitMode) {
    for (uint8_t lane = 1; lane < 5; ++lane) {
      superlooperMutationWindows[lane] = superlooperMutationWindows[0];
    }
    randomValue1 = randomValue2 = randomValue3 = randomValue4 = randomValue;
    return;
  }

  randomValue1 = superlooperProbabilityRoll(probabilityModifier1);
  randomValue2 = superlooperProbabilityRoll(probabilityModifier2);
  randomValue3 = superlooperProbabilityRoll(probabilityModifier3);
  randomValue4 = superlooperProbabilityRoll(probabilityModifier4);
  superlooperMutationWindows[1] = superlooperWindowFromRoll(randomValue1);
  superlooperMutationWindows[2] = superlooperWindowFromRoll(randomValue2);
  superlooperMutationWindows[3] = superlooperWindowFromRoll(randomValue3);
  superlooperMutationWindows[4] = superlooperWindowFromRoll(randomValue4);
}

void superlooperExpireMutations(unsigned long currentMicros) {
  if (clockEdge && superlooperMutationLanes[0].active
      && (currentMicros != superlooperMutationLanes[0].startMicros)) {
    SuperlooperMutationLaneState &clockLane = superlooperMutationLanes[0];
    if (clockLane.clocksRemaining > 0) clockLane.clocksRemaining -= 1;
    modifyLength = clockLane.clocksRemaining;
    if (clockLane.clocksRemaining == 0) superlooperEndClockMutation();
  }

  for (uint8_t channel = 0; channel < 4; ++channel) {
    SuperlooperMutationLaneState &state = superlooperMutationLanes[channel + 1];
    if (state.active && ((currentMicros - state.startMicros) >= state.durationMicros)) {
      superlooperEndTriggerMutation(channel);
    }
  }
}

void superlooperConsiderClockMutation(unsigned long currentMicros) {
  if (!clockEdge || !superlooperMutationWindows[0]) return;
  uint8_t selectedMode = modePosition;
  SuperlooperMutationLaneState &state = superlooperMutationLanes[0];
  if (!state.active || (state.activeMode != selectedMode)) {
    superlooperLaunchClockMutation(selectedMode, currentMicros);
  }
}

void superlooperConsiderTriggerMutation(
  uint8_t channel,
  bool triggerEdge,
  unsigned long currentMicros
) {
  if (!triggerEdge || !superlooperMutationWindows[channel + 1]) return;
  uint8_t selectedMode = modePosition;
  SuperlooperMutationLaneState &state = superlooperMutationLanes[channel + 1];
  if (!state.active || (state.activeMode != selectedMode)) {
    superlooperLaunchTriggerMutation(channel, selectedMode, currentMicros);
  }
}

#if IDUM_ENABLE_CLOCK_STOP_TIMEOUT
void superlooperHandleClockTimeout(unsigned long currentMicros) {
  if (lastClockInterval == 0) return;
  unsigned long timeout = (clockStopTimeoutIntervals > 0
      && lastClockInterval > ULONG_MAX / clockStopTimeoutIntervals)
    ? ULONG_MAX : lastClockInterval * clockStopTimeoutIntervals;
  timeout = max(timeout, clockStopMinimumMicros);
  if ((currentMicros - lastClockTime) <= timeout) return;

  //An old Chance window must not launch a fresh mutation after clock loss.
  for (uint8_t lane = 0; lane < 5; ++lane) superlooperMutationWindows[lane] = 0;
  if (superlooperMutationLanes[0].active
      && (isClockTimeoutMode(mode) || (mode == 7))) {
    //Cancel rather than catch up: clock loss must not generate another burst.
    superlooperMutationLanes[0].active = 0;
    superlooperMutationLanes[0].clocksRemaining = 0;
    modifyLength = 0;
    endModification = 1;
    clockState = 0;
    clockChoke = 1;
    clockRatchet = clockSkip = clockRatchetState = clockRatchetStateOld = forceSkip = 0;
    superlooperMutationClockGate = 0;
    superlooperMutationClockStateOld = clockIn;
  }
  for (uint8_t channel = 0; channel < 4; ++channel) {
    const SuperlooperMutationLaneState &state = superlooperMutationLanes[channel + 1];
    if (state.active && isClockTimeoutMode(state.activeMode)) {
      superlooperEndTriggerMutation(channel);
    }
  }
}
#endif

void superlooperMutationScheduler() {
  startModification = startModification1 = startModification2
                    = startModification3 = startModification4 = 0;
  endModification = endModification1 = endModification2
                  = endModification3 = endModification4 = 0;
  carryOver = carryOver1 = carryOver2 = carryOver3 = carryOver4 = 0;
  carryOverActive = 0;

  unsigned long currentMicros = micros();
#if IDUM_ENABLE_CLOCK_STOP_TIMEOUT
  superlooperHandleClockTimeout(currentMicros);
#endif
  if (modeBtnFallingEdge) {
    superlooperEndClockMutation();
    for (uint8_t channel = 0; channel < 4; ++channel) {
      if (superlooperMutationLanes[channel + 1].active) {
        superlooperEndTriggerMutation(channel);
      }
    }
  }

  superlooperExpireMutations(currentMicros);
  superlooperOpenMutationWindows();
  superlooperConsiderClockMutation(currentMicros);
  superlooperConsiderTriggerMutation(0, trig1Edge, currentMicros);
  superlooperConsiderTriggerMutation(1, trig2Edge, currentMicros);
  superlooperConsiderTriggerMutation(2, trig3Edge, currentMicros);
  superlooperConsiderTriggerMutation(3, trig4Edge, currentMicros);

  modificationActive = 0;
  for (uint8_t lane = 0; lane < 5; ++lane) {
    if (superlooperMutationLanes[lane].active) modificationActive = 1;
  }
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

uint16_t superlooperBallStrengthQ8(unsigned int control) {
  control = min((unsigned int)1024, control);

  //The center is a true dry zone. Moving away from it continuously adds more
  //of the curve, reaching the full excursion only at the ends of the control.
  if (control < 480) return ((uint32_t)(480 - control) * 256) / 480;
  if (control > 544) return ((uint32_t)(control - 544) * 256) / 480;
  return 0;
}

uint32_t superlooperBallRateQ8ForPeriod(unsigned long periodMicros) {
  unsigned long fastPeriod = generatedTriggerPulseWidth
                           + SUPERLOOPER_BALL_FAST_LOW_MICROS;
  periodMicros = max(fastPeriod, periodMicros);
  return SUPERLOOPER_BALL_PHASE_ONE / periodMicros;
}

unsigned long superlooperBallTriggerInterval(uint8_t channel) {
  unsigned long triggerInterval = 0;
  if (channel == 0) triggerInterval = lastTrig1Interval;
  if (channel == 1) triggerInterval = lastTrig2Interval;
  if (channel == 2) triggerInterval = lastTrig3Interval;
  if (channel == 3) triggerInterval = lastTrig4Interval;

  unsigned long fastPeriod = generatedTriggerPulseWidth
                           + SUPERLOOPER_BALL_FAST_LOW_MICROS;
  bool plausible = triggerInterval >= fastPeriod;
  if (plausible && (lastClockInterval > 0)) {
    unsigned long longestPlausible = lastClockInterval > (ULONG_MAX / 16UL)
      ? ULONG_MAX
      : lastClockInterval * 16UL;
    plausible = triggerInterval <= longestPlausible;
  }
  if (plausible) return triggerInterval;
  if (lastClockInterval >= fastPeriod) return lastClockInterval;
  return max(fastPeriod, 125000UL);
}

uint16_t superlooperBallProgressQ8(
  uint8_t channel,
  unsigned long currentMicros
) {
  const SuperlooperBallState &state = superlooperBallStates[channel];
  if (state.mutationDurationMicros == 0) return 256;
  unsigned long elapsed = currentMicros - state.mutationStartMicros;
  if (elapsed >= state.mutationDurationMicros) return 256;

  uint16_t linear;
  if (elapsed <= (ULONG_MAX / 256UL)) {
    linear = ((uint32_t)elapsed * 256UL) / state.mutationDurationMicros;
  }
  else {
    //Only extraordinarily slow clocks reach this path. Scaling the divisor
    //first avoids wrapping the 32-bit numerator while preserving the shape.
    linear = min((unsigned long)255,
      elapsed / max(1UL, state.mutationDurationMicros / 256UL));
  }
  uint16_t curved = linear;
  uint16_t squared = ((uint32_t)linear * linear) / 256;
  uint8_t sampled = max((uint8_t)1, state.sampledLength);
  uint8_t live = max((uint8_t)1, state.liveLength);

  if (live < sampled) curved = (2 * linear) - squared;
  else if (live > sampled) curved = squared;
  else return linear;

  uint8_t distance = live > sampled ? live - sampled : sampled - live;
  uint16_t blend = min((uint16_t)256, (uint16_t)(distance * 36));
  return (((uint32_t)linear * (256 - blend))
       + ((uint32_t)curved * blend)) / 256;
}

uint32_t superlooperBallTargetRateQ8(
  uint8_t channel,
  unsigned int control,
  unsigned long currentMicros
) {
  SuperlooperBallState &state = superlooperBallStates[channel];
  state.strengthQ8 = superlooperBallStrengthQ8(control);
  state.speeding = control < 512;
  uint16_t progress = superlooperBallProgressQ8(channel, currentMicros);
  uint16_t directionProgress = state.speeding ? progress : 256 - progress;
  uint32_t excursion = state.fastRateQ8 - state.slowRateQ8;
  uint32_t scaledProgress = ((uint32_t)state.strengthQ8 * directionProgress) / 256;
  return state.slowRateQ8 + ((excursion * scaledProgress) / 256);
}

void superlooperPrepareBallChannel(uint8_t channel, unsigned int sampledLength) {
  SuperlooperBallState &state = superlooperBallStates[channel];
  SuperlooperMutationLaneState &mutation = superlooperMutationLanes[channel + 1];
  state.prepared = 1;
  state.started = 0;
  state.sampledLength = max((unsigned int)1, sampledLength);
  state.liveLength = max((unsigned int)1, lengthScaled);
  state.strengthQ8 = 0;
  state.mutationStartMicros = mutation.active ? mutation.startMicros : micros();
  state.mutationDurationMicros = mutation.active
    ? mutation.durationMicros
    : superlooperMutationDurationMicros(state.sampledLength);
  state.lastUpdateMicros = state.mutationStartMicros;
  state.pulseStartMicros = state.mutationStartMicros;
  state.phase = 0;
  state.slowRateQ8 = superlooperBallRateQ8ForPeriod(
    superlooperBallTriggerInterval(channel)
  );
  state.fastRateQ8 = superlooperBallRateQ8ForPeriod(
    generatedTriggerPulseWidth + SUPERLOOPER_BALL_FAST_LOW_MICROS
  );
  state.currentRateQ8 = state.slowRateQ8;
}

void superlooperBallChannel(
  uint8_t channel,
  bool inputState,
  bool inputEdge,
  bool &forceStart,
  bool &outputState,
  bool &outputChoke,
  unsigned int &divider,
  unsigned int control
) {
  SuperlooperBallState &state = superlooperBallStates[channel];
  unsigned long currentMicros = micros();
  if (!state.prepared) superlooperPrepareBallChannel(channel, 1);
  state.liveLength = max((unsigned int)1, lengthScaled);

  bool startSignal = inputEdge || forceStart;
  uint32_t targetRateQ8 = superlooperBallTargetRateQ8(
    channel,
    control,
    currentMicros
  );

  if (!state.started && startSignal) {
    state.started = 1;
    state.phase = 0;
    state.pulseStartMicros = currentMicros;
    state.lastUpdateMicros = currentMicros;
    state.currentRateQ8 = targetRateQ8;
    divider = 0;
  }
  forceStart = 0;

  //At the center the mutation is inaudible: the trigger input passes through
  //and no phase-generated Ball events are allowed to leak into the output.
  if (state.strengthQ8 == 0) {
    outputState = inputState;
    outputChoke = 1;
    state.phase = 0;
    state.currentRateQ8 = state.slowRateQ8;
    state.lastUpdateMicros = currentMicros;
    return;
  }

  if (!state.started) {
    outputState = 0;
    return;
  }

  unsigned long elapsed = currentMicros - state.lastUpdateMicros;
  unsigned long integrationElapsed = min(elapsed, 100000UL);
  uint32_t previousRateQ8 = state.currentRateQ8;
  if (elapsed >= SUPERLOOPER_BALL_CONTROL_SLEW_MICROS) {
    state.currentRateQ8 = targetRateQ8;
  }
  else if (elapsed > 0) {
    int32_t difference = (int32_t)targetRateQ8 - (int32_t)state.currentRateQ8;
    int32_t adjustment = (difference * (int32_t)elapsed)
                       / (int32_t)SUPERLOOPER_BALL_CONTROL_SLEW_MICROS;
    state.currentRateQ8 = (uint32_t)((int32_t)state.currentRateQ8 + adjustment);
  }

  uint32_t averageRateQ8 = (previousRateQ8 + state.currentRateQ8) / 2;
  state.phase += averageRateQ8 * integrationElapsed;
  if (state.phase >= SUPERLOOPER_BALL_PHASE_ONE) {
    state.phase %= SUPERLOOPER_BALL_PHASE_ONE;
    state.pulseStartMicros = currentMicros;
  }
  state.lastUpdateMicros = currentMicros;
  outputState = (currentMicros - state.pulseStartMicros)
              < generatedTriggerPulseWidth;
}

void superlooperBall() {
  if ((modifyLength > 0) && (mode == 3)) {
    clockState = ((micros() - firstStepTime) < generatedClockPulseWidth);
  }

  if ((modifyLength1 > 0) && (mode1 == 3)) {
    superlooperBallChannel(0, trig1In, trig1Edge, forceBounce1,
      trig1State, trig1Choke, trig1Divide, modifyParam1);
  }
  if ((modifyLength2 > 0) && (mode2 == 3)) {
    superlooperBallChannel(1, trig2In, trig2Edge, forceBounce2,
      trig2State, trig2Choke, trig2Divide, modifyParam2);
  }
  if ((modifyLength3 > 0) && (mode3 == 3)) {
    superlooperBallChannel(2, trig3In, trig3Edge, forceBounce3,
      trig3State, trig3Choke, trig3Divide, modifyParam3);
  }
  if ((modifyLength4 > 0) && (mode4 == 3)) {
    superlooperBallChannel(3, trig4In, trig4Edge, forceBounce4,
      trig4State, trig4Choke, trig4Divide, modifyParam4);
  }
}

void superlooperCalculateTriggerStates() {
  if (modifyLength == 0) {
    clockState = clockIn;
    clockChoke = 1;
  }
  if (modifyLength1 == 0) {
    trig1State = trig1In;
    trig1Choke = 1;
  }
  if (modifyLength2 == 0) {
    trig2State = trig2In;
    trig2Choke = 1;
  }
  if (modifyLength3 == 0) {
    trig3State = trig3In;
    trig3Choke = 1;
  }
  if (modifyLength4 == 0) {
    trig4State = trig4In;
    trig4Choke = 1;
  }

  if (!modificationActive) return;
  if (clockEdge && !cycleActive) clockChoke = 1;

  for (uint8_t activeMode = 0; activeMode < 8; ++activeMode) {
    if ((mode != activeMode) && (mode1 != activeMode) && (mode2 != activeMode)
        && (mode3 != activeMode) && (mode4 != activeMode)) continue;

    switch (activeMode) {
      case 0: hold(); break;
      case 1: burst(); break;
      case 2: ratchet(); break;
      case 3: superlooperBall(); break;
      case 4: rotate(); break;
      case 5: trigDelay(); break;
      case 6: breakBeat(); break;
      case 7: skip(); break;
    }
  }
}

#ifdef IDUM_SIM
bool superlooperMutationDebugWindowOpen(uint8_t lane) {
  return superlooperMutationWindows[lane];
}

bool superlooperMutationDebugActive(uint8_t lane) {
  return superlooperMutationLanes[lane].active;
}

uint8_t superlooperMutationDebugMode(uint8_t lane) {
  return superlooperMutationLanes[lane].activeMode;
}

unsigned long superlooperMutationDebugStart(uint8_t lane) {
  return superlooperMutationLanes[lane].startMicros;
}

unsigned long superlooperMutationDebugDuration(uint8_t lane) {
  return superlooperMutationLanes[lane].durationMicros;
}

bool superlooperBallDebugStarted(uint8_t channel) {
  return superlooperBallStates[channel].started;
}

bool superlooperBallDebugSpeeding(uint8_t channel) {
  return superlooperBallStates[channel].speeding;
}

uint8_t superlooperBallDebugSampledLength(uint8_t channel) {
  return superlooperBallStates[channel].sampledLength;
}

uint32_t superlooperBallDebugPhase(uint8_t channel) {
  return superlooperBallStates[channel].phase;
}

uint32_t superlooperBallDebugSlowRateQ8(uint8_t channel) {
  return superlooperBallStates[channel].slowRateQ8;
}

uint32_t superlooperBallDebugFastRateQ8(uint8_t channel) {
  return superlooperBallStates[channel].fastRateQ8;
}

uint32_t superlooperBallDebugCurrentRateQ8(uint8_t channel) {
  return superlooperBallStates[channel].currentRateQ8;
}

unsigned long superlooperBallDebugMutationStart(uint8_t channel) {
  return superlooperBallStates[channel].mutationStartMicros;
}

unsigned long superlooperBallDebugMutationDuration(uint8_t channel) {
  return superlooperBallStates[channel].mutationDurationMicros;
}

unsigned long superlooperBallDebugPulseStart(uint8_t channel) {
  return superlooperBallStates[channel].pulseStartMicros;
}
#endif

void superlooperPrepareAtomicBallGestures() {
  const unsigned int unstarted = (unsigned int)-1;

  //Every trigger lane owns its gesture. Same-mode triggers leave this state
  //alone; a newly triggered mode gets a fresh L-long state from the scheduler.
  if (startModification1 && (mode1 == 3)) {
    trig1Divide = unstarted;
    bounce1Time = 0;
    superlooperPrepareBallChannel(0, superlooperMutationLanes[1].sampledLength);
    if (!chokeDisable && !trig1In) trig1Choke = 0;
  }
  if (startModification2 && (mode2 == 3)) {
    trig2Divide = unstarted;
    bounce2Time = 0;
    superlooperPrepareBallChannel(1, superlooperMutationLanes[2].sampledLength);
    if (!chokeDisable && !trig2In) trig2Choke = 0;
  }
  if (startModification3 && (mode3 == 3)) {
    trig3Divide = unstarted;
    bounce3Time = 0;
    superlooperPrepareBallChannel(2, superlooperMutationLanes[3].sampledLength);
    if (!chokeDisable && !trig3In) trig3Choke = 0;
  }
  if (startModification4 && (mode4 == 3)) {
    trig4Divide = unstarted;
    bounce4Time = 0;
    superlooperPrepareBallChannel(3, superlooperMutationLanes[4].sampledLength);
    if (!chokeDisable && !trig4In) trig4Choke = 0;
  }
}

#if IDUM_BALL_TIMING_DIAGNOSTIC

void superlooperBallDiagWriteU16(uint8_t address, uint16_t value) {
  EEPROM.write(address, value & 0xff);
  EEPROM.write(address + 1, value >> 8);
}

void superlooperBallDiagWriteU32(uint8_t address, uint32_t value) {
  for (uint8_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
    EEPROM.write(address + byteIndex, value >> (byteIndex * 8));
  }
}

uint16_t superlooperBallDiagReadU16(uint8_t address) {
  return EEPROM.read(address) | ((uint16_t)EEPROM.read(address + 1) << 8);
}

uint32_t superlooperBallDiagReadU32(uint8_t address) {
  uint32_t value = 0;
  for (uint8_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
    value |= (uint32_t)EEPROM.read(address + byteIndex) << (byteIndex * 8);
  }
  return value;
}

void superlooperBallTimingDiagnosticPrintStored() {
  uint8_t base = SUPERLOOPER_BALL_DIAG_EEPROM_BASE;
  if (EEPROM.read(base) != SUPERLOOPER_BALL_DIAG_MAGIC) {
    Serial.println("ball-loop no saved sample; run TR1 Ball for one clock interval");
    return;
  }

  Serial.print("ball-loop loops=");
  Serial.print(superlooperBallDiagReadU16(base + 2));
  Serial.print(" clock-us=");
  Serial.print(superlooperBallDiagReadU32(base + 4));
  Serial.print(" param=");
  Serial.print(superlooperBallDiagReadU16(base + 8));
  Serial.print(" sampled-L=");
  Serial.print(EEPROM.read(base + 10));
  Serial.print(" live-L=");
  Serial.print(EEPROM.read(base + 11));
  Serial.print(" direction=");
  Serial.print(EEPROM.read(base + 12) ? "slowing" : "speeding");
  Serial.print(" start-divider=");
  Serial.println(superlooperBallDiagReadU16(base + 13));
}

void superlooperBallTimingDiagnosticReset() {
  superlooperBallDiagClockSeen = 0;
  superlooperBallDiagIntervalActive = 0;
  superlooperBallDiagCaptureWritten = 0;
  superlooperBallDiagLoopCount = 0;
}

void superlooperBallTimingDiagnosticSetup() {
  superlooperBallTimingDiagnosticReset();
  superlooperBallTimingDiagnosticPrintStored();
  superlooperBallDiagLastPrint = micros();
}

void superlooperBallTimingDiagnosticSave() {
  uint8_t base = SUPERLOOPER_BALL_DIAG_EEPROM_BASE;
  uint16_t loops = min((uint32_t)65535, superlooperBallDiagLoopCount);
  EEPROM.write(base, 0);
  EEPROM.write(base + 1, 1);
  superlooperBallDiagWriteU16(base + 2, loops);
  superlooperBallDiagWriteU32(base + 4, lastClockInterval);
  superlooperBallDiagWriteU16(base + 8, superlooperBallDiagParam);
  EEPROM.write(base + 10, superlooperBallDiagSampledLength);
  EEPROM.write(base + 11, superlooperBallDiagLiveLength);
  EEPROM.write(base + 12, superlooperBallDiagExpanding);
  superlooperBallDiagWriteU16(base + 13, superlooperBallDiagStartDivider);
  EEPROM.write(base, SUPERLOOPER_BALL_DIAG_MAGIC);
  superlooperBallDiagCaptureWritten = 1;
}

void superlooperBallTimingDiagnosticBeginIteration() {
  if (clockEdge) {
    if (superlooperBallDiagClockSeen && superlooperBallDiagIntervalActive
        && !superlooperBallDiagCaptureWritten) {
      superlooperBallTimingDiagnosticSave();
    }
    superlooperBallDiagClockSeen = 1;
    superlooperBallDiagIntervalActive = 0;
    superlooperBallDiagLoopCount = 0;
  }

  if (superlooperBallDiagClockSeen && !superlooperBallDiagCaptureWritten
      && (superlooperBallDiagLoopCount < 65535)) {
    superlooperBallDiagLoopCount += 1;
  }
}

void superlooperBallTimingDiagnosticObserve() {
  if (!superlooperBallDiagClockSeen || superlooperBallDiagCaptureWritten
      || loopEnable || (modifyLength1 == 0) || (mode1 != 3)) return;

  if (!superlooperBallDiagIntervalActive) {
    superlooperBallDiagParam = modifyParam1;
    superlooperBallDiagSampledLength = superlooperMutationLanes[1].active
      ? superlooperMutationLanes[1].sampledLength
      : originalModifyLength1;
    superlooperBallDiagLiveLength = lengthScaled;
    superlooperBallDiagExpanding = unscaledParam1 > 8;
    superlooperBallDiagStartDivider = trig1Divide;
  }
  superlooperBallDiagIntervalActive = 1;
}

void superlooperBallTimingDiagnosticService() {
  //After the module is reconnected to USB, repeat the saved line until a new
  //clock arrives so opening the monitor after boot cannot miss the result.
  if (superlooperBallDiagClockSeen
      || ((micros() - superlooperBallDiagLastPrint) < 1000000UL)) return;
  superlooperBallDiagLastPrint = micros();
  superlooperBallTimingDiagnosticPrintStored();
}

#endif

void superlooperCalculateRenderedMutationOutputs() {
  const unsigned int unstarted = (unsigned int)-1;
  bool savedTrig1Edge = trig1Edge;
  bool savedTrig2Edge = trig2Edge;
  bool savedTrig3Edge = trig3Edge;
  bool savedTrig4Edge = trig4Edge;

  if ((mode1 == 3) && (trig1Divide != unstarted)) trig1Edge = 0;
  if ((mode2 == 3) && (trig2Divide != unstarted)) trig2Edge = 0;
  if ((mode3 == 3) && (trig3Divide != unstarted)) trig3Edge = 0;
  if ((mode4 == 3) && (trig4Divide != unstarted)) trig4Edge = 0;

  if (externalSequencerTracking) {
    superlooperCalculateTriggerStates();
    superlooperUpdateMutationClockGate();
  }
  else {
    //Without external tracking, keep CLOCK OUT on the plain N-divided input
    //while rendering trigger-channel mutations at their raw-clock timing.
    unsigned int savedClockModifyLength = modifyLength;
    modifyLength = 0;
    superlooperCalculateTriggerStates();
    modifyLength = savedClockModifyLength;
  }

  trig1Edge = savedTrig1Edge;
  trig2Edge = savedTrig2Edge;
  trig3Edge = savedTrig3Edge;
  trig4Edge = savedTrig4Edge;
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
    superlooperPlaybackEditReference = superlooperPhysicalParam;
    superlooperPlaybackEditArmed = 0;
    superlooperNDisplayActive = 1;
    superlooperNDisplayTime = micros();
    superlooperMenuDisplayLed = superlooperPlaybackOption * 2;
  }

  if (menuMode) {
    if (superlooperControlMoved(superlooperPhysicalParam, superlooperPlaybackEditReference)) {
      superlooperPlaybackEditArmed = 1;
    }
    uint8_t requested = min((unsigned int)3, superlooperPhysicalParam / 256);
    int lower = (int)superlooperPlaybackOption * 256 - 12;
    int upper = ((int)superlooperPlaybackOption + 1) * 256 + 12;
    if (superlooperPlaybackEditArmed && requested != superlooperPlaybackOption
        && ((int)superlooperPhysicalParam < lower || (int)superlooperPhysicalParam >= upper)) {
      superlooperPlaybackOption = requested;
      superlooperSpeedBin = -1;
      //Changing the control's meaning resets loop speed to +1x until
      //PARAM is deliberately picked up in LOOP after leaving the menu.
      superlooperSpeedPickup = 1;
      superlooperParamPickup = 1;
      superlooperNDisplayTime = micros();
      superlooperNDisplayActive = 1;
      superlooperMenuDisplayLed = requested * 2;
    }
  }

  if (menuMode && superlooperNEditArmed
      && (lengthPosition != superlooperNEditPosition)) {
    superlooperNEditPosition = lengthPosition;
    uint8_t requestedNOption = superlooperLengthPositionToNOption[lengthPosition];
    if (requestedNOption != superlooperNOption) {
      superlooperSetNOption(requestedNOption);
      superlooperNDisplayTime = micros();
      superlooperNDisplayActive = 1;
      superlooperMenuDisplayLed = superlooperNOption;
    }
  }

  if (!menuMode && superlooperMenuModeOld) {
    EEPROM.write(6, superlooperNOption);
    EEPROM.write(7, SUPERLOOPER_N_EEPROM_MARKER);
    EEPROM.write(8, superlooperPlaybackOption);
    EEPROM.write(9, SUPERLOOPER_PLAYBACK_EEPROM_MARKER);
    superlooperNEditArmed = 0;
    superlooperNDisplayActive = 0;
    superlooperPickupLengthReference = superlooperPhysicalLengthReading;
    superlooperLengthPickup = 1;
    superlooperPickupParamReference = superlooperPhysicalParam;
    superlooperParamPickup = 1;
  }

  superlooperMenuModeOld = menuMode;
  if (menuMode && superlooperNDisplayActive) {
    if ((micros() - superlooperNDisplayTime) < 1000000UL) {
      writeModeLed(superlooperMenuDisplayLed);
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
#if IDUM_BALL_TIMING_DIAGNOSTIC
  superlooperBallTimingDiagnosticBeginIteration();
#endif
  superlooperAdvanceSequencerTiming();
  seedRNGOnFirstClock();
  chooseMode();
  superlooperDetermineLooperStatus();

  uint8_t renderedMask = 0;
  if (loopEnable) {
    renderedMask = superlooperUpdateLoopPlayback();
  }
  else {
    superlooperMutationScheduler();
    superlooperPrepareAtomicBallGestures();
    superlooperCalculateRenderedMutationOutputs();
    renderedMask = superlooperResolvedMutationTriggerMask();
    superlooperRecordRenderedOutputs(renderedMask);
  }

#if IDUM_BALL_TIMING_DIAGNOSTIC
  superlooperBallTimingDiagnosticObserve();
#endif

  superlooperHandleNMenu();
  superlooperWriteOutputs(renderedMask, superlooperSequencerClockHigh());

#if IDUM_BALL_TIMING_DIAGNOSTIC
  superlooperBallTimingDiagnosticService();
#endif

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
  superlooperLoadPlaybackSetting();
  superlooperSpeedBin = -1;
  superlooperSpeedPickup = 0;
  superlooperPlaybackEditArmed = 0;
  superlooperLoopReverse = 0;
  superlooperResetReverseGates();
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
  superlooperLoopRequestPending = 0;
  superlooperLoopRequestedState = 0;
  superlooperResetMutationScheduler();
  for (uint8_t channel = 0; channel < 4; ++channel) {
    superlooperBallStates[channel] = SuperlooperBallState {};
  }
#if IDUM_BALL_TIMING_DIAGNOSTIC
  superlooperBallTimingDiagnosticReset();
#endif
}

#endif
