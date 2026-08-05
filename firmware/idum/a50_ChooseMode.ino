//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//     Choose mode is the function that remembers which modes have been deleted or re-added.  Then it will take the ADC input responsible for
//     the "Mode" parameter (knob and CV added together) and decide which mode we are currently in.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

unsigned int chooseMode() {

  unsigned int currentMode = 0;

  checkCurrentModes();  //determine which modes are currently active

  calculateModeBoundaries();  //calculate the boundaries between the currently active modes

  currentMode = selectCurrentMode();  //take the mode CV input and map it onto the calculated boundaries to decide the current mode.

  return currentMode;
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

void checkCurrentModes() {

  byte modeByte = 0;

  //this function tells us the unscaled position of the mode knob.  The ADC input is not super linear so boundaries are set manual in an
  //array called "defaultModeBoundaries [x]" where 'x' is the transition point between two modes.

  //this for loop checks whether the input ADC value is between the default boundaries until it gets a hit.  It uses the old value of the output
  //of this function to add some hysteresis/ "stickiness" to the mode selection so that the mode will not jump between two different values when
  //the knob is close to a boundary

  for (int i = 0; i < 8; i++) {

    //if the mode ADC value is larger than the default boundary plus some extra terms that slightly shift the boundary if 'i' is close to the previous mode value.
    if ((modeRead > (defaultModeBoundaries[i] + ((modeDialPosition == (i - 1)) * 20) + ((modeDialPosition == i) * -20)))

        //AND if the mode ADC value is less than or equal to the default boundary plus some extra terms that slightly shift the boundary if 'i' is close to the previous mode value.
        && (modeRead <= (defaultModeBoundaries[(i + 1)] + ((modeDialPosition == i) * 20) + ((modeDialPosition == (i + 1) * -20))))) {

      //then we know that the current value of 'i' describes which mode the knob is pointing to.  Save the value and leave the for loop.
      modeDialPosition = i;
      break;
    }
  }

  //If the mode button has just been released and we aren't in the menu mode then we flip whether the current mode is active.
  if (modeBtnFallingEdge && !menuMode && !menuModeReset && !loopEnable && !loopModulationMode) {
    activeModes[modeDialPosition] = !activeModes[modeDialPosition];

    //next we check to make sure that there is at least one mode active or else the dang thing crashes.
    int modeArraySum = 0;  //variable for a test that makes sure there is always at least one active mode
    for (int i = 0; i < 8; i++) {
      modeArraySum += (int)activeModes[i];
    }

    if (modeArraySum == 0) activeModes[modePosition] = 1;  //prevents all modes from being turned off which causes an infinite loop

    //we write the status of all active modes whenever we hit the mode button
    modeByte = getModeByte();
    EEPROM.write(2, modeByte);
  }
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//This function calculates the value of dividing lines between modes when some modes have been removed, changing those boundaries.

unsigned int calculateCustomBoundary(unsigned int leftModeIndex, unsigned int rightModeIndex) {
  leftModeIndex *= 2;
  rightModeIndex *= 2;
  unsigned int middle = (leftModeIndex + rightModeIndex) / 2;
  unsigned int value = (middle < boundariesSize) ? possibleModeBoundaries[middle] : 0;
  return value;
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\


//This function creates a set of intervals based on the active modes.

void calculateModeBoundaries() {
  intervalCount = 0;

  // First pass: clear the live interval array. initializeIntervals() operates
  // on an older parallel array that selectCurrentMode() does not read.
  for (int i = 0; i < 8; ++i) {
    intervals[i].left = 0;
    intervals[i].right = 0;
    intervals[i].mode = 0;
  }

  // Second pass: for each active mode add it to the next available interval:
  for (int i = 0; i < 8; ++i) {
    if (activeModes[i]) {
      intervals[intervalCount++].mode = i;
    }
  }

  if (intervalCount == 0) {
    activeModes[modePosition % 8] = 1;
    intervals[intervalCount++].mode = modePosition % 8;
    EEPROM.write(2, getModeByte());
  }

  // Third pass: Loop through each interval and find boundaries from custom table:
  for (int i = 0; i < intervalCount; ++i) {
    if (i == 0) {
      // The first interval's right side is always the lowest boundary.
      intervals[i].left = 0;
    } else {
      // The left side of this interval needs to be halfway to its neighboring left interval's right side.
      intervals[i].left = CalculateCustomBoundary(intervals[i - 1].mode, intervals[i].mode);
    }

    if (i == intervalCount - 1) {
      // The last interval's right side is always the highest boundary.
      intervals[i].right = possibleModeBoundaries[boundariesSize - 1];
    } else {
      // The right side of this interval needs to be halfway to its neighboring right interval's left side.
      intervals[i].right = CalculateCustomBoundary(intervals[i].mode, intervals[i + 1].mode);
    }
  }
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//This determines which mode we are currently in based on the modified mode boundaries.

unsigned int selectCurrentMode() {

  unsigned int bottomBound = 0;
  unsigned int topBound = 0;

  // Loop through the intervals and find the interval that bounds the knob position
  for (int i = 0; i < intervalCount; ++i) {

    //modePosition stores the actual mode ID, while i is only the packed index
    //of an enabled-mode interval. Compare against the interval mode IDs so
    //hysteresis still works when intermediate modes have been removed.
    bool selectedThisInterval = modePosition == intervals[i].mode;
    bool selectedLeftInterval = (i > 0) && (modePosition == intervals[i - 1].mode);
    bool selectedRightInterval = (i + 1 < intervalCount) && (modePosition == intervals[i + 1].mode);

    //Expand the selected interval by 20 ADC counts and contract its immediate
    //neighbors by the same amount, preventing overlap from changing modes.
    long bottomCalc = (long)intervals[i].left
                      + (selectedLeftInterval ? 20 : 0)
                      - (selectedThisInterval ? 20 : 0);

    //does the same test but for the closest right boundary.
    long topCalc = (long)intervals[i].right
                   + (selectedThisInterval ? 20 : 0)
                   - (selectedRightInterval ? 20 : 0);

    //The -20 hysteresis nudge can cross zero at the first interval. Clamp in
    //signed arithmetic instead of wrapping an unsigned bound near 65535.
    bottomBound = (bottomCalc < 0) ? 0 : bottomCalc;
    topBound = (topCalc < 0) ? 0 : topCalc;

    //If our raw ADC input for the Mode knob is between both boundaries we save our current index as the selected mode and exit the loop.
    if ((bottomBound <= modeRead) && (modeRead < topBound)) {
      if (loopModulationMode) break;
      modePosition = intervals[i].mode;
      break;
    }
  }

  //we don't always update the actual mode with the dial position even though we do the calculation for every loop.  This is because the mode doesn't change
  //during a modification but we still want to calculated the mode to remember the most recent value for the hysteresis calculations.

  if ((modificationActive == 0) && !menuMode && !loopModulationMode) {  //if no modification is being done update the selected mode and mode display

    mode = modePosition;
  }

  return modePosition;
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
