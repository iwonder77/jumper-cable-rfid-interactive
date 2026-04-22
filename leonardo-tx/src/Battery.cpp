#include <Arduino.h>

#include "Battery.h"
#include "Debug.h"
#include "Reader.h"

/*
 * @brief Initializes both terminal readers
 *
 * @return Successful initialization
 */
bool Battery::initialize() {
  // Test MUX communication first
  Wire.beginTransmission(getMuxAddr());
  byte result = Wire.endTransmission();
  if (result != 0) {
    mux_communication_ok_ = false;
    return false;
  } else {
    mux_communication_ok_ = true;
  }

  // ----- Initialize the RFID readers -----
  positive_.init();
  negative_.init();

  if (!positive_.getReaderStatus() || !negative_.getReaderStatus()) {
    DEBUG_PRINT("Warning: Battery ");
    DEBUG_PRINT(id_);
    DEBUG_PRINTLN(" has failed terminal(s)");
  }

  // return true if both readers are working
  return (positive_.getReaderStatus() && negative_.getReaderStatus());
}

/*
 * @brief Updates both positive and negative readers
 */
void Battery::updateReaders() {
  positive_.update();
  negative_.update();
}

/*
 * @brief Determines whether, if both readers detect a tag, polarity of jumper
 * cable tags is correct
 *
 * @return True if configuration is correct, false otherwise
 */
bool Battery::hasValidConfiguration() const {
  // both terminals must have tags in Confirmed state
  if (positive_.getTagState() != TagState::Confirmed ||
      negative_.getTagState() != TagState::Confirmed)
    return false;

  // both cards must have correct polarity
  if (strncmp(positive_.getTagData().type, "POS", 3) != 0)
    return false;

  if (strncmp(negative_.getTagData().type, "NEG", 3) != 0)
    return false;

  // cable IDs must form valid pair
  uint8_t pos_id = positive_.getTagData().id;
  uint8_t neg_id = negative_.getTagData().id;

  return (pos_id == 1 && neg_id == 3) || (pos_id == 2 && neg_id == 4) ||
         (pos_id == 2 && neg_id == 3) || (pos_id == 1 && neg_id == 4);
}

/*
 * @brief Debugging prints for the configuration status of the battery and its
 * terminals
 */
void Battery::printBatteryStatus() const {
  DEBUG_PRINTLN("--- Configuration Status ---");

  DEBUG_PRINT("Positive Terminal: ");
  positive_.printStatus();

  DEBUG_PRINT("Negative Terminal: ");
  negative_.printStatus();

  DEBUG_PRINTLN("----------------------------");
}

/*
 * @brief Helper function for printing important init information during setup
 * phase
 */
void Battery::printInitializationSummary() const {
  DEBUG_PRINT(getName());
  DEBUG_PRINT(" Wall Battery: MUX=");
  DEBUG_PRINT(mux_communication_ok_ ? "OK" : "FAILED");
  DEBUG_PRINT(", Positive=");
  DEBUG_PRINT(positive_.getReaderStatus() ? "OK" : "FAILED");
  DEBUG_PRINT(", Negative=");
  DEBUG_PRINTLN(negative_.getReaderStatus() ? "OK" : "FAILED");
}

/*
 * @brief Helper function for mapping battery ID to battery type
 */
const char *Battery::getName() const {
  switch (id_) {
  case 0:
    return "6V";
  case 1:
    return "12V";
  case 2:
    return "16V";
  default:
    return "Unknown";
  }
}
