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
  Battery(uint8_t mux_addr, uint8_t id)
      : mux_addr_(mux_addr), id_(id),
        negative_("Negative", mux_addr, config::NEGATIVE_TERMINAL_CHANNEL),
        positive_("Positive", mux_addr, config::POSITIVE_TERMINAL_CHANNEL) {}

  bool initialize();
  void updateReaders();
  bool hasValidConfiguration() const;
  void printBatteryStatus() const;
  void printInitializationSummary() const;

  const Reader &getPositive() const { return positive_; }
  const Reader &getNegative() const { return negative_; }
  uint8_t getMuxAddr() const { return mux_addr_; }
  uint8_t getId() const { return id_; }
  const char *getName() const;

private:
  bool mux_communication_ok_;
  uint8_t mux_addr_;
  uint8_t id_;
  Reader positive_;
  Reader negative_;
};
