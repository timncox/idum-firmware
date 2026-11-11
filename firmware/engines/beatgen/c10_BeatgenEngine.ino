//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Beatgen: a clock-driven drum pattern generator with an audio-rate mode.
//
// Patch a clock into CLOCK IN and nothing else: every clock edge plays one
// step on all four trigger outputs. The MODE knob picks one of eight slots:
//
//   0 House   1 Techno   2 Breaks   3 Hip-hop      (pattern bank)
//   4 Euclid  5 Generative                          (algorithmic)
//   6 User    (pattern bank, made for editing on the web page)
//   7 VCO     (the four outputs become audio-rate square waves)
//
// PROB = density (how many of a pattern's optional hits play), LENGTH = loop
// length in steps, PARAM = variation (fills, ghost notes, ratchets). LOOP
// locks the current variation so it repeats exactly. In VCO mode PARAM is
// pitch, LENGTH picks a chord across the four outputs and PROB detunes them.
//
// The five bank patterns live in flash as plain text behind the "IDUMBEAT1"
// marker so web/beatgen.html can find them in a built .hex, edit them and
// flash the result. Five characters are used per step: '.' rest, then
// '1' '2' '3' 'X' from least to most likely to play.
//
// Part of a CC BY-SA 4.0 derivative of IDUM by Eli Pechman / Mystic Circuits,
// built on the Alt-DUM firmware (github.com/ctanis/idum-firmware).
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\

#if IDUM_ALT_ENGINE == IDUM_ALT_ENGINE_BEATGEN

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(address) (*(const uint8_t*)(address))
#endif
#ifndef pgm_read_word
#define pgm_read_word(address) (*(const uint16_t*)(address))
#endif

const uint8_t BEATGEN_STEPS = 32;
const uint8_t BEATGEN_BANK_PATTERNS = 5;
const uint8_t BEATGEN_HEADER = 9;  //strlen("IDUMBEAT1")
const uint8_t BEATGEN_SLOT_EUCLID = 4;
const uint8_t BEATGEN_SLOT_GENERATIVE = 5;
const uint8_t BEATGEN_SLOT_USER = 6;
const uint8_t BEATGEN_SLOT_VCO = 7;

//Rows are OUT1-OUT4 of one pattern; think kick, snare/clap, closed hat, perc.
//Keep every row exactly 32 characters: the web programmer relies on it.
const char beatgenBank[] PROGMEM =
  "IDUMBEAT1"
  //0 House
  "X...X...X...X...X...X...X...X.1."
  "....X.......X.......X.......X..2"
  "21312131213121312131213121312131"
  "..X...X...X...X...X...X...X...X."
  //1 Techno
  "X..1X..1X..1X..1X..1X..1X..1X.21"
  "....3.......X.......3.......X..."
  "1.X.1.X.1.X.1.X.1.X.1.X.1.X.1.X."
  "...2..3.....2.1....2..3....2.3.1"
  //2 Breaks
  "X.X.......XX....X.X.......2X...."
  "....X..2.2..X..2....X..2.2..X.2."
  "X.3.X.3.X.3.X.3.X.3.X.3.X.3.X.3."
  "X.........1.....2.........1....."
  //3 Hip-hop
  "X......X..X.....X......X.2X..2.."
  "....X.......X.......X.......X..1"
  "X.2.X.2.X.2.X.2.X.2.X.2.X.2.X.2."
  ".............................X.."
  //6 User (edit this one on the web page)
  "X.......X.X.....X.......X.X....."
  "....X.......X.......X.......X..."
  "X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X."
  "..............................X.";

const uint8_t beatgenLengths[8] = { 4, 6, 8, 10, 12, 16, 24, 32 };

uint8_t beatgenSlot = 0;
uint8_t beatgenStep = 0;
bool beatgenStarted = 0;
bool beatgenResetPending = 0;
bool beatgenLocked = 0;
bool beatgenLoopButtonOld = 0;
bool beatgenModeButtonOld = 0;
uint16_t beatgenLoopSeed = 1;
uint16_t beatgenGenSeed = 1;
bool beatgenFillThisLoop = 0;

uint8_t beatgenOnMask = 0;            //bit 0-3 trigger outs, bit 4 clock out
unsigned long beatgenOnAt[5] = {};
unsigned long beatgenWidth = 10000;
uint8_t beatgenRatchetMask = 0;
unsigned long beatgenRatchetAt = 0;
unsigned long beatgenLastStepMicros = 0;
bool beatgenAnyStep = 0;

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Small deterministic hash so a locked loop replays the same variation.

