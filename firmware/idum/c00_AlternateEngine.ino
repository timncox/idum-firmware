//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// Compile-time alternate-runner selection.
//
// To add an alternate engine:
//   1. assign it an ID in engine_config.h;
//   2. add firmware/engines/<name>/ with run<Name>Engine() and
//      reset<Name>Engine();
//   3. add one branch here; and
//   4. add a Makefile name-to-ID mapping.
// Only the selected alternate is retained by the linker.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

#if IDUM_ALT_ENGINE != IDUM_ALT_ENGINE_NONE

#undef loop

bool cycleSwitchIn = 0;
bool alternateEngineActive = 0;

void classicReadInputs() {
  // The physical CYCLE switch always selects the runner. Preserve its raw
  // state before adapting the legacy stock variable to the saved tracking
  // option expected by the unchanged sequencing functions.
  readInputs();
  cycleSwitchIn = cycleIn;
  cycleIn = externalSequencerTracking;
}

void runClassicEngine() {
#if IDUM_ADC_DIAGNOSTIC
  adcDiagnosticLoop();
  return;
#endif

  cTimer.tick();
  dTimer.tick();

  classicReadInputs();

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

void loop() {
  if (alternateEngineActive) {
    runSelectedAlternateEngine();
  } else {
    runClassicEngine();
  }

  // The runner that just executed supplies the physical switch state. A
  // transition is intentionally a fresh boundary, with no phase continuity.
  if (alternateEngineActive != cycleSwitchIn) {
    alternateEngineActive = cycleSwitchIn;
    resetSelectedAlternateEngine();
  }
}

void runSelectedAlternateEngine() {
#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER
  runSuperlooperEngine();
#elif IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_BEATGEN
  runBeatgenEngine();
#else
  runClassicEngine();
#endif
}

void resetSelectedAlternateEngine() {
#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_SUPERLOOPER
  resetSuperlooperEngine();
#elif IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_BEATGEN
  resetBeatgenEngine();
#endif
}

#endif
