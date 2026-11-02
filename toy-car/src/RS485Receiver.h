#pragma once
/**
 * RS485Receiver.h
 *
 * Non-blocking RS-485 receiver class. Parses framed packets and invokes a
 * callback when a valid packet has been received.
 *
 * Responsibilities:
 *   - Manage DE/RE pin (start in receive mode).
 *   - Read bytes off a HardwareSerial instance (Serial1).
 *   - Maintain small state-machine to re-sync on framing bytes and to handle
 *     inter-byte timeouts (to recover on errors).
 *   - Validate checksum using CommPacket::xorChecksum.
 *
 * Usage:
 *   RS485Receiver rs485(Serial1);
 *   rs485.begin(config::RS485_BAUD_RATE);
 *   rs485.setPacketHandler(myHandler, this);
 *   -> in loop(): rs485.update();
 */

#include "CommPacket.h"
#include "Config.h"
#include <Arduino.h>

class RS485Receiver {
public:
  // C-style callback: handler(pkt, context)
  using PacketHandlerFn = void (*)(const WallStatusPacket &, void *context);

  explicit RS485Receiver(HardwareSerial &serial,
                         uint8_t dePin = config::RS485_DE_PIN);
  void begin(uint32_t baud = config::RS485_BAUD_RATE);
  void setPacketHandler(PacketHandlerFn handler, void *ctx);
  void update(); // must be called often from loop()

private:
  enum RxState { WAIT_START1, WAIT_START2, READ_DATA };

  HardwareSerial &uart;
  uint8_t dePin;
  RxState rxState;
  uint8_t buffer[sizeof(WallStatusPacket)];
  uint8_t packetIndex;
  unsigned long lastByteMillis;
  PacketHandlerFn handler;
  void *handlerCtx;

  void resetState();
  void handleByte(uint8_t b);
};
