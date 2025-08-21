#include "TerminalReader.h"

void TerminalReader::initialize(MFRC522 &reader) {
  // assume channel has already been set
  Serial.print("Testing ");
  Serial.print(name);
  Serial.print(" Reader I2C Communication on Channel ");
  Serial.print(channel);
  Serial.print(": ");

  Wire.beginTransmission(address);
  if (Wire.endTransmission() != 0) {
    Serial.println("FAILED - I2C Communication ERROR");
    isReaderOK = false;
    return;
  }

  Serial.println(" SUCCESS");
  isReaderOK = true;

  // initialize
  reader.PCD_Init();

  delay(50);
}

void TerminalReader::update(MFRC522 &reader) {
  if (!isReaderOK)
    return;

  unsigned long currentTime = millis();
  bool tagDetected = false;

  // Try to detect tag without halting it
  if (reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()) {
    tagDetected = true;

    // Check if this is the same tag or a different one
    bool isSameTag = (lastUIDLength == reader.uid.size) &&
                     compareUID(lastUID, reader.uid.uidByte, reader.uid.size);

    // Update UID
    memcpy(lastUID, reader.uid.uidByte, reader.uid.size);
    lastUIDLength = reader.uid.size;

    // Update timing
    lastSeenTime = currentTime;
    consecutiveFails = 0;

    // state transitions
    switch (tagState) {
    case TAG_ABSENT:
      tagState = TAG_DETECTED;
      firstSeenTime = currentTime; // start debounce timer
      Serial.print(name);
      Serial.println(": New tag detected!");
      break;

    case TAG_DETECTED:
      // Check if enough time has passed for debouncing
      if (currentTime - firstSeenTime > TAG_DEBOUNCE_TIME) {
        tagState = TAG_PRESENT;
        Serial.print(name);
        Serial.println(": Tag confirmed present");
        readTagData(reader);
      }
      break;

    case TAG_PRESENT:
      if (!isSameTag) {
        // different tag detected
        tagState = TAG_DETECTED;
        firstSeenTime = currentTime;
        clearTagData();
        Serial.print(name);
        Serial.println(": Different tag detected!");
      }
      // otherwise same tag still present - no action needed
      break;

    case TAG_REMOVED:
      tagState = TAG_DETECTED;
      firstSeenTime = currentTime;
      Serial.print(name);
      Serial.println(": Tag returned!");
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
        Serial.print(name);
        Serial.println(": Tag detection failed");
      }
    } else if (tagState == TAG_PRESENT) {
      // More lenient for established tags
      if (consecutiveFails >= TAG_PRESENCE_THRESHOLD ||
          (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT)) {
        tagState = TAG_REMOVED;
        Serial.print(name);
        Serial.println(": Tag removed!");
      }
    } else if (tagState == TAG_REMOVED) {
      // Confirm removal
      if (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT * 2) {
        tagState = TAG_ABSENT;
        clearTagData();
        Serial.print(name);
        Serial.println(": Tag removal confirmed");
      }
    }
  }
}

void TerminalReader::printStatus() const {
  switch (tagState) {
  case TAG_ABSENT:
    Serial.println("No card");
    break;
  case TAG_DETECTED:
    Serial.println("Detecting...");
    break;
  case TAG_PRESENT:
    Serial.print(tagData.type);
    Serial.print(" #");
    Serial.print(tagData.id);
    Serial.println(isCorrectPolarity ? " ✓" : " ✗ Wrong polarity");
    break;
  case TAG_REMOVED:
    Serial.println("Card removed (confirming...)");
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

  Serial.print(name);
  Serial.println(": Reading tag data...");

  byte buffer[18];
  byte bufferSize = sizeof(buffer);

  if (reader.MIFARE_Read(TAG_START_READ_PAGE, buffer, &bufferSize) !=
      MFRC522::StatusCode::STATUS_OK) {
    Serial.print(name);
    Serial.println(": Failed to read card data");
    // Don't clear tag data - we know tag is present, just couldn't read it
    return;
  }

  JumperCableTagData data;
  memcpy(&data, buffer, sizeof(JumperCableTagData));

  uint8_t expectedChecksum =
      calculateChecksum((uint8_t *)&data, sizeof(data) - 1);
  if (expectedChecksum != data.checksum) {
    Serial.print(name);
    Serial.println(": Checksum error");
    return;
  }

  tagData = data;

  bool isTagPos = (strncmp(data.type, "POS", 3) == 0);
  bool isTerminalPos = (channel == 1); // positive channel is 1
  isCorrectPolarity = (isTagPos == isTerminalPos);

  Serial.print(name);
  Serial.print(": Read ");
  Serial.print(data.type);
  Serial.print(" cable #");
  Serial.print(data.id);

  if (!isCorrectPolarity) {
    Serial.print(" [WRONG POLARITY!]");
  }
  Serial.println();

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
