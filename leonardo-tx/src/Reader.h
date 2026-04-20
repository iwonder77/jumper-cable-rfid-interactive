#pragma once

#include <Arduino.h>
#include <MFRC522Debug.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522v2.h>
#include <Wire.h>

#include "Config.h"
#include "JumperCableTag.h"
#include "MuxController.h"

enum class TagState : uint8_t {
  Absent,    // no tag present, waiting for first detection
  Detecting, // first detection, now must pass debounce
  Confirmed, // debounce passed, reading data
  Departing  // stopped seeing tag, confirming removal w/ debounce
};

// Reader class owns channel switching on mux
struct MuxGuard {
  uint8_t mux_addr;
  MuxGuard(uint8_t addr, uint8_t ch) : mux_addr(addr) {
    MuxController::selectChannel(addr, ch);
  }
  ~MuxGuard() { MuxController::disableChannel(mux_addr); }
};

class Reader {
public:
  Reader(const char *name, uint8_t mux_addr, uint8_t channel)
      : name_(name), channel_(channel), mux_addr_(mux_addr),
        driver_(config::WS1850S_I2C_ADDR, Wire), reader_(driver_) {};

  void init();
  void update();
  void printStatus() const;

  TagState getTagState() const { return tag_state_; };
  uint8_t getChannel() const { return channel_; }
  bool getReaderStatus() const { return reader_ok_; }
  const jumper_cable_tag::TagData &getTagData() const { return tag_; }

private:
  const char *name_;
  uint8_t channel_;
  uint8_t mux_addr_;
  MFRC522DriverI2C driver_;
  MFRC522 reader_;
  bool reader_ok_ = false;

  jumper_cable_tag::TagData tag_;
  bool tag_identified_ = false;
  uint8_t read_attempts_ = 0;

  TagState tag_state_ = TagState::Absent;
  uint32_t last_seen_time_ = 0;
  uint32_t first_seen_time_ = 0;

  uint8_t consecutive_fails_ = 0;
  uint8_t last_UID_[10]{};
  uint8_t last_UID_length_ = 0;

  void clearTagData();
  void readTagData();
  bool compareUID(uint8_t *uid1, uint8_t len1, uint8_t *uid2, uint8_t len2);
};
