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
    muxCommunicationOK = false;
    return false;
  } else {
    muxCommunicationOK = true;
  }

  // ----- Initialize the RFID readers -----
  positive.init(reader);
  negative.init(reader);

  if (!positive.getReaderStatus() || !negative.getReaderStatus()) {
    DEBUG_PRINT("Warning: Battery ");
    DEBUG_PRINT(id);
    DEBUG_PRINTLN(" has failed terminal(s)");
  }

  // return true if both readers are working
  return (positive.getReaderStatus() && negative.getReaderStatus());
}

/*
 * @brief Updates both positive and negative readers
 *
 * @param reader MFRC522 rfid reader object
 */
void Battery::updateReaders() {
  positive.update(reader);
  negative.update(reader);
}

/*
 * @brief Determines whether, if both readers detect a tag, polarity of jumper
 * cable tags is correct
 *
 * @return True if configuration is correct, false otherwise
 */
bool Battery::hasValidConfiguration() const {
  // both terminals must have tags in Confirmed state
  if (positive.getTagState() != TagState::Confirmed ||
      negative.getTagState() != TagState::Confirmed)
    return false;

  // both cards must have correct polarity
  if (strncmp(positive.getTagData().type, "POS", 3) != 0)
    return false;

  if (strncmp(negative.getTagData().type, "NEG", 3) != 0)
    return false;

  // cable IDs must form valid pair
  uint8_t posID = positive.getTagData().id;
  uint8_t negID = negative.getTagData().id;

  return (posID == 1 && negID == 3) || (posID == 2 && negID == 4) ||
         (posID == 2 && negID == 3) || (posID == 1 && negID == 4);
}

/*
 * @brief Debugging prints for the configuration status of the battery and its
 * terminals
 */
void Battery::printBatteryStatus() const {
  DEBUG_PRINTLN("--- Configuration Status ---");

  DEBUG_PRINT("Positive Terminal: ");
  positive.printStatus();

  DEBUG_PRINT("Negative Terminal: ");
  negative.printStatus();

  DEBUG_PRINTLN("----------------------------");
}

/*
 * @brief Helper function for printing important init information during setup
 * phase
 */
void Battery::printInitializationSummary() const {
  DEBUG_PRINT(getName());
  DEBUG_PRINT(" Wall Battery: MUX=");
  DEBUG_PRINT(muxCommunicationOK ? "OK" : "FAILED");
  DEBUG_PRINT(", Positive=");
  DEBUG_PRINT(positive.getReaderStatus() ? "OK" : "FAILED");
  DEBUG_PRINT(", Negative=");
  DEBUG_PRINTLN(negative.getReaderStatus() ? "OK" : "FAILED");
}

/*
 * @brief Helper function for mapping battery ID to battery type
 */
const char *Battery::getName() const {
  switch (id) {
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
