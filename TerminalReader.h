#include <Arduino.h>
#include <MFRC522Debug.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522v2.h>
#include <Wire.h>

const uint8_t RFID2_WS1850S_ADDR = 0x28;

// ===================== HARDWARE INSTANCES ====================
MFRC522DriverI2C driver{RFID2_WS1850S_ADDR, Wire};
MFRC522 reader{driver};
// =============================================================

enum TagState { TAG_ABSENT, TAG_DETECTED, TAG_PRESENT, TAG_REMOVED };

struct JumperCableTagData {
  char type[4];     // either "POS" or "NEG"
  uint8_t id;       // 1, 2, 3, or 4 (for the 4 cable ends)
  uint8_t checksum; // simple validation
};

class TerminalReader {
public:
  TerminalReader(const char *name, uint8_t channel)
      : name(name), channel(channel) {}

  void initialize();
  void update();
  void clearTagData();
  void readTagData();
  void printStatus() const;

  TagState getTagState() const { return tagState; }
  bool polarityOK() const { return isCorrectPolarity; }
  JumperCableTagData getTagData() const { return tagData; }

private:
  const char *name;
  uint8_t channel;
  bool isReaderOK = false;
  TagState tagState = TAG_ABSENT;
  unsigned long lastSeenTime = 0;
  unsigned long firstSeenTime = 0;
  uint8_t consecutiveFails = 0;
  bool isCorrectPolarity = false;
  JumperCableTagData tagData{};
  byte lastUID[10]{};
  byte lastUIDLength = 0;

  // Private helper
  bool compareUID(byte *uid1, byte *uid2, byte length) {
    return memcmp(uid1, uid2, length) == 0;
  }
};
