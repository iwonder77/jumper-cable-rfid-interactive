#ifndef WALLBATTERYSYSTEM_H
#define WALLBATTERYSYSTEM_H

#include <Arduino.h>
#include <MFRC522v2.h>

#include "Battery.h"

// ----- SYSTEM CONSTANTS -----
const uint8_t NUM_BATTERIES = 3;
const unsigned long TAG_POLL_INTERVAL = 100; // how often to check for tags (ms)

// ----- I2C ADDRESSES -----
const uint8_t TCA9548A_6V_ADDR = 0x70;
const uint8_t TCA9548A_12V_ADDR = 0x71;
const uint8_t TCA9548A_16V_ADDR = 0x72;
const uint8_t RFID2_WS1850S_ADDR = 0x28;

// ----- LED PINS -----
const uint8_t GREEN_LED_PIN = 2;
const uint8_t RED_LED_PIN = 3;

// ----- RS485 PINS -----
const uint8_t RS485_IO = 5;
const uint8_t RS485_TRANSMIT = HIGH;
const uint16_t RS485_BAUD_RATE = 9600;

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
  Battery batteries[NUM_BATTERIES];
  BatteryState lastStates[NUM_BATTERIES];

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
