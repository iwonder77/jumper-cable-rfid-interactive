#include "LEDCommander.h"
#include <Arduino.h>
#include <FastLED.h>

void LEDCommander::init() { sendCommand(config::CMD_DEFAULT_ANIMATION); }

bool LEDCommander::sendCommand(uint8_t cmd, uint8_t *params,
                               uint8_t paramCount) {
  Wire.beginTransmission(config::LED_CONTROLLER_ADDR);
  Wire.write(cmd);

  for (uint8_t i = 0; i < paramCount; i++) {
    Wire.write(params[i]);
  }

  uint8_t error = Wire.endTransmission();

  if (error != 0) {
    Serial.print("I2C Error: ");
    Serial.println(error);
    return false;
  }
  return true;
}
