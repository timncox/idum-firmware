//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// GREEN LED mode that scrambles each input and output
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

void rotate() {

  unsigned int rotation1 = unscaledParam1 / 2;
  unsigned int rotation2 = unscaledParam2 / 2;
  unsigned int rotation3 = unscaledParam3 / 2;
  unsigned int rotation4 = unscaledParam4 / 2;

  if ((modifyLength > 0) && (mode == 4)) clockState = ((micros() - firstStepTime) < generatedClockPulseWidth);  // set the clock to make a trigger once at the start of the modification

  //Each row describes which input feeds one output for rotation positions 0-7.
  //Keeping the permutation in one table prevents the four channel copies from
  //silently drifting apart.
  static const unsigned int rotateSource[4][8] = {
    { 1, 3, 4, 2, 1, 4, 3, 2 },
    { 3, 4, 3, 1, 2, 1, 4, 3 },
    { 2, 1, 2, 4, 3, 2, 1, 4 },
    { 4, 2, 1, 3, 4, 3, 2, 1 },
  };

  //ROTATE routes a real input gate, so its choke must travel with the source
  //gate. Otherwise a silent destination remains muted even when a live source
  //is routed to it. Keep routed output chokes separate: feeding them back into
  //trigNChoke would permute the source-latch state again on every loop pass.
  bool inputs[4] = { trig1In, trig2In, trig3In, trig4In };
  bool chokes[4] = { trig1Choke, trig2Choke, trig3Choke, trig4Choke };

  if ((modifyLength1 > 0) && (mode1 == 4)) {
    unsigned int source = rotateSource[0][rotation1] - 1;
    trig1State = inputs[source];
    rotate1Choke = chokes[source];
  }

  if ((modifyLength2 > 0) && (mode2 == 4)) {
    unsigned int source = rotateSource[1][rotation2] - 1;
    trig2State = inputs[source];
    rotate2Choke = chokes[source];
  }

  if ((modifyLength3 > 0) && (mode3 == 4)) {
    unsigned int source = rotateSource[2][rotation3] - 1;
    trig3State = inputs[source];
    rotate3Choke = chokes[source];
  }

  if ((modifyLength4 > 0) && (mode4 == 4)) {
    unsigned int source = rotateSource[3][rotation4] - 1;
    trig4State = inputs[source];
    rotate4Choke = chokes[source];
  }
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
