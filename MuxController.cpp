#include "MuxController.h"
#include "Arduino.h"

void MuxController::selectChannel(uint8_t muxAddress, uint8_t channel) {
  if (channel > 7)
    return;
  Wire.beginTransmission(muxAddress);
  Wire.write(1 << channel);
  Wire.endTransmission();

  delayMicroseconds(5);
}
void MuxController::disableAll(uint8_t muxAddress) {
  Wire.beginTransmission(muxAddress);
  Wire.write(0);
  Wire.endTransmission();
}
