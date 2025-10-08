#pragma once
/**
 * ToyCarSystem.h
 *
 * High-level system for the toy car. Responsible for:
 *  - Wiring RS485Receiver to a packet handler
 *  - Translating WallStatusPacket into actions (audio cues, LED feedback)
 *  - Exposing begin() and update() entry points for the main sketch
 *
 * Keep logic here focused on 'what happens when a packet arrives' — not on
 * low-level parsing or audio details.
 */

#include <Arduino.h>
#include <MFRC522v2.h>

#include "AudioPlayer.h"
#include "CommPacket.h"
#include "LEDController.h"
#include "RS485Receiver.h"
#include "TerminalReader.h"

struct BatteryState {
  uint8_t id;
  bool posPresent;
  bool negPresent;
  bool posPolarity;
  bool negPolarity;

  // equality operator for easy comparison (used when checking previous vs
  // current state)
  bool operator!=(const BatteryState &other) const {
    return posPresent != other.posPresent || negPresent != other.negPresent ||
           posPolarity != other.posPolarity || negPolarity != other.negPolarity;
  }
  bool successfulConnection() const {
    return (posPresent && posPolarity) && (negPresent && negPolarity);
  }
};

struct TerminalState {
  bool posPresent;
  bool negPresent;
  bool framePresent;
  bool posPolarity;
  bool negPolarity;
  bool framePolarity;

  // equality operator for easy comparison (used when checking previous vs
  // current state)
  bool operator!=(const TerminalState &other) const {
    return posPresent != other.posPresent || negPresent != other.negPresent ||
           framePresent != other.framePresent ||
           posPolarity != other.posPolarity ||
           negPolarity != other.negPolarity ||
           framePolarity != other.framePolarity;
  }
};

class ToyCarSystem {
public:
  ToyCarSystem(HardwareSerial &serialPort);
  bool initialize(MFRC522 &reader);
  void update(MFRC522 &reader); // call frequently from loop()

private:
  bool muxCommunicationOK;
  uint8_t muxAddr;
  uint8_t id;
  TerminalReader positive;
  TerminalReader negative;
  TerminalReader gnd_frame;
  RS485Receiver rs485;
  AudioPlayer audio;
  LEDController ledController;
  TerminalState prevToyCarTerminalState;
  BatteryState prevWallBatteryState;
  TerminalState toyCarTerminalState;
  BatteryState wallBatteryState;

  // state helper
  TerminalState getCurrentState() const;

  // LED helper
  void pulseLed(uint16_t durationMs);

  // packet callback glue
  // static means "not tied to any specific object"
  static void packetHandlerStatic(const WallStatusPacket &pkt, void *ctx);
  // actual member callback function
  void onPacketReceived(const WallStatusPacket &pkt);
};
