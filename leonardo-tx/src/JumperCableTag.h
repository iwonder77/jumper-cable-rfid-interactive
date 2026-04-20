#pragma once

#include <Arduino.h>

namespace jumper_cable_tag {

/*
enum class CableEnd : uint8_t {
  Negative = 0,
  None = 2,
  Positive = 1
};
*/

struct TagData {
  char type[4];     // raw bytes from tag: "POS\0" or "NEG\0"
  uint8_t id;       // cable end identifier: 1–4
  uint8_t checksum; // XOR checksum stored on tag
  // CableEnd end;     // derived from type during parseTagData()
};

inline uint8_t calculateChecksum(const uint8_t *data, uint8_t length) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum ^= data[i];
  }
  return sum;
}

/**
 * Parse tag data from a raw byte buffer.
 * Validates field type and checksum.
 *
 * The TagData struct `out` is always populated via memcpy, even if buffer data
 * doesn't match, so it is up to the caller to inspect individual fields for
 * diagnostics.
 */
inline bool parseTagData(const byte *buffer, TagData &out) {
  memcpy(&out, buffer, sizeof(TagData));

  // validate type field is a known value before accepting
  if (strncmp(out.type, "POS", 3) != 0 && strncmp(out.type, "NEG", 3) != 0)
    return false;

  // validate checksum
  uint8_t expected = calculateChecksum((uint8_t *)&out, sizeof(out) - 1);

  return expected == out.checksum;
}

inline const char *typeToString(const TagData &tag) {
  if (strncmp(tag.type, "POS", 3) == 0)
    return "POS";
  if (strncmp(tag.type, "NEG", 3) == 0)
    return "NEG";
  return "???";
}

} // namespace jumper_cable_tag
