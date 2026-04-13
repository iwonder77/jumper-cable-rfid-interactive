#include "RS485Receiver.h"
#include "Config.h"
#include "Debug.h"

RS485Receiver::RS485Receiver(HardwareSerial &serial, uint8_t dePin)
    : uart(serial), dePin(dePin), rxState(WAIT_START1), packetIndex(0),
      lastByteMillis(0), handler(nullptr), handlerCtx(nullptr) {}

void RS485Receiver::begin(uint32_t baud) {
  uart.begin(baud);
  pinMode(dePin, OUTPUT);
  // ensure receiver mode
  digitalWrite(dePin, config::RS485_RX_ENABLE);

  DEBUG_PRINTLN("RS485Receiver initialized");
  resetState();
}

// register the call back function by invoking this method
void RS485Receiver::setPacketHandler(PacketHandlerFn h, void *ctx) {
  handler = h;
  handlerCtx = ctx;
}

void RS485Receiver::resetState() {
  rxState = WAIT_START1;
  packetIndex = 0;
  lastByteMillis = millis();
}

void RS485Receiver::handleByte(uint8_t b) {
  lastByteMillis = millis();

  switch (rxState) {
  case WAIT_START1:
    if (b == config::PACKET_START1) {
      buffer[0] = b;
      packetIndex = 1;
      rxState = WAIT_START2;
    } else {
      // stay in WAIT_START1
    }
    break;

  case WAIT_START2:
    if (b == config::PACKET_START2) {
      buffer[1] = b;
      packetIndex = 2;
      rxState = READ_DATA;
    } else {
      // bad second start byte -> resync
      rxState = WAIT_START1;
      packetIndex = 0;
    }
    break;

  case READ_DATA:
    buffer[packetIndex++] = b;
    if (packetIndex >= sizeof(WallStatusPacket)) {
      // full packet received
      WallStatusPacket pkt;
      memcpy(&pkt, buffer, sizeof(WallStatusPacket));

      // validate framing bytes just in case
      if (pkt.START1 != config::PACKET_START1 ||
          pkt.START2 != config::PACKET_START2) {
        DEBUG_PRINTLN("Framing bytes mismatch after full read");
      } else {
        // validate packet again with xor checksum check
        uint8_t expected = xorChecksum(pkt);
        if (expected == pkt.CHK) {
          // valid packet received here -> now call handler to "hand off" the
          // packet
          if (handler)
            handler(pkt, handlerCtx);
        } else {
          DEBUG_PRINT("Checksum invalid. expected=");
          DEBUG_PRINT(expected);
          DEBUG_PRINT(" got=");
          DEBUG_PRINTLN(pkt.CHK);
        }
      }
      // always reset for next packet
      resetState();
    }
    break;
  }
}

void RS485Receiver::update() {
  // timeout protection: if data stream stalled, reset state to avoid getting
  // stuck
  if ((millis() - lastByteMillis) > config::PACKET_READ_TIMEOUT_MS) {
    resetState();
  }

  // consume all available bytes while available
  while (uart.available()) {
    uint8_t b = static_cast<uint8_t>(uart.read());
    handleByte(b);
  }
}
