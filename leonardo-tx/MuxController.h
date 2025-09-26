#ifndef MUXCONTROLLER_H
#define MUXCONTROLLER_H

#include <Arduino.h>

#include "Config.h"
#include "Wire.h"

class MuxController {
public:
  static void selectChannel(uint8_t muxAddress, uint8_t channel) {
    if (channel > 7)
      return;
    Wire.beginTransmission(muxAddress);
    Wire.write(1 << channel);
    Wire.endTransmission();

    delay(config::CHANNEL_SWITCH_SETTLE_MS);
  }
  static void disableChannel(uint8_t muxAddress) {
    Wire.beginTransmission(muxAddress);
    Wire.write(0);
    Wire.endTransmission();

    delay(config::CHANNEL_SWITCH_SETTLE_MS);
  }
};

#endif
