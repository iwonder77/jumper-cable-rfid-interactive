#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <MFRC522v2.h>

#include "TerminalReader.h"

// ----- TCA9548A Channels -----
const uint8_t NEGATIVE_TERMINAL_CHANNEL = 0;
const uint8_t POSITIVE_TERMINAL_CHANNEL = 1;

class Battery {
public:
  Battery(uint8_t muxAddr, uint8_t id)
      : muxAddr(muxAddr), id(id),
        negative("Negative", NEGATIVE_TERMINAL_CHANNEL),
        positive("Positive", POSITIVE_TERMINAL_CHANNEL) {}

  void initializeReaders(MFRC522 &reader);
  void updateReaders(MFRC522 &reader);
  bool hasValidConfiguration() const;
  void printStatus() const;

  const TerminalReader &getPositive() const { return positive; }
  const TerminalReader &getNegative() const { return negative; }
  uint8_t getId() const { return id; }

private:
  uint8_t muxAddr;
  uint8_t id;
  TerminalReader positive;
  TerminalReader negative;
};
#endif
