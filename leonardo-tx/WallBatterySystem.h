#ifndef WALLBATTERYSYSTEM_H
#define WALLBATTERYSYSTEM_H

#include <Arduino.h>
#include <MFRC522v2.h>

#include "Battery.h"
#include "Config.h"

// ----- LEDState ENUM -----
enum LEDState { LED_OFF, LED_GREEN, LED_RED };

struct BatteryState {
  bool posPresent;
  bool negPresent;
  bool posPolarity;
  bool negPolarity;

  // equality operator for easy comparison (used when checking previous vs
  // current state)
  bool operator!=(const BatteryState &other) const {
    return posPresent != other.posPresent || negPresent != other.negPresent ||
           posPolarity != other.posPolarity || negPolarity != other.negPolarity;
  }
};

// ----- COMMUNICATION PACKET -----
struct __attribute__((packed)) WallStatusPacket {
  uint8_t START1;
  uint8_t START2;
  uint8_t BAT_ID;
  uint8_t NEG_PRESENT;
  uint8_t NEG_STATE;
  uint8_t POS_PRESENT;
  uint8_t POS_STATE;
  uint8_t CHK;
};

class WallBatterySystem {
public:
  // constructor
  WallBatterySystem();

  // main system methods
  bool initializeSystem(MFRC522 &reader);
  void updateSystem(MFRC522 &reader);
  void processSystemLogic();

  // system status
  bool isSystemHealthy() const;
  void printSystemStatus() const;

  // hardware setup helpers
  void initializeHardware();
  void initializeRS485();

private:
  // ----- CLASS INSTANCES -----
  Battery batteries[config::NUM_BATTERIES];
  BatteryState lastStates[config::NUM_BATTERIES];

  // ----- SYSTEM STATE -----
  LEDState currentLEDState;
  LEDState lastLEDState;
  int activeBattery;
  bool systemHealthy;

  // ----- TIMING CONTROL -----
  unsigned long lastPollTime;
  uint8_t currentPollingBattery;

  // ----- PRIVATE METHODS -----

  // battery management
  bool initializeBatteries(MFRC522 &reader);
  BatteryState getCurrentBatteryState(uint8_t batteryIndex) const;

  // communication
  void updateCommunication();
  void sendBatteryPacket(uint8_t batteryIndex, const BatteryState &state);

  // LED control
  void updateLEDs();
  LEDState determineLEDState();
  int findActiveBattery() const;
  void setLEDState(LEDState state);

  // System health
  void updateSystemHealth();
  void handleSystemFailure() const;

  // Utility
  void disableAllMuxChannels();
  uint8_t calculateChecksum(const WallStatusPacket &pkt) const;
};

#endif
