//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Extra under the hood options.  You can change these values and modify how your IDUM works easily!  Read the description for each option to
//learn what it does.

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//This controls the speed of the slow clock cycle function.
//Increase the value if your favorite sequencer is not playing nicely with IDUM's clock manipulation.
//Decrease the value if you think that your favorite sequencer might be able to run faster with IDUM's clock manipulation.

unsigned int slowClockSpeed = 2400;

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Normally after a modification is activated IDUM waits for a trigger to come in each channels gate input before adding new gates to that channels
//output.  This makes it so that for example during a burst you won't end up with bursts being added to steps where no triggers are present unless
//you are using a particularly long length setting and even then bursts are only added after incoming triggers and not before.  Turning this option
//on will disable the input trigger checking and make it so that IDUM will add bursts and stuff like that as soon as a modification becomes active.

bool chokeDisable = 0;        //default = 0

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//This makes it so that the param function cant change while a modification is active.
//Turning this off will make it so that param is only sampled once at the beginning of each modification and stays the same over that modification.
//In "split" mode this makes it so that each channel can have a different param value as well as mode value.
//This doesn't affect how param interacts with the loop mode.

bool freezeParam = 0;         //default = 0

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//IDUM will sometimes treat modifications that are triggered one after the other as one long modification.  This works for stuff like calculating
//the individual clock chokes, the step in the preset rhythms in break mode, the start time of various burst modes etc.  I felt like this was 
//useful because sometimes you would trigger a modification at one time during the cycle of your incoming pattern and it would sound different than
//if a modification activated at another time in the pattern.  This option disables the carryover behavior and makes IDUM treat each modification
//as it's own individual modification.

bool carryOverDisable = 0;    //default = 0

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//External sequencer clock manipulation speed.
//This fork assumes attached sequencers can handle IDUM's fast clock manipulation behavior.

const bool fastExternalSequencerClock = 1; //default = 1; rebuild to change

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Generated trigger carryover behavior.
//Burst and Multiply/Divide can carry over from one modification window into the next. Re-arming chokes during carryover keeps a sparse
//trigger from unlocking endless generated pulses when chance is high and length is short.

bool rearmGeneratedTriggerChokesOnCarryOver = 1; //default = 1

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Mode-ring LED order. Hardware v1.3 and later uses the remapped order. Older
//boards can select the original mux order by holding LOOP while powering on.
//The selected order is stored separately in EEPROM so a factory settings reset
//does not erase this hardware-specific choice.

bool LEDTweakSetEEPROM = 1; //default = 1 for hardware v1.3 and later
bool LEDTweak = 1;

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Probability behavior.  Lowering probabilityLowerThreshold lets the bottom of the Chance slider produce tiny amounts of randomness.
//A value of 0 means the fully-down slider is still off, but anything above zero can trigger a very rare modification.
//probabilityLengthPenalty makes long modifications less likely to immediately retrigger.

unsigned int probabilityLowerThreshold = 0;    //default IDUM v0.99 behavior was 15
unsigned int probabilityUpperThreshold = 1000; //default = 1000
unsigned int probabilityLengthPenalty = 90;    //default = 90

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Generated clock/ratchet behavior.
//IDUM cannot know a clock has stopped until the next expected pulse does not arrive.  These values control how long generated-repeat modes
//are allowed to keep running after the most recent clock before they release back to pass-through.

//Set IDUM_ENABLE_CLOCK_STOP_TIMEOUT=0 at build time to restore v0.99's
//unbounded generated-repeat behavior after the external clock stops.
#ifndef IDUM_ENABLE_CLOCK_STOP_TIMEOUT
#define IDUM_ENABLE_CLOCK_STOP_TIMEOUT 1
#endif

unsigned int clockStopTimeoutIntervals = 2;          //default = 2 missed clock intervals
unsigned long clockStopMinimumMicros = 100000;       //default = 100ms, mostly affects very fast clocks

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Generated output pulse widths.

unsigned long generatedTriggerPulseWidth = 10000;    //default = 10ms for burst/ratchet trigger outputs
unsigned long generatedClockPulseWidth = 10000;      //default = 10ms for one-shot clock outputs at modification start
unsigned long breakTriggerPulseWidth = 5000;         //default = 5ms for Break mode trigger cells
unsigned long loopClockPulseWidth = 5000;            //default = 5ms for loop/external sequencer clocking
unsigned long externalSequencerBurstHalfPeriod = 500; //default = fixed 500us high/low burst cadence, independent of N and loop speed
unsigned int clockMultiplyWindow = 1000;             //default = 1ms window for x2/x4 clock tick detection

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//Mode LED multiplexer mapping for hardware v1.3 and later.
//Your hardware was observed to light physical LEDs 7, 6, 4, 2, 1, 3, 5, 8 while sweeping logical modes 1-8.
//This table maps logical display positions 1-8 back to the mux addresses that light physical LEDs 1-8.

byte modeLedAddressMap[8] = { 4, 3, 5, 2, 6, 1, 0, 7 };

//////////////////////////////////////////////////////////////////////////\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
