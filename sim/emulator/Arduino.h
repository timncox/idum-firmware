#pragma once
// Freestanding Arduino stubs for the browser emulator (WebAssembly, no C++ standard library).
// Same API as sim/Arduino.h; emu.cpp provides the definitions.

#include <stddef.h>
#include <stdint.h>

using byte = uint8_t;
using boolean = bool;

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define RISING 0x3

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? ((value) |= (1UL << (bit))) : ((value) &= ~(1UL << (bit))))

template <typename T, typename U>
auto min(T a, U b) -> decltype(a + b) {
  return a < b ? a : b;
}

template <typename T, typename U>
auto max(T a, U b) -> decltype(a + b) {
  return a > b ? a : b;
}

inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

unsigned long micros();
unsigned long millis();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t value);
int analogRead(uint8_t pin);
void attachInterrupt(uint8_t pin, void (*handler)(), int mode);

long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);
void noInterrupts();
void interrupts();

extern "C" size_t strlen(const char* s);

struct SerialStub {
  void begin(unsigned long) {}
  template <typename T>
  void print(const T&) {}
  template <typename T>
  void println(const T&) {}
  void println() {}
};

extern SerialStub Serial;
