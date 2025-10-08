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
                uint8_t brightness = config::LED_BRIGHTNESS)
      : numLEDs(num), brightness(brightness) {};
  void initialize();
  void animationDefault();
  void animation6V();
  void animation12V();
  void animation16V();
  void animationWrong();

private:
  uint8_t numLEDs;
  uint8_t brightness;

  // shared timing
  unsigned long lastFrameTime = 0;
  const uint16_t frameIntervalMs = config::ANIMATION_FRAME_INTERVAL_MS;

  // animation states
  float redBarPos = 0;
  uint8_t redBarDir = 1;
  uint8_t electronOffset = 0;
  unsigned long lastSparkTime = 0;
};

#endif
