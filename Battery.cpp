#include <Arduino.h>

#include "Battery.h"
#include "MuxController.h"

bool Battery::initialize(MFRC522 &reader) {
  // Test MUX communication first
  muxCommunicationOK = testMuxCommunication();
  if (!muxCommunicationOK) {
    return false;
  }

  // Initialize the RFID readers
  initializeReaders(reader);

  // Return true if at least one reader is working
  return (positive.getReaderStatus() && negative.getReaderStatus());
}

bool Battery::testMuxCommunication() const {
  Wire.beginTransmission(getMuxAddr());
  byte result = Wire.endTransmission();
  return (result == 0);
}

void Battery::initializeReaders(MFRC522 &reader) {
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
  delay(10);
  positive.update(reader);

  // update negative terminal
  MuxController::selectChannel(muxAddr, NEGATIVE_TERMINAL_CHANNEL);
  delay(10);
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
  Serial.println("--- Configuration Status ---");

  Serial.print("Positive Terminal: ");
  positive.printStatus();

  Serial.print("Negative Terminal: ");
  negative.printStatus();

  Serial.println("----------------------------");
}

void Battery::printInitializationSummary() const {
  Serial.print(getName());
  Serial.print(" Wall Battery: MUX=");
  Serial.print(muxCommunicationOK ? "OK" : "FAILED");
  Serial.print(", Positive=");
  Serial.print(positive.getReaderStatus() ? "OK" : "FAILED");
  Serial.print(", Negative=");
  Serial.println(negative.getReaderStatus() ? "OK" : "FAILED");
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
