#include "WallBatterySystem.h"
#include "Debug.h"
#include "MuxController.h"

// ========== CONSTRUCTOR ==========
WallBatterySystem::WallBatterySystem()
    : batteries{{config::TCA9548A_6V_ADDR, 0, config::RFID2_WS1850S_ADDR},
                {config::TCA9548A_12V_ADDR, 1, config::RFID2_WS1850S_ADDR},
                {config::TCA9548A_16V_ADDR, 2, config::RFID2_WS1850S_ADDR}},
      currentLEDState(LED_OFF), lastLEDState(LED_OFF), activeBattery(-1),
      systemHealthy(false), lastPollTime(0), currentPollingBattery(0) {

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    lastStates[i] = {false, false, 0, 0};
  }
}

// ========== PUBLIC METHODS ==========
bool WallBatterySystem::initializeSystem(MFRC522 &reader) {
  // ----- RS485 SETUP -----
  pinMode(config::RS485_DE_PIN, OUTPUT); // control pin
  digitalWrite(config::RS485_DE_PIN,
               config::RS485_TRANSMIT); // set to TRANSMIT mode

  // ----- LED SETUP -----
  pinMode(config::GREEN_LED_PIN, OUTPUT);
  pinMode(config::RED_LED_PIN, OUTPUT);
  digitalWrite(config::GREEN_LED_PIN, LOW);
  digitalWrite(config::RED_LED_PIN, LOW);

  currentLEDState = LED_OFF;
  lastLEDState = LED_OFF;

  // begin RS485 Software Serial
  Serial1.begin(config::RS485_BAUD_RATE);

  DEBUG_PRINT("RS485 Hardware Serial1 initialized at ");
  DEBUG_PRINT(config::RS485_BAUD_RATE);
  DEBUG_PRINTLN(" baud");

  DEBUG_PRINTLN("=== Jumper Cable Interactive v2 ===");

  // Initialize I2C
  Wire.begin();
  Wire.setClock(config::I2C_CLOCK_SPEED);

  disableAllMuxChannels();

  // Initialize battery subsystem
  systemHealthy = initializeBatteries(reader);

  if (!systemHealthy) {
    handleSystemFailure();
    return false;
  }

  disableAllMuxChannels();

  DEBUG_PRINTLN("\n=== System Ready ===");
  DEBUG_PRINTLN("Place jumper cable tags on terminals to test");

  return true;
}

void WallBatterySystem::updateSystem(MFRC522 &reader) {
  unsigned long currentTime = millis();

  if (currentTime - lastPollTime >= config::POLL_INTERVAL_MS) {
    // Poll only one battery per cycle (round-robin)
    batteries[currentPollingBattery].updateReaders(reader);

    // Move to next battery
    currentPollingBattery = (currentPollingBattery + 1) % config::NUM_BATTERIES;
    lastPollTime = currentTime;
  }
}

void WallBatterySystem::processSystemLogic() {
  updateCommunication();
  updateLEDs();
  updateSystemHealth();
}

bool WallBatterySystem::isSystemHealthy() const { return systemHealthy; }

void WallBatterySystem::printSystemStatus() const {
  DEBUG_PRINTLN("\n=== SYSTEM STATUS ===");
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    DEBUG_PRINT(batteries[i].getName());
    DEBUG_PRINT(" Battery: ");
    if (batteries[i].getPositive().getReaderStatus() &&
        batteries[i].getNegative().getReaderStatus()) {
      DEBUG_PRINTLN("OK");
    } else {
      DEBUG_PRINTLN("FAILED");
    }
  }
  DEBUG_PRINT("Overall System: ");
  DEBUG_PRINTLN(systemHealthy ? "HEALTHY" : "UNHEALTHY");
  DEBUG_PRINTLN("=====================");
}

// ========== BATTERY MANAGEMENT ==========
bool WallBatterySystem::initializeBatteries(MFRC522 &reader) {
  DEBUG_PRINTLN("\nInitializing batteries...");
  bool allBatteriesOK = true;

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    DEBUG_PRINT("\n--- Initializing ");
    DEBUG_PRINT(batteries[i].getName());
    DEBUG_PRINTLN(" Battery ---");

    bool batteryOK = batteries[i].initialize(reader);
    batteries[i].printInitializationSummary();

    if (!batteryOK) {
      DEBUG_PRINT("ERROR: ");
      DEBUG_PRINT(batteries[i].getName());
      DEBUG_PRINTLN(" battery initialization failed!");
      allBatteriesOK = false;
    }
  }

  return allBatteriesOK;
}

BatteryState
WallBatterySystem::getCurrentBatteryState(uint8_t batteryIndex) const {
  return {batteries[batteryIndex].getPositive().getTagState() == TAG_PRESENT,
          batteries[batteryIndex].getNegative().getTagState() == TAG_PRESENT,
          batteries[batteryIndex].getPositive().polarityOK(),
          batteries[batteryIndex].getNegative().polarityOK()};
}

// ========== COMMUNICATION ==========
void WallBatterySystem::updateCommunication() {
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    BatteryState currentState = getCurrentBatteryState(i);

    // check for any change in state
    if (currentState != lastStates[i]) {
      // update the stored state
      lastStates[i] = currentState;

      // only send packet if change in state has occured
      sendBatteryPacket(i, currentState);
      delay(config::RS485_LINE_SETTLE_MS);
    }
  }
}

