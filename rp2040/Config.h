#pragma once
/**
 * Config.h
 *
 * Centralized configuration constants used by the Toy Car (MKRZero) system
 * firmware
 *
 * - make sure to `#include "Config.h"` then use config::XXXXX throughout the
 * projects for no magic numbers
 * - units are encoded in the name (e.g. _MS, _US)
 */

#include <Arduino.h>

namespace config {
static constexpr uint16_t NUM_LEDS = 56;
static constexpr uint8_t LED_DATA_PIN = D1;
static constexpr uint16_t LED_BRIGHTNESS = 10; // as a percentage (%)
static constexpr unsigned long ANIMATION_DURATION_MS = 5000; // 5 seconds
static constexpr uint16_t FPS = 60; // target frames per second of animation

static constexpr uint8_t LED_CONTROLLER_ADDR = 0x20;

static constexpr uint8_t CMD_6V_ANIMATION = 0x01;
static constexpr uint8_t CMD_12V_ANIMATION = 0x02;
static constexpr uint8_t CMD_16V_ANIMATION = 0x03;
static constexpr uint8_t CMD_DEFAULT_ANIMATION = 0x04;

} // namespace config
