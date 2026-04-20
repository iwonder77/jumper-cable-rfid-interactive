#include "Reader.h"
#include "Config.h"
#include "Debug.h"
#include "JumperCableTag.h"

// NOTE: must call Wire.begin() in setup() before we initialize reader!
void Reader::init() {
  // a guard is used to ensure channel is disabled when this guy goes out of
  // scope (see struct in Reader.h)
  MuxGuard guard(mux_addr_, channel_);

  Serial.print("Testing ");
  Serial.print(name_);
  Serial.print(" Reader I2C Communication on Channel ");
  Serial.print(channel_);
  Serial.print(": ");

  Wire.beginTransmission(config::WS1850S_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("FAILED - I2C Communication ERROR");
    reader_ok_ = false;
    return;
  }

  Serial.println(" SUCCESS");
  reader_ok_ = true;

  // initialize hardware instance
  reader_.PCD_Init();
}

void Reader::update() {
  if (!reader_ok_) {
    return;
  }

  // same scoped mux selection as init(): everything below talks to the
  // reader on channel `channel_`, and the mux channels are disabled when
  // update() returns (MuxGuard destructor).
  MuxGuard guard(mux_addr_, channel_);

  uint32_t now = millis();
  bool tag_detected = false; // flips to false on every update() call, only set
                             // to true when detected with MFRC522 methods below

  // --- TAG DETECTION ---
  if (reader_.PICC_IsNewCardPresent() && reader_.PICC_ReadCardSerial()) {
    tag_detected = true;

    // check if this is the same tag or a different one
    bool is_same_tag = (last_UID_length_ == reader_.uid.size) &&
                       compareUID(last_UID_, last_UID_length_,
                                  reader_.uid.uidByte, reader_.uid.size);

    // update UID
    memcpy(last_UID_, reader_.uid.uidByte, reader_.uid.size);
    last_UID_length_ = reader_.uid.size;

    // update timing and error
    last_seen_time_ = now;
    consecutive_fails_ = 0;

    // transition tagState now that a tag has been detected
    switch (tag_state_) {
    case TagState::Absent:
      tag_state_ = TagState::Detecting;
      first_seen_time_ = now; // start debounce timer
      Serial.print(name_);
      Serial.println(": New tag detected!");
      break;
    case TagState::Detecting:
      // check debounce timer in case we have a false positive detection
      // this debounce check ensures tag is actually present
      if (now - first_seen_time_ > config::TAG_DEBOUNCE_TIME) {
        tag_state_ = TagState::Confirmed;
        tag_identified_ = false;
        read_attempts_ = 0;
        Serial.print(name_);
        Serial.println(
            ": Tag confirmed present, transitioning to read its data");
      }
      break;
    case TagState::Confirmed:
      // 1. Handle tag swap first
      // before doing anything else, check if this is a different tag, if it is,
      // we need to throw away our current state, fire a removal event, clear
      // previous tag data, and start the debounce process over again for the
      // new tag
      if (!is_same_tag) {
        // different tag detected
        tag_state_ = TagState::Detecting;
        first_seen_time_ = now;
        Serial.print(name_);
        Serial.println(": Different tag detected, clearing previous data");
        clearTagData();
        tag_identified_ = false;
        read_attempts_ = 0;
        break; // exit the switch - don't try to read a tag we haven't confirmed
      }
      // 2. Attempt tag identification
      // we can assume the same tag is still here, if we haven't identified it
      // yet, try reading
      if (!tag_identified_) {

        if (readTagData()) {
          // parse succeeded - we must know what this piece is now
          tag_identified_ = true;
          Serial.print(name_);
          Serial.print(": Identified piece as ");
          Serial.println(jumper_cable_tag::typeToString(tag_));
          DEBUG_PRINT(" #");
          DEBUG_PRINTLN(tag_.id);
        } else {
          // parse failed - count read attempts here
          read_attempts_++;

          if (read_attempts_ >= config::MAX_READ_ATTEMPTS) {
            Serial.print(name_);
            Serial.println(": Tag present but unreadable after max attempts");
            // stop trying — tag is physically there but we can't identify it.
            // we don't clear tag state or change tag_state, because the tag
            // IS present. we just can't read it. the system treats this slot
            // as occupied but unknown.
          }
        }
      }
      // else: tag is already identified, same tag still present - nothing to
      // do
      break;
    case TagState::Departing:
      tag_state_ = TagState::Detecting;
      first_seen_time_ = now;
      Serial.print(name_);
      Serial.println(": Tag returned!");
      break;
    default:
      break;
    }
  }

  // --- ABSENCE DETECTION ---
  if (!tag_detected && tag_state_ != TagState::Absent) {
    consecutive_fails_++;

    switch (tag_state_) {
    case TagState::Detecting:
      // small timeout for tags that were just detected or for false positives
      if (consecutive_fails_ > 2) {
        tag_state_ = TagState::Absent;
        Serial.print(name_);
        Serial.println(": Tag detection failed");
        clearTagData();
      }
      break;
    case TagState::Confirmed:
      // more lenient/longer timeout for already established tags
      if (consecutive_fails_ >= config::TAG_PRESENCE_THRESHOLD) {
        tag_state_ = TagState::Departing;
        Serial.print(name_);
        Serial.println(": Tag removed");
        clearTagData();
      }
      break;
    case TagState::Departing:
      // confirm removal (a little more lenient on removal)
      if (now - last_seen_time_ > config::TAG_ABSENCE_TIMEOUT * 2) {
        tag_state_ = TagState::Absent;
        Serial.print(name_);
        Serial.println(": Tag removal confirmed");
      }
      break;
    default:
      break;
    }
  }
}

void Reader::printStatus() const {
  switch (tag_state_) {
  case TagState::Absent:
    Serial.println("No card");
    break;
  case TagState::Detecting:
    Serial.println("Detecting...");
    break;
  case TagState::Confirmed:
    Serial.println("Tag present");
    break;
  case TagState::Departing:
    Serial.println("Card removed (confirming...)");
    break;
  }
}

void Reader::clearTagData() {
  last_UID_length_ = 0;
  memset(last_UID_, 0, sizeof(last_UID_));
  tag_identified_ = false;
  read_attempts_ = 0;
  tag_ = {};
}

bool Reader::readTagData() {
  if (!reader_ok_ || tag_state_ != TagState::Confirmed)
    return false;

  Serial.print(name_);
  Serial.println(": Reading tag data...");

  byte buffer[18];
  byte bufferSize = sizeof(buffer);

  if (reader_.MIFARE_Read(config::TAG_START_READ_PAGE, buffer, &bufferSize) !=
      MFRC522::StatusCode::STATUS_OK) {
    Serial.print(name_);
    Serial.println(": Failed to read card data");
    // Don't clear tag data - we know tag is present, just couldn't read it
    return false;
  }

  // MIFARE_Read read some data into buffer, lets parse it and verify
  bool ok = jumper_cable_tag::parseTagData(buffer, tag_);
  if (!ok) {
    Serial.print(name_);
    Serial.println(": data read but incorrect");
    return false;
  }

  /*****************
   * DON'T HALT - let tag remain active for continuous detection
   * reader.PICC_HaltA();
   ******************/
  reader_.PCD_StopCrypto1();
  return ok;
}

// ========== UTILITY FUNCTIONS ==========
bool Reader::compareUID(byte *uid1, uint8_t len1, byte *uid2, uint8_t len2) {
  return (len1 == len2) && memcmp(uid1, uid2, len1) == 0;
}
