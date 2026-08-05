#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>

using byte = uint8_t;
using boolean = bool;

#ifndef HIGH
#define HIGH 0x1
#endif

#ifndef LOW
#define LOW 0x0
#endif

#ifndef INPUT
#define INPUT 0x0
#endif

#ifndef OUTPUT
#define OUTPUT 0x1
#endif

#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif

#ifndef RISING
#define RISING 0x3
#endif

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#endif

#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? ((value) |= (1UL << (bit))) : ((value) &= ~(1UL << (bit))))
#endif

template <typename T, typename U>
auto min(T a, U b) -> decltype(a < b ? a : b) {
  return a < b ? a : b;
}

template <typename T, typename U>
auto max(T a, U b) -> decltype(a > b ? a : b) {
  return a > b ? a : b;
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

struct SerialStub {
  void begin(unsigned long) {}

  template <typename T>
  void print(const T& value) { std::cout << value; }

  template <typename T>
  void println(const T& value) { std::cout << value << '\n'; }

  void println() { std::cout << '\n'; }
};

extern SerialStub Serial;
