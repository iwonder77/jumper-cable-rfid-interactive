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
// ----- RS-485 COMM -----
static constexpr uint8_t RS485_DE_PIN = 5;
static constexpr uint8_t RS485_TX_ENABLE = HIGH;
static constexpr uint8_t RS485_RX_ENABLE = LOW;
static constexpr uint32_t RS485_BAUD_RATE = 9600;

// ----- PACKET FRAMING -----
static constexpr uint8_t PACKET_START1 = 0xAA;
static constexpr uint8_t PACKET_START2 = 0x55;
static constexpr unsigned long PACKET_READ_TIMEOUT_MS =
    100; // timeout between bytes while reading packet

// ----- LED / UI -----
static constexpr uint8_t ONBOARD_LED_PIN = 32;
static constexpr uint16_t LED_PULSE_MS = 200;

// ----- AUDIO -----
static constexpr const char AUDIO_SUCCESS_FILE[] = "success.wav";
static constexpr const char AUDIO_FAIL_FILE[] = "fail.wav";
static constexpr const char AUDIO_IDLE_FILE[] = "idle.wav";
static constexpr uint8_t AUDIO_VOLUME_PERCENT = 50;

} // namespace config
