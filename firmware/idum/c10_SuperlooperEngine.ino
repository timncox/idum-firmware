//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// Selected alternate runner.
//
// This file is the only runtime seam between stock IDUM and an alternate
// personality. Future alternates provide the same two functions in their own
// compile-time branch. They may duplicate and change core sequencing here
// without adding policy hooks to the classic sequencing functions.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER

void superlooperReadInputs() {
  // Begin with the stock hardware read. The alternate runner is then free to
  // reinterpret the controls without changing the stock reader.
  readInputs();
  cycleSwitchIn = cycleIn;

  // Every engine shares the same control contract: menu setting 0 enables
  // external-sequencer tracking; CYCLE itself only selects the engine.
  cycleIn = externalSequencerTracking;
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
  handleLooper();
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
}

#endif
