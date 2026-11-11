#pragma once

#include <stdint.h>

// 256 bytes is all the firmware uses (the ATmega4809 has 256 bytes of EEPROM).
class EEPROMClass {
public:
  uint8_t read(int address) const {
    if (address < 0 || address >= 256) return 0;
    return data[address];
  }

  void write(int address, uint8_t value) {
    if (address < 0 || address >= 256) return;
    data[address] = value;
  }

  uint8_t data[256] = {};
};

extern EEPROMClass EEPROM;
