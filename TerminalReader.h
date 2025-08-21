#ifndef TERMINALREADER_H
#define TERMINALREADER_H

#include <Arduino.h>
#include <MFRC522Debug.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522v2.h>

#include <Wire.h>

enum TagState { TAG_ABSENT, TAG_DETECTED, TAG_PRESENT, TAG_REMOVED };

struct JumperCableTagData {
  char type[4];     // either "POS" or "NEG"
  uint8_t id;       // 1, 2, 3, or 4 (for the 4 cable ends)
  uint8_t checksum; // simple validation
};

class TerminalReader {
public:
  TerminalReader(uint8_t address, const char *name, uint8_t channel)
      : address(address), name(name), channel(channel) {}

  void initialize(MFRC522 &reader);
  void update(MFRC522 &reader);
  void printStatus() const;

  TagState getTagState() const { return tagState; }
  JumperCableTagData getTagData() const { return tagData; }
  uint8_t getChannel() const { return channel; }
  bool getReaderStatus() const { return isReaderOK; }
  bool polarityOK() const { return isCorrectPolarity; }

private:
  const char *name;
  uint8_t address;
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

  void clearTagData();
  void readTagData(MFRC522 &reader);
  uint8_t calculateChecksum(const uint8_t *data, uint8_t length);
  bool compareUID(byte *uid1, byte *uid2, byte length);

  // ---------- CONSTANTS ----------
  static constexpr unsigned long TAG_DEBOUNCE_TIME =
      50; // debounce time for tag detection (ms)
  static constexpr unsigned long TAG_ABSENCE_TIMEOUT =
      500; // time before considering tag removed (ms)
  static constexpr uint8_t TAG_PRESENCE_THRESHOLD =
      3; // consecutive reading fails before marking absent
  static constexpr uint8_t TAG_START_READ_PAGE =
      4; // page # to begin reading data from in Tag
};

#endif