void WallBatterySystem::sendBatteryPacket(uint8_t batteryIndex,
                                          const BatteryState &state) {
  WallStatusPacket packet = {0};
  packet.START1 = config::PACKET_START1;
  packet.START2 = config::PACKET_START2;
  packet.BAT_ID = batteries[batteryIndex].getId();
  packet.NEG_PRESENT = state.negPresent ? 1 : 0;
  packet.NEG_STATE = state.negPolarity ? 1 : 0;
  packet.POS_PRESENT = state.posPresent ? 1 : 0;
  packet.POS_STATE = state.posPolarity ? 1 : 0;
  packet.CHK = calculateChecksum(packet);

  Serial1.write((uint8_t *)&packet, sizeof(WallStatusPacket));
  Serial1.flush();

  DEBUG_PRINT("📤 Packet sent for battery ");
  DEBUG_PRINTLN(batteries[batteryIndex].getName());

  DEBUG_PRINT("Valid Packet Sent -> BAT:");
  DEBUG_PRINT(packet.BAT_ID);
  DEBUG_PRINT(", NEG_PRESENT: ");
  DEBUG_PRINT(packet.NEG_PRESENT);
  DEBUG_PRINT(", NEG_STATE: ");
  DEBUG_PRINT(packet.NEG_STATE);
  DEBUG_PRINT(", POS_PRESENT: ");
  DEBUG_PRINT(packet.POS_PRESENT);
  DEBUG_PRINT(", POS_STATE: ");
  DEBUG_PRINTLN(packet.POS_STATE);
}

// ========== LED CONTROL ==========
void WallBatterySystem::updateLEDs() {
  LEDState newState = determineLEDState();

  if (newState != lastLEDState) {
    setLEDState(newState);

    // Print status message and battery info
    switch (newState) {
    case LED_OFF:
      DEBUG_PRINTLN("LEDs OFF - waiting for both tags");
      break;
    case LED_GREEN:
      DEBUG_PRINT("✅ ");
      DEBUG_PRINT(batteries[activeBattery].getName());
      DEBUG_PRINTLN(" battery ready for jumpstart!");
      break;
    case LED_RED:
      DEBUG_PRINT("❌ ");
      DEBUG_PRINT(batteries[activeBattery].getName());
      DEBUG_PRINTLN(" battery incorrect configuration");
      break;
    }

    if (activeBattery >= 0) {
      batteries[activeBattery].printBatteryStatus();
    }

    lastLEDState = newState;
  }

  currentLEDState = newState;
}

LEDState WallBatterySystem::determineLEDState() {
  // LED State is determined by active battery, if any
  // active battery is any battery with tags present on both terminals
  activeBattery = findActiveBattery();

  if (activeBattery < 0) {
    return LED_OFF; // no active battery found
  }

  // check if active battery has valid configuration
  return batteries[activeBattery].hasValidConfiguration() ? LED_GREEN : LED_RED;
}

int WallBatterySystem::findActiveBattery() const {
  // find the battery with the most complete configuration
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    bool posPresent = (batteries[i].getPositive().getTagState() == TAG_PRESENT);
    bool negPresent = (batteries[i].getNegative().getTagState() == TAG_PRESENT);

    // prioritize batteries with both terminals connected
    if (posPresent && negPresent) {
      return i;
    }
  }

  return -1; // No active battery
}

void WallBatterySystem::setLEDState(LEDState state) {
  switch (state) {
  case LED_OFF:
    digitalWrite(config::GREEN_LED_PIN, LOW);
    digitalWrite(config::RED_LED_PIN, LOW);
    break;
  case LED_GREEN:
    digitalWrite(config::GREEN_LED_PIN, HIGH);
    digitalWrite(config::RED_LED_PIN, LOW);
    break;
  case LED_RED:
    digitalWrite(config::GREEN_LED_PIN, LOW);
    digitalWrite(config::RED_LED_PIN, HIGH);
    break;
  }
}

// ========== SYSTEM HEALTH ==========
void WallBatterySystem::updateSystemHealth() {
  // System is healthy if at least one battery is fully functional
  systemHealthy = false;

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    if (batteries[i].getPositive().getReaderStatus() &&
        batteries[i].getNegative().getReaderStatus()) {
      systemHealthy = true;
      break;
    }
  }
}

void WallBatterySystem::handleSystemFailure() const {
  DEBUG_PRINTLN("\nSystem initialization failed! Check wiring.");

  // Blink red LED indefinitely
  while (1) {
    digitalWrite(config::RED_LED_PIN, !digitalRead(config::RED_LED_PIN));
    delay(500);
  }
}

// ========== UTILITY METHODS ==========
void WallBatterySystem::disableAllMuxChannels() {
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries[i].getMuxAddr());
  }
}

uint8_t
WallBatterySystem::calculateChecksum(const WallStatusPacket &pkt) const {
  uint8_t sum = 0;
  sum ^= pkt.BAT_ID;
  sum ^= pkt.NEG_PRESENT;
  sum ^= pkt.NEG_STATE;
  sum ^= pkt.POS_PRESENT;
  sum ^= pkt.POS_STATE;
  return sum;
}
