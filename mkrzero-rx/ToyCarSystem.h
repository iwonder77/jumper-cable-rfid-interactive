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

#include "AudioPlayer.h"
#include "CommPacket.h"
#include "RS485Receiver.h"
#include <Arduino.h>

class ToyCarSystem {
public:
  ToyCarSystem(HardwareSerial &serialPort);
  bool begin();
  void update(); // call frequently from loop()

private:
  RS485Receiver rs485;
  AudioPlayer audio;

  // LED helper
  void pulseLed(uint16_t durationMs);

  // packet callback glue
  // static means "not tied to any specific object"
  static void packetHandlerStatic(const WallStatusPacket &pkt, void *ctx);
  void onPacketReceived(const WallStatusPacket &pkt);
};
