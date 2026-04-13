#pragma once
/**
 * Battery.h
 *
 * Centralized class for each battery on the wall
 * - Each `Battery` will coordinate its own two terminal readers (positive and
 * negative)
 * - Provides helper functions for retrieving reader instances and valid
 * configurations if tags present
 */

#include <Arduino.h>
#include <MFRC522v2.h>

#include "Config.h"
#include "TerminalReader.h"

class Battery {
public:
  Battery(uint8_t muxAddr, uint8_t id, uint8_t readerAddr)
      : muxAddr(muxAddr), id(id),
        negative(readerAddr, "Negative", config::NEGATIVE_TERMINAL_CHANNEL),
        positive(readerAddr, "Positive", config::POSITIVE_TERMINAL_CHANNEL) {}

  bool initialize(MFRC522 &reader);
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
