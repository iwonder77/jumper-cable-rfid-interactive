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
  }

  FastLED.show();
}

// ---------------------------------------------------------
// 6V animation: rising and falling red "energy" bar
// ---------------------------------------------------------
void LEDController::stepAnimation6V() {
  // clear the strip first
  fill_solid(led::leds, numLEDs, CRGB::Black);

  // how far up the strip the bar can travel
  const int maxPos = numLEDs / 2;
  float speed =
      (redBarDir == 1)
          ? 0.4f + (1.0f - redBarPos / float(numLEDs)) * 0.8f // slows near top
          : 0.6f + (redBarPos / float(numLEDs)) * 0.6f; // speeds near bottom
  redBarPos += redBarDir * speed;

  // update position
  redBarPos += redBarDir * speed;

  // reverse direction at limits
  if (redBarPos >= maxPos) {
    redBarPos = maxPos;
    redBarDir = -1;
  } else if (redBarPos <= 0) {
    redBarPos = 0;
    redBarDir = 1;
  }

  // bar grows from bottom (0) to current position
  int barTop = int(redBarPos);

  // light up the red bar from 0 to barTop
  for (int i = 0; i <= barTop; i++) {
    if (i < numLEDs) {
      led::leds[i] = CRGB::Red;
    }
  }

  // optionally add a subtle trailing glow at the top
  if (barTop + 1 < numLEDs) {
    led::leds[barTop + 1] = CRGB(64, 0, 0); // dim red glow
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
// Default animation
// ---------------------------------------------------------
void LEDController::stepAnimationDefault() {
  // Time-based wave (slow breathing pattern)
  float t = millis() / 2000.0f; // slow oscillation (~1 cycle every 2 seconds)
  uint8_t breath =
      (sin8(int(t * 255)) / 2) + 128; // 128–255 range for gentle breathing

  // Slowly shifting hue between orange and red
  uint8_t hue =
      8 + sin8(int(t * 80)) / 16; // oscillates slightly around red-orange

  // Fill the strip with a uniform warm tone
  fill_solid(led::leds, numLEDs, CHSV(hue, 200, breath));

  // Add a faint wandering highlight pixel for a bit of motion
  static uint8_t wander = 0;
  wander += 1;
  int pos = map(sin8(wander), 0, 255, 0, numLEDs - 1);
  led::leds[pos] += CHSV(hue - 8, 150, 255);

  // Gentle fade so the highlight blends naturally
  fadeToBlackBy(led::leds, numLEDs, 5);
}
