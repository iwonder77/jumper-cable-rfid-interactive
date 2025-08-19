#include <Arduino.h>

const uint8_t TCA9548A_ADDR = 0x70;

class MuxController {
public:
  static void selectChannel(uint8_t muxAddress, uint8_t channel);
  static void disableAll();
};
