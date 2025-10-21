#include "LEDController.h"
#include "Config.h"
#include <Arduino.h>
#include <FastLED.h>

namespace led {
CRGB leds[config::NUM_LEDS];
}

void LEDController::initialize() {
  FastLED.addLeds<WS2815, config::LED_DATA_PIN, GRB>(led::leds, numLEDs);
  FastLED.setBrightness((255UL * brightness) / 100UL);

  // brief startup sequence
  fill_solid(led::leds, numLEDs, CRGB::Red);
  FastLED.show();
  delay(200);
  fill_solid(led::leds, numLEDs, CRGB::Green);
  FastLED.show();
  delay(200);
  fill_solid(led::leds, numLEDs, CRGB::Blue);
  FastLED.show();
  delay(200);
  fill_solid(led::leds, numLEDs, CRGB::White);
  FastLED.show();
  delay(200);
  fill_solid(led::leds, numLEDs, CRGB::Black);
  FastLED.show();

  randomSeed(millis());
}

void LEDController::update(uint8_t animationMode) {
  unsigned long now = millis();
  if (now - lastFrameTime < (1000 / FPS)) {
    return;
  };
  lastFrameTime = now;

  switch (animationMode) {
  case config::CMD_6V_ANIMATION:
    stepAnimation6V();
    break;
  case config::CMD_12V_ANIMATION:
    stepAnimation12V();
    break;
  case config::CMD_16V_ANIMATION:
    stepAnimation16V();
    break;
  case config::CMD_DEFAULT_ANIMATION:
    stepAnimationDefault();
    break;
  case config::CMD_WRONG_ANIMATION:
    stepAnimationWrong();
    break;
  }

  FastLED.show();
}

// ---------------------------------------------------------
// 6V animation: rising and falling red "energy" bar
// ---------------------------------------------------------
void LEDController::stepAnimation6V() {
  fill_solid(led::leds, config::NUM_LEDS, CRGB::Black);

  const int maxPos = config::NUM_LEDS / 2;
  float speed = (redBarDir == 1)
                    ? 0.1f + (1.0f - redBarPos / float(config::NUM_LEDS)) *
                                 0.2f // slows near top
                    : 0.3f + (redBarPos / float(config::NUM_LEDS)) *
                                 0.4f; // speeds near bottom

  redBarPos += redBarDir * speed;

  if (redBarPos >= maxPos) {
    redBarPos = maxPos;
    redBarDir = -1;
  } else if (redBarPos <= 0) {
    redBarPos = 0;
    redBarDir = 1;
  }

  int barTop = int(redBarPos);
  for (int i = 0; i <= barTop && i < config::NUM_LEDS; i++) {
    led::leds[i] = CRGB::Red;
  }

  if (barTop + 1 < config::NUM_LEDS) {
    led::leds[barTop + 1] = CRGB(64, 0, 0);
  }
}

// ---------------------------------------------------------
// 12V animation: smooth electron flow (in green)
// ---------------------------------------------------------
void LEDController::stepAnimation12V() {
  for (int i = 0; i < numLEDs; i++) {
    uint8_t wave = sin8(i * 10 + electronOffset);
    led::leds[i] = CHSV(96, 255, wave); // green hue
  }
  electronOffset += 4;
}

// ---------------------------------------------------------
// 16V animation: blue sparks (arcing)
// ---------------------------------------------------------
void LEDController::stepAnimation16V() {
  fadeToBlackBy(led::leds, numLEDs, 80);

  // random bursts
  if (random8() < 40) {
    int sparks = random(1, 5);
    for (int i = 0; i < sparks; i++) {
      int pos = random(numLEDs);
      uint8_t hue = 160 + random8(10);
      uint8_t intensity = random8(180, 255);
      led::leds[pos] = CHSV(hue, 200, intensity);
    }
  }

  // occasional bright arc flash
  if (random8() < 4) {
    int flashPos = random(numLEDs);
    for (int i = -2; i <= 2; i++) {
      int p = flashPos + i;
      if (p >= 0 && p < numLEDs)
        led::leds[p] += CHSV(180, 100, 255);
    }
  }

  // dim background glow
  for (int i = 0; i < numLEDs; i++) {
    if (led::leds[i].getAverageLight() < 10)
      led::leds[i] = CRGB(0, 0, 5);
  }
}

// ---------------------------------------------------------
// Wrong animation: full strip glows red
// ---------------------------------------------------------
void LEDController::stepAnimationWrong() {
  unsigned long now = millis();

  // breathing effect using sine wave
  // adjust the divisor (100.0) to control breathing speed: higher = slower
  float breathe = (sin8(now / 200.0 * PI) + 1.0) / 2.0; // 0.0 to 1.0

  // map to brightness range (50-255 for a nice glow, never fully off)
  uint8_t brightness = 50 + (breathe * 205);

  // Fill strip with red at calculated brightness
  fill_solid(led::leds, config::NUM_LEDS, CRGB::Red);
  fadeToBlackBy(led::leds, config::NUM_LEDS, 255 - brightness);

  FastLED.show();
}

// ---------------------------------------------------------
// Default animation
// ---------------------------------------------------------
void LEDController::stepAnimationDefault() {
  unsigned long now = millis();

  // clear green strip in default mode
  fill_solid(led::leds, config::NUM_LEDS, CRGB::Black);

  // rainbow effect on red strip
  fill_solid(led::leds, config::NUM_LEDS, CHSV(hue, 80, 180));
  hue += 0.5; // Increment hue for rainbow cycling

  FastLED.show();
}
