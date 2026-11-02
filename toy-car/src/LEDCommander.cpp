#include "LEDCommander.h"
#include "Debug.h"
#include <Arduino.h>
#include <FastLED.h>

void LEDCommander::init() { sendCommand(config::CMD_DEFAULT_ANIMATION); }

bool LEDCommander::sendCommand(uint8_t cmd, uint8_t *params,
                               uint8_t paramCount) {
  DEBUG_PRINTLN("Sending LED command to RP2040");

  Wire.beginTransmission(config::LED_CONTROLLER_ADDR);
  Wire.write(cmd);

  for (uint8_t i = 0; i < paramCount; i++) {
    Wire.write(params[i]);
  }

  uint8_t error = Wire.endTransmission();

  DEBUG_PRINTLN("I²C result: ");
  switch (error) {
  case 0:
    DEBUG_PRINTLN("SUCCESS");
    return true;
  case 1:
    DEBUG_PRINTLN("ERROR: Data too long");
    break;
  case 2:
    DEBUG_PRINTLN("ERROR: NACK on address"); // ← Most likely issue
    break;
  case 3:
    DEBUG_PRINTLN("ERROR: NACK on data");
    break;
  case 4:
    DEBUG_PRINTLN("ERROR: Other");
    break;
  default:
    DEBUG_PRINT("ERROR: Unknown (");
    DEBUG_PRINT(error);
    DEBUG_PRINTLN(")");
  }
  return false;
}