uint16_t beatgenHash(uint16_t seed, uint8_t step, uint8_t channel) {
  uint16_t h = seed ^ (uint16_t)(step * 0x9E37u) ^ (uint16_t)(channel * 0x7F4Bu);
  h ^= h << 7;
  h ^= h >> 9;
  h ^= h << 8;
  h ^= h >> 5;
  return h;
}

uint16_t beatgenRandom16() {
  return (uint16_t)random(0x10000L);
}

uint8_t beatgenLevelWeight(char level) {
  switch (level) {
    case '1': return 64;
    case '2': return 128;
    case '3': return 192;
    case 'X': return 255;
    default: return 0;
  }
}

uint8_t beatgenBankWeight(uint8_t pattern, uint8_t channel, uint8_t step) {
  uint16_t index = BEATGEN_HEADER
                 + ((uint16_t)(pattern * 4 + channel) * BEATGEN_STEPS) + step;
  return beatgenLevelWeight((char)pgm_read_byte(&beatgenBank[index]));
}

uint8_t beatgenTrailingZeros(uint8_t step) {
  if (step == 0) return 5;
  uint8_t count = 0;
  while (!(step & 1)) {
    step >>= 1;
    count++;
  }
  return count;
}

//Weights for the generative slot: random, but leaning on where each voice
//usually sits in a bar so the result still grooves.
uint8_t beatgenGenerativeWeight(uint8_t channel, uint8_t step) {
  uint8_t r = beatgenHash(beatgenGenSeed, step, channel) & 0xff;
  uint8_t strength = beatgenTrailingZeros(step);  //0 offbeat 16th .. 5 downbeat
  switch (channel) {
    case 0:
      if ((step & 7) == 0) return 160 + (r >> 2);
      return (uint16_t)r * strength / 6;
    case 1:
      if ((step & 7) == 4) return 128 + (r >> 1);
      return r >> 2;
    case 2:
      return (step & 1) ? (r >> 1) : 64 + (r >> 1);
    default:
      return (step & 1) ? (r >> 1) + 32 : (r >> 2);
  }
}

