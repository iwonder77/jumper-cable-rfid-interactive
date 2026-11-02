#include "WallBatterySystem.h"
#include "CommPacket.h"
#include "Config.h"
#include "Debug.h"
#include "MuxController.h"

/*
 * @brief Constructor
 */
WallBatterySystem::WallBatterySystem()
    : batteries_{{config::TCA9548A_6V_ADDR, 0},
                 {config::TCA9548A_12V_ADDR, 1},
                 {config::TCA9548A_16V_ADDR, 2}},
      current_led_state_(LedState::Off), last_led_state_(LedState::Off),
      active_battery_id_(-1), system_health_(false), last_poll_time_(0),
      curr_polling_battery_(0) {

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    last_battery_state_[i] = {false, false, false, false};
  }
}

/*
 * @brief initializes hardware
 */
bool WallBatterySystem::initializeSystem() {
  // ----- RS485 SETUP -----
  pinMode(config::RS485_DE_PIN, OUTPUT); // control pin
  digitalWrite(config::RS485_DE_PIN,
               config::RS485_TRANSMIT); // set to TRANSMIT mode

  // ----- LED SETUP -----
  pinMode(config::GREEN_LED_PIN, OUTPUT);
  pinMode(config::RED_LED_PIN, OUTPUT);
  digitalWrite(config::GREEN_LED_PIN, LOW);
  digitalWrite(config::RED_LED_PIN, LOW);

  current_led_state_ = LedState::Off;
  last_led_state_ = LedState::Off;

  // begin RS485 Hardware Serial (Serial1)
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
  system_health_ = initializeBatteries();

  if (!system_health_) {
    handleSystemFailure();
    return false;
  }

  disableAllMuxChannels();

  DEBUG_PRINTLN("\n=== System Ready ===");
  DEBUG_PRINTLN("Place jumper cable tags on terminals to test");

  return true;
}

/*
 * @brief Updates system's internal members at a certain sample interval
 */
void WallBatterySystem::updateSystem() {
  uint32_t now = millis();

  if (now - last_poll_time_ >= config::POLL_INTERVAL_MS) {
    // Poll only one battery per cycle (round-robin)
    batteries_[curr_polling_battery_].updateReaders();

    // Move to next battery
    curr_polling_battery_ = (curr_polling_battery_ + 1) % config::NUM_BATTERIES;
    last_poll_time_ = now;
  }
}

/*
 * @brief Processes internal members and executes logic accordingly
 */
void WallBatterySystem::processSystemLogic() {
  updateCommunication();
  updateLEDs();
  updateSystemHealth();
}

/*
 * @brief Prints complete system status
 */
void WallBatterySystem::printSystemStatus() const {
  DEBUG_PRINTLN("\n=== SYSTEM STATUS ===");
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    DEBUG_PRINT(batteries_[i].getName());
    DEBUG_PRINT(" Battery: ");
    if (batteries_[i].getPositive().getReaderStatus() &&
        batteries_[i].getNegative().getReaderStatus()) {
      DEBUG_PRINTLN("OK");
    } else {
      DEBUG_PRINTLN("FAILED");
    }
  }
  DEBUG_PRINT("Overall System: ");
  DEBUG_PRINTLN(system_health_ ? "HEALTHY" : "UNHEALTHY");
  DEBUG_PRINTLN("=====================");
}

/*
 * @brief Initializes each of the wall batteries
 *
 * @return successful initialization of all wall batteries
 */
bool WallBatterySystem::initializeBatteries() {
  DEBUG_PRINTLN("\nInitializing batteries...");
  bool all_batteries_ok = true;

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    DEBUG_PRINT("\n--- Initializing ");
    DEBUG_PRINT(batteries_[i].getName());
    DEBUG_PRINTLN(" Battery ---");

    bool battery_ok = batteries_[i].initialize();
    batteries_[i].printInitializationSummary();

    if (!battery_ok) {
      DEBUG_PRINT("ERROR: ");
      DEBUG_PRINT(batteries_[i].getName());
      DEBUG_PRINTLN(" battery initialization failed!");
      all_batteries_ok = false;
    }
  }

  return all_batteries_ok;
}

/*
 * @brief Retrieves the state of any battery
 *
 * @param battery_index index of battery to retrieve state from
 * @return battery state's `BatteryState` struct
 */
BatteryState
WallBatterySystem::getCurrentBatteryState(uint8_t battery_index) const {
  const Reader &pos = batteries_[battery_index].getPositive();
  const Reader &neg = batteries_[battery_index].getNegative();

  bool pos_present = (pos.getTagState() == TagState::Confirmed);
  bool neg_present = (neg.getTagState() == TagState::Confirmed);

  bool pos_polarity =
      pos_present && (strncmp(pos.getTagData().type, "POS", 3) == 0);
  bool neg_polarity =
      neg_present && (strncmp(neg.getTagData().type, "NEG", 3) == 0);

  return {pos_present, neg_present, pos_polarity, neg_polarity};
}

/*
 * @brief Updates communication to Toy Car System whenever a battery's state
 * changes
 */
