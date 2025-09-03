#include "TerminalReader.h"
#include "Debug.h"

void TerminalReader::initialize(MFRC522 &reader) {
  // assume channel has already been set
  DEBUG_PRINT("Testing ");
  DEBUG_PRINT(name);
  DEBUG_PRINT(" Reader I2C Communication on Channel ");
  DEBUG_PRINT(channel);
  DEBUG_PRINT(": ");

  Wire.beginTransmission(address);
  if (Wire.endTransmission() != 0) {
    DEBUG_PRINTLN("FAILED - I2C Communication ERROR");
    isReaderOK = false;
    return;
  }

  DEBUG_PRINTLN(" SUCCESS");
  isReaderOK = true;

  // initialize
  reader.PCD_Init();

  delay(20);
}

void TerminalReader::update(MFRC522 &reader) {
  if (!isReaderOK)
    return;

  unsigned long currentTime = millis();
  bool tagDetected = false;

  // try to detect tag without halting it
  if (reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()) {
    tagDetected = true;

    // check if this is the same tag or a different one
    bool isSameTag = (lastUIDLength == reader.uid.size) &&
                     compareUID(lastUID, reader.uid.uidByte, reader.uid.size);

    // update UID
    memcpy(lastUID, reader.uid.uidByte, reader.uid.size);
    lastUIDLength = reader.uid.size;

    // update timing
    lastSeenTime = currentTime;
    consecutiveFails = 0;

    // state transitions
    switch (tagState) {
    case TAG_ABSENT:
      tagState = TAG_DETECTED;
      firstSeenTime = currentTime; // start debounce timer
      DEBUG_PRINT(name);
      DEBUG_PRINTLN(": New tag detected!");
      break;

    case TAG_DETECTED:
      // check if enough time has passed for debouncing
      if (currentTime - firstSeenTime > TAG_DEBOUNCE_TIME) {
        tagState = TAG_PRESENT;
        DEBUG_PRINT(name);
        DEBUG_PRINTLN(": Tag confirmed present");
        readTagData(reader);
      }
      break;

    case TAG_PRESENT:
      if (!isSameTag) {
        // different tag detected
        tagState = TAG_DETECTED;
        firstSeenTime = currentTime;
        clearTagData();
        DEBUG_PRINT(name);
        DEBUG_PRINTLN(": Different tag detected!");
      }
      // otherwise same tag still present - no action needed
      break;

    case TAG_REMOVED:
      tagState = TAG_DETECTED;
      firstSeenTime = currentTime;
      DEBUG_PRINT(name);
      DEBUG_PRINTLN(": Tag returned!");
      break;
    }
  }

  // Handle absence detection
  if (!tagDetected && tagState != TAG_ABSENT) {
    consecutiveFails++;

    // Use different logic based on current state
    if (tagState == TAG_DETECTED) {
      // Quick timeout for tags that were just detected
      if (consecutiveFails >= 2) {
        tagState = TAG_ABSENT;
        clearTagData();
        DEBUG_PRINT(name);
        DEBUG_PRINTLN(": Tag detection failed");
      }
    } else if (tagState == TAG_PRESENT) {
      // More lenient for established tags
      if (consecutiveFails >= TAG_PRESENCE_THRESHOLD ||
          (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT)) {
        tagState = TAG_REMOVED;
        DEBUG_PRINT(name);
        DEBUG_PRINTLN(": Tag removed!");
      }
    } else if (tagState == TAG_REMOVED) {
      // Confirm removal
      if (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT * 2) {
        tagState = TAG_ABSENT;
        clearTagData();
        DEBUG_PRINT(name);
        DEBUG_PRINTLN(": Tag removal confirmed");
      }
    }
  }
}

void TerminalReader::printStatus() const {
  switch (tagState) {
  case TAG_ABSENT:
    DEBUG_PRINTLN("No card");
    break;
  case TAG_DETECTED:
    DEBUG_PRINTLN("Detecting...");
    break;
  case TAG_PRESENT:
    DEBUG_PRINT(tagData.type);
    DEBUG_PRINT(" #");
    DEBUG_PRINT(tagData.id);
    DEBUG_PRINTLN(isCorrectPolarity ? " ✓" : " ✗ Wrong polarity");
    break;
  case TAG_REMOVED:
    DEBUG_PRINTLN("Card removed (confirming...)");
    break;
  }
}

void TerminalReader::clearTagData() {
  isCorrectPolarity = false;
  memset(&tagData, 0, sizeof(tagData));
  lastUIDLength = 0;
  memset(lastUID, 0, sizeof(lastUID));
}

void TerminalReader::readTagData(MFRC522 &reader) {
  if (!isReaderOK || tagState != TAG_PRESENT)
    return;

  DEBUG_PRINT(name);
  DEBUG_PRINTLN(": Reading tag data...");

  byte buffer[18];
  byte bufferSize = sizeof(buffer);

  if (reader.MIFARE_Read(TAG_START_READ_PAGE, buffer, &bufferSize) !=
      MFRC522::StatusCode::STATUS_OK) {
    DEBUG_PRINT(name);
    DEBUG_PRINTLN(": Failed to read card data");
    // Don't clear tag data - we know tag is present, just couldn't read it
    return;
  }

  JumperCableTagData data;
  memcpy(&data, buffer, sizeof(JumperCableTagData));

  uint8_t expectedChecksum =
      calculateChecksum((uint8_t *)&data, sizeof(data) - 1);
  if (expectedChecksum != data.checksum) {
    DEBUG_PRINT(name);
    DEBUG_PRINTLN(": Checksum error");
    return;
  }

  tagData = data;

  bool isTagPos = (strncmp(data.type, "POS", 3) == 0);
  bool isTerminalPos = (channel == 1); // positive channel is 1
  isCorrectPolarity = (isTagPos == isTerminalPos);

  DEBUG_PRINT(name);
  DEBUG_PRINT(": Read ");
  DEBUG_PRINT(data.type);
  DEBUG_PRINT(" cable #");
  DEBUG_PRINT(data.id);

  if (!isCorrectPolarity) {
    DEBUG_PRINT(" [WRONG POLARITY!]");
  }
  DEBUG_PRINTLN();

  /*****************
   * DON'T HALT - let tag remain active for continuous detection
   * reader.PICC_HaltA();
   ******************/
  reader.PCD_StopCrypto1();
}

// ========== UTILITY FUNCTIONS ==========
uint8_t TerminalReader::calculateChecksum(const uint8_t *data, uint8_t length) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum ^= data[i];
  }
  return sum;
}

bool TerminalReader::compareUID(byte *uid1, byte *uid2, byte length) {
  return memcmp(uid1, uid2, length) == 0;
}
