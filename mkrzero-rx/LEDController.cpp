#include "LEDController.h"
#include <Arduino.h>
#include <FastLED.h>

namespace led {
CRGB leds[config::NUM_LEDS];
}

void LEDController::initialize() {
  FastLED.addLeds<WS2815, config::LED_DATA_PIN, GRB>(led::leds, numLEDs);
  FastLED.setBrightness(255 * brightness / 100);

  // Startup color check
  fill_solid(led::leds, numLEDs, CRGB::Red);
  FastLED.show();
  delay(1000);
  fill_solid(led::leds, numLEDs, CRGB::Green);
  FastLED.show();
  delay(1000);
  fill_solid(led::leds, numLEDs, CRGB::Blue);
  FastLED.show();
  delay(1000);
  fill_solid(led::leds, numLEDs, CRGB::White);
  FastLED.show();
  delay(1000);
  fill_solid(led::leds, numLEDs, CRGB::Black);
  FastLED.show();
}

// ---------------------------------------------------------
// Default exhibit animation: soft slow shimmer
// ---------------------------------------------------------
void LEDController::animationDefault() {
  static uint8_t hueOffset = 0;
  unsigned long now = millis();
  if (now - lastFrameTime < frameIntervalMs)
    return; // ~25 FPS
  lastFrameTime = now;

  for (int i = 0; i < numLEDs; i++) {
    uint8_t hue = hueOffset + (i * 4); // slow hue gradient along strip
    uint8_t brightness = sin8(i * 5 + hueOffset * 2); // gentle pulsing
    led::leds[i] = CHSV(hue, 180, brightness);
  }

  hueOffset++; // slowly rotate color over time
  FastLED.show();
}

// ---------------------------------------------------------
// 6V animation: rising and falling red "energy" bar
// ---------------------------------------------------------
void LEDController::animation6V() {
  unsigned long now = millis();
  if (now - lastFrameTime < frameIntervalMs)
    return; // frame limiter
  lastFrameTime = now;

  fadeToBlackBy(led::leds, numLEDs, 30); // leave trails
  // Smooth ease motion: slower near top, faster near bottom
  float speed = (redBarDir == 1) ? 0.4 + (1.0 - redBarPos / numLEDs) * 0.8
                                 : 0.6 + (redBarPos / numLEDs) * 0.6;
  redBarPos += redBarDir * speed;

  // Direction reversal
  if (redBarPos >= numLEDs - 1) {
    redBarPos = numLEDs - 1;
    redBarDir = -1;
  } else if (redBarPos <= 0) {
    redBarPos = 0;
    redBarDir = 1;
  }

  int litCount = constrain((int)redBarPos, 0, numLEDs);

  for (int i = 0; i < litCount; i++) {
    uint8_t bright = map(i, 0, litCount - 1, 120, 255);
    led::leds[i] = CHSV(0, 255, bright);
  }

  // Subtle glow at the tip when rising
  if (redBarDir == 1 && litCount < numLEDs)
    led::leds[litCount] += CHSV(0, 200, 100);

  FastLED.show();
}

// ---------------------------------------------------------
// 12V animation: smooth electron flow (in green)
// ---------------------------------------------------------
void LEDController::animation12V() {
  unsigned long now = millis();
  if (now - lastFrameTime < frameIntervalMs)
    return;
  lastFrameTime = now;

  for (int i = 0; i < numLEDs; i++) {
    uint8_t wave = sin8(i * 10 + electronOffset);
    led::leds[i] = CHSV(96, 255, wave); // green hue
  }
  electronOffset += 4;

  FastLED.show();
}

// ---------------------------------------------------------
// 16V animation: blue sparks (arcing)
// ---------------------------------------------------------
void LEDController::animation16V() {
  unsigned long now = millis();
  if (now - lastFrameTime < frameIntervalMs)
    return;
  lastFrameTime = now;

  fadeToBlackBy(led::leds, numLEDs, 80);

  // Random bursts
  if (random8() < 40) {
    int sparks = random(1, 5);
    for (int i = 0; i < sparks; i++) {
      int pos = random(numLEDs);
      uint8_t hue = 160 + random8(10);
      uint8_t intensity = random8(180, 255);
      led::leds[pos] = CHSV(hue, 200, intensity);
    }
  }

  // Occasional bright arc flash
  if (random8() < 4) {
    int flashPos = random(numLEDs);
    for (int i = -2; i <= 2; i++) {
      int p = flashPos + i;
      if (p >= 0 && p < numLEDs)
        led::leds[p] += CHSV(180, 100, 255);
    }
  }

  // Dim background glow
  for (int i = 0; i < numLEDs; i++) {
    if (led::leds[i].getAverageLight() < 10)
      led::leds[i] = CRGB(0, 0, 5);
  }

  FastLED.show();
}

// ---------------------------------------------------------
// Wrong animation: to play for any other incorrect configuration
// ---------------------------------------------------------
void LEDController::animationWrong() {
  static uint8_t pulse = 0;
  static int8_t direction = 5;
  unsigned long now = millis();
  if (now - lastFrameTime < frameIntervalMs)
    return; // ~40 FPS
  lastFrameTime = now;

  // pulse brightness between 50–255
  pulse += direction;
  if (pulse >= 255 || pulse <= 50)
    direction = -direction;

  // flicker some pixels brighter for intensity
  for (int i = 0; i < numLEDs; i++) {
    if (random8() < 10) {
      led::leds[i] = CHSV(10, 255, 255); // bright orange flash
    } else {
      led::leds[i] = CHSV(0, 255, pulse); // deep red pulse
    }
  }

  FastLED.show();
}
