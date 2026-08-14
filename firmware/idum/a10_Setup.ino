//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// This function runs once at startup and then the top level of code transitions to the "Loop" function in the next page
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//The panel ADCs are not floating, but their least-significant bits and the
//free-running timer still provide a better startup seed than Arduino's fixed
//default sequence. The first external clock supplies the stronger timing seed.
unsigned long gatherEntropy() {
  unsigned long entropy = micros();
  const uint8_t entropyPins[4] = { modePin, probPin, lengPin, paramPin };

  for (uint8_t i = 0; i < 16; ++i) {
    entropy = (entropy << 1) | (analogRead(entropyPins[i & 3]) & 1);
    entropy ^= micros();
  }

  return entropy;
}

void seedRNGOnFirstClock() {
  if (!rngSeeded && clockEdge) {
    randomSeed(micros() ^ ((unsigned long)analogRead(paramPin) << 6) ^ analogRead(probPin));
    rngSeeded = 1;
  }
}

void setup() {

  assignPins();  //Set input and output status of pins.

  randomSeed(gatherEntropy());

  readStartupButtonStates();

#if IDUM_ADC_DIAGNOSTIC
  Serial.begin(115200);
#endif

  if (printEnable) Serial.begin(9600);  //only setup the serial communication if printing is enabled.  Causes the MCU to work way slower.

  initializeEEPROM();  //check and read saved variables from before the last power down.

  initializeIntervals();  //Sets up some variables used in the "Boundary Utils" page of functions.

  initializeHardwareInterrupts();

  startupAnimation();

  displayFirmwareVersion(7);  //This displays on the mode ring which firmware is loaded on your IDUM.  Increase the number for each update.

  delay(1000);  //Delay the start of the program so that we can display the firmware number for a little bit longer.

  dTimer.every(1, displayTimer);  //Start up the display counter, runs every '1' microsecond.
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Sets the input or output status of all of the Arduino Pins.  Pin numbers are refered to by names assigned in the "variables" page.

void assignPins() {

  pinMode(loopLED, OUTPUT);
  pinMode(loopButton, INPUT);

  pinMode(mode1LED, OUTPUT);
  pinMode(mode2LED, OUTPUT);
  pinMode(mode3LED, OUTPUT);

  pinMode(clockPin, INPUT);
  pinMode(trig1Pin, INPUT);
  pinMode(trig2Pin, INPUT);
  pinMode(trig3Pin, INPUT);
  pinMode(trig4Pin, INPUT);

  pinMode(clockOut, OUTPUT);
  pinMode(trig1Out, OUTPUT);
  pinMode(trig2Out, OUTPUT);
  pinMode(trig3Out, OUTPUT);
  pinMode(trig4Out, OUTPUT);

  pinMode(loopGate, INPUT);
  pinMode(cycleSw, INPUT);
  pinMode(modeBtn, INPUT_PULLUP);

  pinMode(loopLED, OUTPUT);

  pinMode(modePin, INPUT);
  pinMode(probPin, INPUT);
  pinMode(lengPin, INPUT);
  pinMode(paramPin, INPUT);
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Set the display multiplexer bits.  LEDs are not in ascending order on the PCB,
//so display positions are translated through modeLedAddressMap.

void writeModeMuxAddress(unsigned int muxAddress) {

  switch (muxAddress) {

    case 0:
      digitalWriteFast(mode1LED, HIGH);
      digitalWriteFast(mode2LED, LOW);
      digitalWriteFast(mode3LED, LOW);
      break;
    case 1:
      digitalWriteFast(mode1LED, LOW);
      digitalWriteFast(mode2LED, HIGH);
      digitalWriteFast(mode3LED, LOW);
      break;
    case 2:
      digitalWriteFast(mode1LED, LOW);
      digitalWriteFast(mode2LED, LOW);
      digitalWriteFast(mode3LED, HIGH);
      break;
    case 3:
      digitalWriteFast(mode1LED, LOW);
      digitalWriteFast(mode2LED, HIGH);
      digitalWriteFast(mode3LED, HIGH);
      break;
    case 4:
      digitalWriteFast(mode1LED, HIGH);
      digitalWriteFast(mode2LED, HIGH);
      digitalWriteFast(mode3LED, HIGH);
      break;
    case 5:
      digitalWriteFast(mode1LED, HIGH);
      digitalWriteFast(mode2LED, LOW);
      digitalWriteFast(mode3LED, HIGH);
      break;
    case 6:
      digitalWriteFast(mode1LED, HIGH);
      digitalWriteFast(mode2LED, HIGH);
      digitalWriteFast(mode3LED, LOW);
      break;
    case 7:
      digitalWriteFast(mode1LED, LOW);
      digitalWriteFast(mode2LED, LOW);
      digitalWriteFast(mode3LED, LOW);
      break;
    default: break;
  }
}

void writeModeLed(unsigned int ledIndex) {
  if (ledIndex > 7) ledIndex = 7;
  writeModeMuxAddress(LEDTweak ? modeLedAddressMap[ledIndex] : ledIndex);
}

void displayFirmwareVersion(unsigned int firmwareNumber) {
  if (firmwareNumber == 0) writeModeLed(0);
  else writeModeLed(firmwareNumber - 1);
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Reads the options, active modes, and any other variables that have been saved from before the most recent power down.

void readStartupButtonStates() {
  factoryResetHeld = !digitalReadFast(modeBtn);
  if (factoryResetHeld) delay(20);
  factoryResetHeld = factoryResetHeld && !digitalReadFast(modeBtn);

  changeLEDOrder = !digitalReadFast(loopButton);
  if (changeLEDOrder) delay(20);
  changeLEDOrder = changeLEDOrder && !digitalReadFast(loopButton);
}

void writeEEPROMDefaults() {
  EEPROM.write(0, 'R');
  EEPROM.write(1, 0);
  EEPROM.write(2, 255);
  EEPROM.write(3, 0);
  EEPROM.write(4, 0);
}

void initializeEEPROM() {

  byte modeByte = 0;    //This is used to save all of the active modes into one byte instead of as separate variables.
  byte optionByte = 0;  //This is used to save all of the active options into one byte instead of as separate variables.

  char eepromInitialized = EEPROM.read(0);  //We save a letter into the first memory location on the EEPROM to show that it has been initialiazed.

  if (eepromInitialized == 'R') {  //R adds the independently stored LED-order byte.
    LEDTweak = EEPROM.read(5) != 0;

    if (factoryResetHeld) {
      writeEEPROMDefaults();
      startupAnimationNumber = 2;
    }

    else {
      optionByte = EEPROM.read(1);
      modeByte = EEPROM.read(2);
      paramResolution = EEPROM.read(3);
      lengthResolution = EEPROM.read(4);

      setModeArray(modeByte);  //Take each bit of the mode byte and spit it into separate locations in the mode Array.
      setOptions(optionByte);  //Take each bit of the option byte and split it into separate variables.

      if (printEnable) Serial.println("test passed");  //Lets us know that the EEPROM had been saved to before in the appropriate format.
    }
  }

  else {                                                                    //If our test of the EEPROM having the right letter saved fails we write some default values in.
    writeEEPROMDefaults();
    LEDTweak = LEDTweakSetEEPROM;
    EEPROM.write(5, LEDTweak);
    startupAnimationNumber = 1;
    if (printEnable) Serial.println("Test failed.  EEPROM Reinitialized");  //Lets us know that the EEPROM had to be reset.
  }

  if (changeLEDOrder) {
    LEDTweak = !LEDTweak;
    EEPROM.write(5, LEDTweak);
    startupAnimationNumber = 3;
  }
}

void startupAnimation() {
  if (startupAnimationNumber == 1) {
    for (uint8_t i = 0; i < 5; ++i) {
      digitalWriteFast(loopLED, HIGH);
      delay(100);
      digitalWriteFast(loopLED, LOW);
      delay(100);
    }
  }

  else if (startupAnimationNumber == 2) {
    unsigned int led = 0;
    digitalWriteFast(loopLED, HIGH);
    while (!digitalReadFast(modeBtn)) {
      writeModeLed(led++ % 8);
      delay(50);
    }
    digitalWriteFast(loopLED, LOW);
  }

  else if (startupAnimationNumber == 3) {
    int led = 7;
    digitalWriteFast(loopLED, HIGH);
    while (!digitalReadFast(loopButton)) {
      writeModeLed(led);
      led = (led + 7) % 8;
      delay(50);
    }
    digitalWriteFast(loopLED, LOW);
  }
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

void initializeHardwareInterrupts() {

  attachInterrupt(trig1Pin, trig1InterruptHandler, RISING);
  attachInterrupt(trig2Pin, trig2InterruptHandler, RISING);
  attachInterrupt(trig3Pin, trig3InterruptHandler, RISING);
  attachInterrupt(trig4Pin, trig4InterruptHandler, RISING);
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
