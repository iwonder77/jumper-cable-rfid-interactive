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

#include "Config.h"
#include "Reader.h"

class Battery {
public:
  Battery(uint8_t muxAddr, uint8_t id)
      : muxAddr(muxAddr), id(id),
        negative("Negative", muxAddr, config::NEGATIVE_TERMINAL_CHANNEL),
        positive("Positive", muxAddr, config::POSITIVE_TERMINAL_CHANNEL) {}

  bool initialize();
  void updateReaders();
  bool hasValidConfiguration() const;
  void printBatteryStatus() const;
  void printInitializationSummary() const;

  const Reader &getPositive() const { return positive; }
  const Reader &getNegative() const { return negative; }
  uint8_t getMuxAddr() const { return muxAddr; }
  uint8_t getId() const { return id; }
  const char *getName() const;

private:
  bool muxCommunicationOK;
  uint8_t muxAddr;
  uint8_t id;
  Reader positive;
  Reader negative;
};
