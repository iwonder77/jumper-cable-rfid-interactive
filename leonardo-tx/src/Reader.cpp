#include "Reader.h"
#include "Config.h"
#include "Debug.h"
#include "JumperCableTag.h"

// NOTE: must call Wire.begin() in setup() before we initialize reader!
void Reader::init() {
  // a guard is used to ensure channel is disabled when this guy goes out of
  // scope (see struct in Reader.h)
  MuxGuard guard(mux_addr_, channel_);

  DEBUG_PRINT("Testing ");
  DEBUG_PRINT(name_);
  DEBUG_PRINT(" Reader I2C Communication on Channel ");
  DEBUG_PRINT(channel_);
  DEBUG_PRINT(": ");

  Wire.beginTransmission(config::WS1850S_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    DEBUG_PRINTLN("FAILED - I2C Communication ERROR");
    reader_ok_ = false;
    return;
  }

  DEBUG_PRINTLN(" SUCCESS");
  reader_ok_ = true;

  // initialize hardware instance
  reader_.PCD_Init();

  delay(config::READER_INIT_SETTLE_MS);
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
    bool is_same_tag = (last_uid_length_ == reader_.uid.size) &&
                       compareUid(last_uid_, last_uid_length_,
                                  reader_.uid.uidByte, reader_.uid.size);

    // update UID
    memcpy(last_uid_, reader_.uid.uidByte, reader_.uid.size);
    last_uid_length_ = reader_.uid.size;

    // update timing and error
    last_seen_time_ = now;
    consecutive_fails_ = 0;

    // transition tagState now that a tag has been detected
    switch (tag_state_) {
    case TagState::Absent:
      tag_state_ = TagState::Detecting;
      first_seen_time_ = now; // start debounce timer
      DEBUG_PRINT(name_);
      DEBUG_PRINTLN(": New tag detected!");
      break;
    case TagState::Detecting:
      // check debounce timer in case we have a false positive detection
      // this debounce check ensures tag is actually present
      if (now - first_seen_time_ > config::TAG_DEBOUNCE_TIME) {
        tag_state_ = TagState::Confirmed;
        tag_identified_ = false;
        read_attempts_ = 0;
        DEBUG_PRINT(name_);
        DEBUG_PRINTLN(
            ": Tag confirmed present, transitioning to read its data");
      }
      break;
    case TagState::Confirmed:
      // 1. Handle tag swap first
      if (!is_same_tag) {
        // different tag detected
        tag_state_ = TagState::Detecting;
        first_seen_time_ = now;
        DEBUG_PRINT(name_);
        DEBUG_PRINTLN(": Different tag detected, clearing previous data");
        clearTagData();
        tag_identified_ = false;
        read_attempts_ = 0;
        break; // exit the switch - don't try to read a tag we haven't confirmed
      }
      // 2. Attempt tag identification
      // we can assume the same tag is still here, if we haven't identified it
      // yet, try reading
      if (!tag_identified_ && read_attempts_ < config::MAX_READ_ATTEMPTS) {
        if (readTagData()) {
          // parse succeeded - we must know what this piece is now
          tag_identified_ = true;
          DEBUG_PRINT(name_);
          DEBUG_PRINT(": Identified piece as ");
          DEBUG_PRINTLN(jumper_cable_tag::typeToString(tag_));
          DEBUG_PRINT(" #");
          DEBUG_PRINTLN(tag_.id);
        } else {
          // parse failed - count read attempts here
          read_attempts_++;

          if (read_attempts_ >= config::MAX_READ_ATTEMPTS) {
            DEBUG_PRINT(name_);
            DEBUG_PRINTLN(": Tag present but unreadable after max attempts");
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
      DEBUG_PRINT(name_);
      DEBUG_PRINTLN(": Tag returned!");
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
        DEBUG_PRINT(name_);
        DEBUG_PRINTLN(": Tag detection failed");
        clearTagData();
      }
      break;
    case TagState::Confirmed:
      // more lenient/longer timeout for already established tags
      if (consecutive_fails_ >= config::TAG_PRESENCE_THRESHOLD) {
        tag_state_ = TagState::Departing;
        DEBUG_PRINT(name_);
        DEBUG_PRINTLN(": Tag removed");
        clearTagData();
      }
      break;
    case TagState::Departing:
      // confirm removal (a little more lenient on removal)
      if (now - last_seen_time_ > config::TAG_ABSENCE_TIMEOUT * 2) {
        tag_state_ = TagState::Absent;
        DEBUG_PRINT(name_);
        DEBUG_PRINTLN(": Tag removal confirmed");
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
    DEBUG_PRINTLN("No card");
    break;
  case TagState::Detecting:
    DEBUG_PRINTLN("Detecting...");
    break;
  case TagState::Confirmed:
    DEBUG_PRINTLN("Tag present");
    break;
  case TagState::Departing:
    DEBUG_PRINTLN("Card removed (confirming...)");
    break;
  }
}

void Reader::clearTagData() {
  last_uid_length_ = 0;
  memset(last_uid_, 0, sizeof(last_uid_));
  tag_identified_ = false;
  read_attempts_ = 0;
  tag_ = {};
}

bool Reader::readTagData() {
  if (!reader_ok_ || tag_state_ != TagState::Confirmed)
    return false;

  DEBUG_PRINT(name_);
  DEBUG_PRINTLN(": Reading tag data...");

  byte buffer[18];
  byte buffer_size = sizeof(buffer);

  if (reader_.MIFARE_Read(config::TAG_START_READ_PAGE, buffer, &buffer_size) !=
      MFRC522::StatusCode::STATUS_OK) {
    DEBUG_PRINT(name_);
    DEBUG_PRINTLN(": Failed to read card data");
    // Don't clear tag data - we know tag is present, just couldn't read it
    return false;
  }

  // MIFARE_Read read some data into buffer, lets parse it and verify
  bool ok = jumper_cable_tag::parseTagData(buffer, tag_);
  if (!ok) {
    DEBUG_PRINT(name_);
    DEBUG_PRINTLN(": data read but incorrect");
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
bool Reader::compareUid(byte *uid1, uint8_t len1, byte *uid2, uint8_t len2) {
  return (len1 == len2) && memcmp(uid1, uid2, len1) == 0;
}