uint8_t beatgenWeight(uint8_t channel, uint8_t step) {
  if (beatgenSlot == BEATGEN_SLOT_GENERATIVE) return beatgenGenerativeWeight(channel, step);
  uint8_t pattern = (beatgenSlot == BEATGEN_SLOT_USER) ? 4 : beatgenSlot;
  return beatgenBankWeight(pattern, channel, step);
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Controls

void beatgenReadInputs() {
  readGateInputs();
  readAnalogInputs();
  scaleCVInputs();
  cycleSwitchIn = cycleIn;
}

void beatgenEdges() {
  detectEdges();
  calculateGateVariables();
  saveGateStates();
}

void beatgenChooseSlot() {
  int reading = modeRead;
  for (int i = 0; i < 8; i++) {
    int low = (int)defaultModeBoundaries[i] + (beatgenSlot == i - 1 ? 20 : 0) - (beatgenSlot == i ? 20 : 0);
    int high = (int)defaultModeBoundaries[i + 1] + (beatgenSlot == i ? 20 : 0) - (beatgenSlot == i + 1 ? 20 : 0);
    if (i == 0) low = -1;
    if (i == 7) high = 2000;
    if (reading > low && reading <= high) {
      beatgenSlot = i;
      return;
    }
  }
}

uint8_t beatgenLength() {
  return beatgenLengths[lengthPosition & 7];
}

uint8_t beatgenDensity() {
  return probRead >= 1020 ? 255 : (uint8_t)(probRead >> 2);
}

uint8_t beatgenVariation() {
  return paramRead >= 1020 ? 255 : (uint8_t)(paramRead >> 2);
}

void beatgenHandleButtons() {
  //LOOP button toggles the lock; a high LOOP gate also locks while held.
  if (loopButtonIn && !beatgenLoopButtonOld) beatgenLocked = !beatgenLocked;
  beatgenLoopButtonOld = loopButtonIn;
  loopEnable = beatgenLocked || loopGateIn;  //drives the LOOP LED

  //MODE button restarts the pattern on the next clock.
  if (modeBtnIn && !beatgenModeButtonOld) beatgenResetPending = 1;
  beatgenModeButtonOld = modeBtnIn;

  if (trig1Edge) beatgenResetPending = 1;  //TR1 in: reset
  if (trig3Edge) {                         //TR3 in: new variation now
    beatgenLoopSeed = beatgenRandom16();
    beatgenGenSeed = beatgenRandom16();
  }
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Pattern logic. Returns trigger bits for this step; ratchets go in *ratchet.

void beatgenStartLoop() {
  if (loopEnable) return;  //locked: replay the same seeds
  beatgenLoopSeed = beatgenRandom16();
  uint8_t variation = beatgenVariation();
  if (beatgenSlot == BEATGEN_SLOT_GENERATIVE) {
    //PARAM is the chance each loop is re-rolled; fully CCW keeps it forever.
    if ((beatgenRandom16() & 0xff) < variation) beatgenGenSeed = beatgenRandom16();
  }
  beatgenFillThisLoop = (beatgenSlot < BEATGEN_SLOT_EUCLID || beatgenSlot == BEATGEN_SLOT_USER)
                     && ((beatgenLoopSeed >> 8) < (variation >> 1));
}

uint8_t beatgenEuclidMask(uint8_t step, uint8_t length) {
  static const uint8_t base[4] = { 4, 2, 8, 1 };    //sixteenths of the loop at PROB CCW
  static const uint8_t extra[4] = { 4, 4, 8, 6 };   //added at PROB CW
  uint8_t mask = 0;
  uint8_t rotation = unscaledParam;
  for (uint8_t c = 0; c < 4; c++) {
    uint16_t pulses = ((uint32_t)length * (base[c] * 1024UL + extra[c] * (uint32_t)probRead)) / 16384UL;
    if (c == 0 && pulses < 1) pulses = 1;
    if (pulses > length) pulses = length;
    uint8_t position = (step + rotation * c) % length;
    if ((uint16_t)(position * pulses) % length < pulses) mask |= 1 << c;
  }
  return mask;
}

uint8_t beatgenStepMask(uint8_t step, uint8_t length, uint8_t* ratchet) {
  *ratchet = 0;
  if (beatgenSlot == BEATGEN_SLOT_EUCLID) {
    uint8_t mask = beatgenEuclidMask(step, length);
    if (trig2In) {  //fill: roll the second voice
      mask |= 2;
      *ratchet = 2;
    }
    if (trig4In) mask &= 1;
    return mask;
  }

  uint8_t density = beatgenDensity();
  uint8_t variation = (beatgenSlot == BEATGEN_SLOT_GENERATIVE) ? 0 : beatgenVariation();
  uint8_t threshold = 255 - density;
  uint8_t mask = 0;

  for (uint8_t c = 0; c < 4; c++) {
    uint8_t weight = beatgenWeight(c, step);
    if (weight == 0) continue;
    uint16_t h = beatgenHash(beatgenLoopSeed, step, c);
    int16_t effective = weight;
    if (variation) effective += (int16_t)(((h & 0xff) * (uint16_t)variation) >> 8) - (variation >> 1);
    if (weight == 255) effective = 255;  //core hits always play
    if (effective >= threshold) mask |= 1 << c;

    //Ratchet a hat now and then once PARAM passes about two thirds.
    if (c == 2 && (mask & 4) && variation > 160 && ((h >> 8) & 0xff) < (uint8_t)((variation - 160) * 2)) {
      *ratchet |= 4;
    }
  }

  bool inFillZone = step >= length - (length >> 2);
  if (trig2In || (beatgenFillThisLoop && inFillZone)) {
    uint16_t h = beatgenHash(beatgenLoopSeed, step, 7);
    if ((h & 3) != 0 || trig2In) mask |= 2;
    if (step == length - 1 || trig2In) *ratchet |= 2;
  }

  if (trig4In) {  //TR4 in held: breakdown, first voice only
    mask &= 1;
    *ratchet &= 1;
  }
  return mask;
}

void beatgenFire(uint8_t mask, unsigned long now) {
  for (uint8_t c = 0; c < 5; c++) {
    if (mask & (1 << c)) {
      beatgenOnMask |= 1 << c;
      beatgenOnAt[c] = now;
    }
  }
}

void beatgenAdvance(unsigned long now) {
  uint8_t length = beatgenLength();
  if (!beatgenStarted || beatgenResetPending) {
    beatgenStep = 0;
    beatgenStarted = 1;
    beatgenResetPending = 0;
  } else {
    beatgenStep++;
    if (beatgenStep >= length) beatgenStep = 0;
  }
  if (beatgenStep >= length) beatgenStep = 0;
  if (beatgenStep == 0) beatgenStartLoop();

  //Trigger width: 10 ms, or a quarter of the clock period if that is shorter.
  unsigned long interval = lastClockInterval ? lastClockInterval : 250000UL;
  beatgenWidth = interval / 4;
  if (beatgenWidth > 10000UL) beatgenWidth = 10000UL;
  if (beatgenWidth < 200UL) beatgenWidth = 200UL;

  uint8_t ratchet = 0;
  uint8_t mask = beatgenStepMask(beatgenStep, length, &ratchet);
  if (beatgenStep == 0) mask |= 0x10;  //CLOCK OUT marks the top of the loop

  beatgenFire(mask, now);
  beatgenRatchetMask = ratchet & mask;
  beatgenRatchetAt = now + interval / 2;
  beatgenLastStepMicros = now;
  beatgenAnyStep = 1;
}

void beatgenWriteOutputs(unsigned long now) {
  if (beatgenRatchetMask && (long)(now - beatgenRatchetAt) >= 0) {
    beatgenFire(beatgenRatchetMask, now);
    beatgenRatchetMask = 0;
  }
  for (uint8_t c = 0; c < 5; c++) {
    if ((beatgenOnMask & (1 << c)) && (now - beatgenOnAt[c]) >= beatgenWidth) {
      beatgenOnMask &= ~(1 << c);
    }
  }
  digitalWrite(clockOut, (beatgenOnMask & 0x10) ? LOW : HIGH);
  digitalWriteFast(trig1Out, (beatgenOnMask & 1) ? LOW : HIGH);
  digitalWriteFast(trig2Out, (beatgenOnMask & 2) ? LOW : HIGH);
  digitalWriteFast(trig3Out, (beatgenOnMask & 4) ? LOW : HIGH);
  digitalWriteFast(trig4Out, (beatgenOnMask & 8) ? LOW : HIGH);
}

void beatgenAllOff() {
  beatgenOnMask = 0;
  beatgenRatchetMask = 0;
  digitalWrite(clockOut, HIGH);
  digitalWriteFast(trig1Out, HIGH);
  digitalWriteFast(trig2Out, HIGH);
  digitalWriteFast(trig3Out, HIGH);
  digitalWriteFast(trig4Out, HIGH);
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// VCO mode. OUT4 (PB2) is TCA0's WO2 pin, so it gets a clean hardware square.
// OUT1-OUT3 (PA1, PE3, PF4) have no usable timer output, so a 31.25 kHz
// interrupt on TCB2 runs three phase accumulators (rougher at high pitch).
// Pitch is uncalibrated: PARAM knob + CV sweeps about 16 Hz to 4 kHz.

const uint16_t BEATGEN_DDS_RATE = 31250;
const uint16_t beatgenExp2Q16[32] PROGMEM = {
  0, 1435, 2902, 4400, 5932, 7496, 9096, 10730, 12400, 14106, 15850,
  17633, 19454, 21315, 23216, 25160, 27146, 29175, 31249, 33369, 35534, 37747,
  40009, 42320, 44682, 47095, 49562, 52082, 54658, 57289, 59979, 62727
};
//Semitone offsets for OUT1-OUT3 against the OUT4 root, picked by LENGTH.
const int8_t beatgenChords[8][3] = {
  { 0, 0, 0 },    //unison (use PROB to detune)
  { 12, 24, -12 },//octaves
  { 7, 12, 19 },  //fifths
  { 4, 7, 12 },   //major
  { 3, 7, 12 },   //minor
  { 3, 7, 10 },   //minor 7
  { 4, 7, 11 },   //major 7
  { 5, 10, 15 },  //fourths
};

bool beatgenAudioOn = 0;
unsigned long beatgenAudioUpdated = 0;
uint32_t beatgenFreqQ8[4] = {};  //last computed pitch per output, Hz * 256
volatile uint16_t beatgenPhase[3] = {};
volatile uint16_t beatgenIncrement[3] = {};

//Pitch in 1/1024-octave units above 16 Hz -> Hz * 256.
uint32_t beatgenPitchToFreqQ8(int32_t units) {
  if (units < 0) units = 0;
  if (units > 9 * 1024L) units = 9 * 1024L;
  uint8_t octave = units >> 10;
  uint16_t fraction = units & 1023;
  uint8_t k = fraction >> 5;
  uint8_t rem = fraction & 31;
  uint32_t a = pgm_read_word(&beatgenExp2Q16[k]);
  uint32_t b = (k == 31) ? 65536UL : pgm_read_word(&beatgenExp2Q16[k + 1]);
  uint32_t mantissa = 65536UL + a + (((b - a) * rem) >> 5);
  return ((4096UL * mantissa) >> 16) << octave;  //16 Hz * 256 = 4096
}

void beatgenComputeFrequencies() {
  int32_t root = (int32_t)paramRead * 8;  //8 octaves over the PARAM range
  int32_t detune = ((int32_t)probRead * 48) >> 10;
  const int8_t* chord = beatgenChords[lengthPosition & 7];
  beatgenFreqQ8[3] = beatgenPitchToFreqQ8(root);
  for (uint8_t i = 0; i < 3; i++) {
    int32_t offset = ((int32_t)chord[i] * 1024) / 12;
    int32_t spread = (i == 1) ? -detune : (i == 0 ? detune : detune >> 1);
    beatgenFreqQ8[i] = beatgenPitchToFreqQ8(root + offset + spread);
  }
}

#ifndef IDUM_SIM
uint8_t beatgenSavedTcaCtrlA = 0;
uint8_t beatgenSavedTcaCtrlB = 0;
uint16_t beatgenSavedTcaPer = 0;
uint16_t beatgenSavedTcaCmp2 = 0;
uint8_t beatgenSavedTcbCtrlA = 0;
uint8_t beatgenSavedTcbCtrlB = 0;
uint16_t beatgenSavedTcbCcmp = 0;
uint8_t beatgenTcaPrescaler = 0xff;

ISR(TCB2_INT_vect) {
  TCB2.INTFLAGS = TCB_CAPT_bm;
  beatgenPhase[0] += beatgenIncrement[0];
  beatgenPhase[1] += beatgenIncrement[1];
  beatgenPhase[2] += beatgenIncrement[2];
  if (beatgenPhase[0] & 0x8000) VPORTA.OUT |= PIN1_bm; else VPORTA.OUT &= ~PIN1_bm;
  if (beatgenPhase[1] & 0x8000) VPORTE.OUT |= PIN3_bm; else VPORTE.OUT &= ~PIN3_bm;
  if (beatgenPhase[2] & 0x8000) VPORTF.OUT |= PIN4_bm; else VPORTF.OUT &= ~PIN4_bm;
}
#endif

void beatgenApplyFrequencies() {
  uint16_t increments[3];
  for (uint8_t i = 0; i < 3; i++) {
    uint32_t f = beatgenFreqQ8[i];
    if (f > 7000UL * 256) f = 7000UL * 256;  //keep the soft oscillators below ~Nyquist/2
    increments[i] = (uint16_t)((f * 256UL) / BEATGEN_DDS_RATE);
  }
#ifdef IDUM_SIM
  for (uint8_t i = 0; i < 3; i++) beatgenIncrement[i] = increments[i];
#else
  noInterrupts();
  for (uint8_t i = 0; i < 3; i++) beatgenIncrement[i] = increments[i];
  interrupts();

  //OUT4 on TCA0: divide by 1 above ~245 Hz, by 16 below.
  uint32_t f4 = beatgenFreqQ8[3];
  uint8_t prescaler;
  uint32_t period;
  if (f4 >= 245UL * 256) {
    prescaler = TCA_SINGLE_CLKSEL_DIV1_gc;
    period = (F_CPU * 256UL) / f4;
  } else {
    prescaler = TCA_SINGLE_CLKSEL_DIV16_gc;
    period = ((F_CPU / 16) * 256UL) / f4;
  }
  if (period > 65535UL) period = 65535UL;
  if (period < 2) period = 2;
  if (prescaler != beatgenTcaPrescaler) {
    TCA0.SINGLE.CTRLA = prescaler | TCA_SINGLE_ENABLE_bm;
    beatgenTcaPrescaler = prescaler;
  }
  TCA0.SINGLE.PERBUF = period - 1;
  TCA0.SINGLE.CMP2BUF = period / 2;
#endif
}

void beatgenAudioStart() {
  if (beatgenAudioOn) return;
  beatgenAllOff();
  beatgenComputeFrequencies();
#ifndef IDUM_SIM
  beatgenSavedTcaCtrlA = TCA0.SINGLE.CTRLA;
  beatgenSavedTcaCtrlB = TCA0.SINGLE.CTRLB;
  beatgenSavedTcaPer = TCA0.SINGLE.PER;
  beatgenSavedTcaCmp2 = TCA0.SINGLE.CMP2;
  beatgenSavedTcbCtrlA = TCB2.CTRLA;
  beatgenSavedTcbCtrlB = TCB2.CTRLB;
  beatgenSavedTcbCcmp = TCB2.CCMP;
  beatgenTcaPrescaler = 0xff;

  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP2EN_bm;
  TCB2.CTRLA = 0;
  TCB2.CTRLB = TCB_CNTMODE_INT_gc;
  TCB2.CCMP = (F_CPU / BEATGEN_DDS_RATE) - 1;
  TCB2.CNT = 0;
  TCB2.INTFLAGS = TCB_CAPT_bm;
  TCB2.INTCTRL = TCB_CAPT_bm;
  TCB2.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
#endif
  beatgenAudioOn = 1;
  beatgenApplyFrequencies();
  beatgenAudioUpdated = micros();
}

void beatgenAudioStop() {
  if (!beatgenAudioOn) return;
#ifndef IDUM_SIM
  TCB2.INTCTRL = 0;
  TCB2.CTRLA = 0;
  TCB2.CTRLB = beatgenSavedTcbCtrlB;
  TCB2.CCMP = beatgenSavedTcbCcmp;
  TCB2.INTFLAGS = TCB_CAPT_bm;
  TCB2.CTRLA = beatgenSavedTcbCtrlA;

  TCA0.SINGLE.CTRLA = 0;
  TCA0.SINGLE.CTRLB = beatgenSavedTcaCtrlB;
  TCA0.SINGLE.PER = beatgenSavedTcaPer;
  TCA0.SINGLE.CMP2 = beatgenSavedTcaCmp2;
  TCA0.SINGLE.CNT = 0;
  TCA0.SINGLE.CTRLA = beatgenSavedTcaCtrlA;
#endif
  beatgenAudioOn = 0;
  for (uint8_t i = 0; i < 3; i++) beatgenIncrement[i] = 0;
  beatgenAllOff();
}

void beatgenAudioService(unsigned long now) {
  if (now - beatgenAudioUpdated < 2000UL) return;  //retune at most every 2 ms
  beatgenAudioUpdated = now;
  beatgenComputeFrequencies();
  beatgenApplyFrequencies();
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\
// Display: the ring shows the slot; each step briefly shows the step position.

bool beatgenDisplay() {
  if (!alternateEngineActive) return false;
  if (beatgenAnyStep && beatgenSlot != BEATGEN_SLOT_VCO
      && (micros() - beatgenLastStepMicros) < 25000UL) {
    writeModeLed(beatgenStep & 7);
  } else {
    writeModeLed(beatgenSlot);
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\

void runBeatgenEngine() {
  cTimer.tick();
  dTimer.tick();
  beatgenReadInputs();
  beatgenEdges();
  seedRNGOnFirstClock();
  beatgenChooseSlot();
  beatgenHandleButtons();

  unsigned long now = micros();
  if (beatgenSlot == BEATGEN_SLOT_VCO) {
    beatgenAudioStart();
    beatgenAudioService(now);
    return;
  }
  beatgenAudioStop();

  if (clockEdge) beatgenAdvance(now);
  beatgenWriteOutputs(now);
}

void resetBeatgenEngine() {
  beatgenAudioStop();
  loopEnable = 0;
  modifyLength = modifyLength1 = modifyLength2 = modifyLength3 = modifyLength4 = 0;
  modificationActive = 0;
  carryOverActive = 0;
  cycleActive = 0;
  cycleCount = 0;

  beatgenStarted = 0;
  beatgenResetPending = 0;
  beatgenLocked = 0;
  beatgenLoopButtonOld = loopButtonIn;
  beatgenModeButtonOld = modeBtnIn;
  beatgenOnMask = 0;
  beatgenRatchetMask = 0;
  beatgenAnyStep = 0;
  beatgenFillThisLoop = 0;
  beatgenLoopSeed = beatgenRandom16() | 1;
  beatgenGenSeed = beatgenRandom16() | 1;
}

#endif
