//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
// Optional ADC diagnostic firmware.  Build with IDUM_ADC_DIAGNOSTIC=1 to print
// the physical analog input ranges over USB serial instead of running IDUM.
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

#if IDUM_ADC_DIAGNOSTIC

unsigned int adcDiagMinMode = 1024;
unsigned int adcDiagMinProb = 1024;
unsigned int adcDiagMinLength = 1024;
unsigned int adcDiagMinParam = 1024;
unsigned int adcDiagMaxMode = 0;
unsigned int adcDiagMaxProb = 0;
unsigned int adcDiagMaxLength = 0;
unsigned int adcDiagMaxParam = 0;
unsigned long adcDiagLastPrint = 0;

void updateAdcRange(unsigned int value, unsigned int &minimum, unsigned int &maximum) {
  if (value < minimum) minimum = value;
  if (value > maximum) maximum = value;
}

void printAdcValue(const char *label, unsigned int value, unsigned int minimum, unsigned int maximum) {
  Serial.print(label);
  Serial.print('=');
  Serial.print(value);
  Serial.print(" min=");
  Serial.print(minimum);
  Serial.print(" max=");
  Serial.print(maximum);
  Serial.print("  ");
}

void adcDiagnosticLoop() {
  unsigned long currentMicros = micros();
  if ((currentMicros - adcDiagLastPrint) < 100000) return;
  adcDiagLastPrint = currentMicros;

  unsigned int modeValue = 1024 - analogRead(modePin);
  unsigned int probValue = 1024 - analogRead(probPin);
  unsigned int lengthValue = 1024 - analogRead(lengPin);
  unsigned int paramValue = 1024 - analogRead(paramPin);

  updateAdcRange(modeValue, adcDiagMinMode, adcDiagMaxMode);
  updateAdcRange(probValue, adcDiagMinProb, adcDiagMaxProb);
  updateAdcRange(lengthValue, adcDiagMinLength, adcDiagMaxLength);
  updateAdcRange(paramValue, adcDiagMinParam, adcDiagMaxParam);

  Serial.print("adc ");
  printAdcValue("mode", modeValue, adcDiagMinMode, adcDiagMaxMode);
  printAdcValue("chance", probValue, adcDiagMinProb, adcDiagMaxProb);
  printAdcValue("length", lengthValue, adcDiagMinLength, adcDiagMaxLength);
  printAdcValue("param", paramValue, adcDiagMinParam, adcDiagMaxParam);
  Serial.println();
}

#endif
