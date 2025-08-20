#include "MuxController.h"
#include "Arduino.h"

void MuxController::selectChannel(uint8_t muxAddress, uint8_t channel) {
  // disable channel first
  Wire.beginTransmission(muxAddress);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(5);
  if (channel > 7)
    return;
  Wire.beginTransmission(muxAddress);
  Wire.write(1 << channel);
  Wire.endTransmission();

  delay(5);
}
void MuxController::disableChannel(uint8_t muxAddress) {
  Wire.beginTransmission(muxAddress);
  Wire.write(0);
  Wire.endTransmission();
  delay(5);
}
