#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "Config.h"
#include <Arduino.h>
#include <Wire.h>

class LEDCommander {
public:
  void init();
  bool sendCommand(uint8_t cmd, uint8_t *params = nullptr,
                   uint8_t paramCount = 0);
};

#endif
