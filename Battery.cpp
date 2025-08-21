#include <Arduino.h>

#include "Battery.h"
#include "MuxController.h"

void Battery::initializeReaders(MFRC522 &reader) {
  Serial.print("Initializing Battery ");
  Serial.println(id);

  // Initialize positive terminal
  MuxController::selectChannel(muxAddr, POSITIVE_TERMINAL_CHANNEL);
  delay(10);
  positive.initialize(reader);

  // Initialize negative terminal
  MuxController::selectChannel(muxAddr, NEGATIVE_TERMINAL_CHANNEL);
  delay(10);
  negative.initialize(reader);

  if (!positive.getReaderStatus() || !negative.getReaderStatus()) {
    Serial.print("Warning: Battery ");
    Serial.print(id);
    Serial.println(" has failed terminal(s)");
  }
}

void Battery::updateReaders(MFRC522 &reader) {
  // update positive terminal
  MuxController::selectChannel(muxAddr, POSITIVE_TERMINAL_CHANNEL);
  delay(20);
  positive.update(reader);

  // update negative terminal
  MuxController::selectChannel(muxAddr, NEGATIVE_TERMINAL_CHANNEL);
  delay(20);
  negative.update(reader);

  MuxController::disableChannel(muxAddr);
}

bool Battery::hasValidConfiguration() const {
  // Both terminals must have tags in PRESENT state
  if (positive.getTagState() != TAG_PRESENT ||
      negative.getTagState() != TAG_PRESENT)
    return false;

  // Both cards must have correct polarity
  if (!positive.polarityOK() || !negative.polarityOK())
    return false;

  // Cable IDs must form valid pair
  uint8_t posID = positive.getTagData().id;
  uint8_t negID = negative.getTagData().id;

  return (posID == 1 && negID == 3) || (posID == 2 && negID == 4) ||
         (posID == 2 && negID == 3) || (posID == 1 && negID == 4);
}

void Battery::printStatus() const {
  Serial.println("--- Configuration Status ---");

  Serial.print("Positive Terminal: ");
  positive.printStatus();

  Serial.print("Negative Terminal: ");
  negative.printStatus();

  Serial.println("----------------------------");
}
