#include <Arduino.h>

#include "Battery.h"
#include "Config.h"
#include "Debug.h"
#include "MuxController.h"

bool Battery::initialize(MFRC522 &reader) {
  // Test MUX communication first
  Wire.beginTransmission(getMuxAddr());
  byte result = Wire.endTransmission();
  if (result != 0) {
    return false;
  }

  // ----- Initialize the RFID readers -----
  // initialize positive terminal
  MuxController::selectChannel(muxAddr, config::POSITIVE_TERMINAL_CHANNEL);
  delay(config::CHANNEL_SWITCH_SETTLE_MS);
  positive.init(reader);

  // initialize negative terminal
  MuxController::selectChannel(muxAddr, config::NEGATIVE_TERMINAL_CHANNEL);
  delay(config::CHANNEL_SWITCH_SETTLE_MS);
  negative.init(reader);

  if (!positive.getReaderStatus() || !negative.getReaderStatus()) {
    DEBUG_PRINT("Warning: Battery ");
    DEBUG_PRINT(id);
    DEBUG_PRINTLN(" has failed terminal(s)");
  }

  // return true if both readers are working
  return (positive.getReaderStatus() && negative.getReaderStatus());
}

void Battery::updateReaders(MFRC522 &reader) {
  // update positive terminal
  MuxController::selectChannel(muxAddr, config::POSITIVE_TERMINAL_CHANNEL);
  reader.PCD_Init();
  positive.update(reader);

  // update negative terminal
  MuxController::selectChannel(muxAddr, config::NEGATIVE_TERMINAL_CHANNEL);
  reader.PCD_Init();
  negative.update(reader);

  MuxController::disableChannel(muxAddr);
}

bool Battery::hasValidConfiguration() const {
  // both terminals must have tags in PRESENT state
  if (positive.getTagState() != TAG_PRESENT ||
      negative.getTagState() != TAG_PRESENT)
    return false;

  // both cards must have correct polarity
  if (!positive.polarityOK() || !negative.polarityOK())
    return false;

  // cable IDs must form valid pair
  uint8_t posID = positive.getTagData().id;
  uint8_t negID = negative.getTagData().id;

  return (posID == 1 && negID == 3) || (posID == 2 && negID == 4) ||
         (posID == 2 && negID == 3) || (posID == 1 && negID == 4);
}

void Battery::printBatteryStatus() const {
  DEBUG_PRINTLN("--- Configuration Status ---");

  DEBUG_PRINT("Positive Terminal: ");
  positive.printStatus();

  DEBUG_PRINT("Negative Terminal: ");
  negative.printStatus();

  DEBUG_PRINTLN("----------------------------");
}

void Battery::printInitializationSummary() const {
  DEBUG_PRINT(getName());
  DEBUG_PRINT(" Wall Battery: MUX=");
  DEBUG_PRINT(muxCommunicationOK ? "OK" : "FAILED");
  DEBUG_PRINT(", Positive=");
  DEBUG_PRINT(positive.getReaderStatus() ? "OK" : "FAILED");
  DEBUG_PRINT(", Negative=");
  DEBUG_PRINTLN(negative.getReaderStatus() ? "OK" : "FAILED");
}

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
