#pragma once
/**
 * CommPacket.h
 *
 * Defines the WallStatusPacket used by the Leonardo (sender) and the MKRZero
 * (receiver). Centralized here so both sides use identical framing & checksum.
 *
 * NOTE: Keep this identical to the MKRZero's packet definition. I tried making
 * this a global file for both projects but that was a little difficult to do
 */

#include <Arduino.h>

struct __attribute__((packed)) WallStatusPacket {
  uint8_t START1;
  uint8_t START2;
  uint8_t BAT_ID;
  uint8_t NEG_PRESENT;
  uint8_t NEG_STATE;
  uint8_t POS_PRESENT;
  uint8_t POS_STATE;
  uint8_t CHK;
};

static_assert(sizeof(WallStatusPacket) == 8,
              "WallStatusPacket must be 8 bytes (packed)");

// XOR checksum function used by sender & receiver (centralized here)
inline uint8_t xorChecksum(const WallStatusPacket &pkt) {
  uint8_t sum = 0;
  sum ^= pkt.BAT_ID;
  sum ^= pkt.NEG_PRESENT;
  sum ^= pkt.NEG_STATE;
  sum ^= pkt.POS_PRESENT;
  sum ^= pkt.POS_STATE;
  return sum;
}
