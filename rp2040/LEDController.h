#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "Config.h"
#include <Arduino.h>
#include <FastLED.h>

namespace led {
extern CRGB leds[config::NUM_LEDS];
}

class LEDController {
public:
  LEDController(uint8_t num = config::NUM_LEDS,
                uint8_t brightness = config::LED_BRIGHTNESS,
                uint16_t fps = config::FPS)
      : numLEDs(num), brightness(brightness), FPS(fps) {};
  void initialize();
  void update(uint8_t animationMode);

private:
  void stepAnimation6V();
  void stepAnimation12V();
  void stepAnimation16V();
  void stepAnimationDefault();

  uint8_t numLEDs;
  uint8_t brightness;

  // shared timing
  const uint16_t FPS;
  unsigned long lastFrameTime = 0;

  // animation states
  const int maxRedBarPos = numLEDs / 2;
  float redBarPos = 0.0f;
  int8_t redBarDir = 1;
  uint8_t electronOffset = 0;
};

#endif
