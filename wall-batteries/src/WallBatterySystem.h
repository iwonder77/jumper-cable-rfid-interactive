#pragma once
/**
 * WallBatterySystem.h
 *
 * Coordinator for all classes and main loop
 * - hardware initialization
 * - basic LED state machine for visualizing correct polarity
 * - system health monitoring
 * - RS485 communication logic (should probably move to own class)
 */

#include <Arduino.h>

#include "Battery.h"
#include "Config.h"

// ----- LedState ENUM -----
enum class LedState : uint8_t { Off, Green, Red };

struct BatteryState {
  bool pos_present;
  bool neg_present;
  bool pos_polarity;
  bool neg_polarity;

  // equality operator for easy comparison (used when checking previous vs
  // current state)
  bool operator!=(const BatteryState &other) const {
    return pos_present != other.pos_present ||
           neg_present != other.neg_present ||
           pos_polarity != other.pos_polarity ||
           neg_polarity != other.neg_polarity;
  }
};

class WallBatterySystem {
public:
  // constructor
  WallBatterySystem();

  // main system methods
  bool initializeSystem();
  void updateSystem();
  void processSystemLogic();

  // system status
  bool isSystemHealthy() const { return system_health_; };
  void printSystemStatus() const;

private:
  // ----- CLASS INSTANCES -----
  Battery batteries_[config::NUM_BATTERIES];
  BatteryState last_battery_state_[config::NUM_BATTERIES];

  // ----- SYSTEM STATE -----
  LedState current_led_state_;
  LedState last_led_state_;
  int active_battery_id_;
  bool system_health_;

  // ----- TIMING CONTROL -----
  uint32_t last_poll_time_;
  uint8_t curr_polling_battery_;

  // ----- PRIVATE METHODS -----
  // battery management
  bool initializeBatteries();
  BatteryState getCurrentBatteryState(uint8_t battery_index) const;

  // communication
  void updateCommunication();
  void sendBatteryPacket(uint8_t battery_index, const BatteryState &state);

  // LED control
  void updateLEDs();
  LedState determineLEDState();
  int findActiveBattery() const;
  void setLEDState(LedState state);

  // System health
  void updateSystemHealth();
  void handleSystemFailure() const;

  // Utility
  void disableAllMuxChannels();
};
