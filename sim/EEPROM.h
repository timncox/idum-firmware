#pragma once

#include <array>
#include <cstdint>

class EEPROMClass {
public:
  uint8_t read(int address) const {
    if (address < 0 || address >= static_cast<int>(data.size())) return 0;
    return data[static_cast<size_t>(address)];
  }

  void write(int address, uint8_t value) {
    if (address < 0 || address >= static_cast<int>(data.size())) return;
    data[static_cast<size_t>(address)] = value;
  }

  void clear() {
    data.fill(0);
  }

private:
  std::array<uint8_t, 1024> data {};
};

extern EEPROMClass EEPROM;
