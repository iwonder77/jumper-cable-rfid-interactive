#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <MFRC522v2.h>

#include "TerminalReader.h"

// ----- TCA9548A I2C MUX Channels -----
const uint8_t NEGATIVE_TERMINAL_CHANNEL = 0;
const uint8_t POSITIVE_TERMINAL_CHANNEL = 1;

class Battery {
public:
  Battery(uint8_t muxAddr, uint8_t id, uint8_t readerAddr)
      : muxAddr(muxAddr), id(id),
        negative(readerAddr, "Negative", NEGATIVE_TERMINAL_CHANNEL),
        positive(readerAddr, "Positive", POSITIVE_TERMINAL_CHANNEL) {}

  bool initialize(MFRC522 &reader);
  bool testMuxCommunication() const;
  void initializeReaders(MFRC522 &reader);
  void updateReaders(MFRC522 &reader);
  bool hasValidConfiguration() const;
  void printBatteryStatus() const;
  void printInitializationSummary() const;

  const TerminalReader &getPositive() const { return positive; }
  const TerminalReader &getNegative() const { return negative; }
  uint8_t getMuxAddr() const { return muxAddr; }
  uint8_t getId() const { return id; }
  const char *getName() const;

private:
  bool muxCommunicationOK;
  uint8_t muxAddr;
  uint8_t id;
  TerminalReader positive;
  TerminalReader negative;
};
#endif