void WallBatterySystem::updateCommunication() {
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    BatteryState curr_state = getCurrentBatteryState(i);

    // check for any change in state
    if (curr_state != last_battery_state_[i]) {
      // update the stored state
      last_battery_state_[i] = curr_state;

      // only send packet if change in state has occured
      sendBatteryPacket(i, curr_state);
      delay(config::RS485_LINE_SETTLE_MS);
    }
  }
}

/*
 * @brief Constructs data packet to be sent, then sends it to Toy Car system via
 * RS485
 *
 * @param battery_index Index of battery we are sending packet of
 * @param state Direct reference to the battery's state
 */
void WallBatterySystem::sendBatteryPacket(uint8_t battery_index,
                                          const BatteryState &state) {
  WallStatusPacket packet = {0};
  packet.START1 = config::PACKET_START1;
  packet.START2 = config::PACKET_START2;
  packet.BAT_ID = batteries_[battery_index].getId();
  packet.NEG_PRESENT = state.neg_present ? 1 : 0;
  packet.NEG_STATE = state.neg_polarity ? 1 : 0;
  packet.POS_PRESENT = state.pos_present ? 1 : 0;
  packet.POS_STATE = state.pos_polarity ? 1 : 0;
  packet.CHK = xorChecksum(packet);

  Serial1.write((uint8_t *)&packet, sizeof(WallStatusPacket));
  Serial1.flush();

  DEBUG_PRINT("📤 Packet sent for battery ");
  DEBUG_PRINTLN(batteries_[battery_index].getName());

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

/*
 * @brief Controls which LED to light up, if any, based on battery state
 */
void WallBatterySystem::updateLEDs() {
  LedState new_state = determineLEDState();

  if (new_state != last_led_state_) {
    setLEDState(new_state);

    // Print status message and battery info
    switch (new_state) {
    case LedState::Off:
      DEBUG_PRINTLN("LEDs OFF - waiting for both tags");
      break;
    case LedState::Green:
      DEBUG_PRINT("✅ ");
      DEBUG_PRINT(batteries_[active_battery_id_].getName());
      DEBUG_PRINTLN(" battery ready for jumpstart!");
      break;
    case LedState::Red:
      DEBUG_PRINT("❌ ");
      DEBUG_PRINT(batteries_[active_battery_id_].getName());
      DEBUG_PRINTLN(" battery incorrect configuration");
      break;
    }

    if (active_battery_id_ >= 0) {
      batteries_[active_battery_id_].printBatteryStatus();
    }

    last_led_state_ = new_state;
  }

  current_led_state_ = new_state;
}

/*
 * @brief Determines the state of an active battery, if any. An active
 * battery is any battery with tags present on both terminals
 *
 * @return Led state enum
 */
LedState WallBatterySystem::determineLEDState() {
  active_battery_id_ = findActiveBattery();

  if (active_battery_id_ < 0) {
    return LedState::Off; // no active battery found
  }

  // check if active battery has valid configuration
  return batteries_[active_battery_id_].hasValidConfiguration()
             ? LedState::Green
             : LedState::Red;
}

/*
 * @brief Searches through internal battery members and uses their methods to
 * determine if any of them are active
 *
 * @return int corresponding to index of battery that is currently active
 */
int WallBatterySystem::findActiveBattery() const {
  // find the battery with the most complete configuration
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    bool pos_present =
        (batteries_[i].getPositive().getTagState() == TagState::Confirmed);
    bool neg_present =
        (batteries_[i].getNegative().getTagState() == TagState::Confirmed);

    // prioritize battery with both terminals connected
    if (pos_present && neg_present) {
      return i;
    }
  }

  return -1; // No active battery
}

/*
 * @brief Sets the state of the LED
 */
void WallBatterySystem::setLEDState(LedState state) {
  switch (state) {
  case LedState::Off:
    digitalWrite(config::GREEN_LED_PIN, LOW);
    digitalWrite(config::RED_LED_PIN, LOW);
    break;
  case LedState::Green:
    digitalWrite(config::GREEN_LED_PIN, HIGH);
    digitalWrite(config::RED_LED_PIN, LOW);
    break;
  case LedState::Red:
    digitalWrite(config::GREEN_LED_PIN, LOW);
    digitalWrite(config::RED_LED_PIN, HIGH);
    break;
  }
}

/*
 * @brief Handles system health monitoring
 */
void WallBatterySystem::updateSystemHealth() {
  // System is healthy if at least one battery is fully functional
  system_health_ = false;

  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    if (batteries_[i].getPositive().getReaderStatus() &&
        batteries_[i].getNegative().getReaderStatus()) {
      system_health_ = true;
      break;
    }
  }
}

/*
 * @brief Handles system failure
 */
void WallBatterySystem::handleSystemFailure() const {
  DEBUG_PRINTLN("\nSystem initialization failed! Check wiring.");

  // Blink red LED indefinitely
  while (1) {
    digitalWrite(config::RED_LED_PIN, !digitalRead(config::RED_LED_PIN));
    delay(500);
  }
}

/*
 * @brief Utility function for diabling all mux channels
 */
void WallBatterySystem::disableAllMuxChannels() {
  for (int i = 0; i < config::NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries_[i].getMuxAddr());
  }
}
