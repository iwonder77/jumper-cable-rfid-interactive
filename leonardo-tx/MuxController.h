#ifndef MUXCONTROLLER_H
#define MUXCONTROLLER_H

#include <Arduino.h>

class MuxController {
public:
  static void selectChannel(uint8_t muxAddress, uint8_t channel);
  static void disableChannel(uint8_t muxAddress);
};

#endif
