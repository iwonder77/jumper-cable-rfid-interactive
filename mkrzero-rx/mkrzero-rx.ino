/**
 * main.ino
 *
 * Integrates RS-485 receiver + audio player in a single MKRZero sketch.
 * Usage:
 *   - Connect RS485 transceiver to Serial1 + DE/RE pin (config::RS485_DE_PIN).
 *   - Insert SD card with success.wav and fail.wav (or change filenames in Config.h).
 *   - Power, open Serial console for debug logs.
 */

#include "ToyCarSystem.h"
#include "Debug.h"

ToyCarSystem toyCar(Serial1);

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  DEBUG_PRINTLN("Toy Car MKRZero starting...");
  toyCar.begin();
}

void loop() {
  toyCar.update();
  // keep loop free for quick responses (no heavy blocking)
}
