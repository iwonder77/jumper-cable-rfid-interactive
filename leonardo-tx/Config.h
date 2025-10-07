#pragma once
/**
 * Config.h
 *
 * Centralized configuration constants used by the Wall Battery (Arduino
 * Leonardo) system firmware
 *
 * - make sure to `#include "Config.h"` then use config::XXXXX throughout the
 * projects for no magic numbers
 * - units are encoded in the name (e.g. _MS, _US)
 */

#include <Arduino.h>

namespace config {

// ----- SYSTEM CONSTANTS -----
static constexpr uint8_t NUM_BATTERIES = 3;
static constexpr uint16_t POLL_INTERVAL_MS =
    50; // how often to poll all battery readers for tags (ms)

// ----- LED PINS -----
static constexpr uint8_t GREEN_LED_PIN = 6;
static constexpr uint8_t RED_LED_PIN = 7;

// ----- RS-485 -----
static constexpr uint32_t RS485_BAUD_RATE = 9600;
static constexpr uint8_t RS485_DE_PIN = 5; // driver enable pin (DE/RE toggle)
static constexpr uint8_t RS485_TRANSMIT = HIGH;
static constexpr uint8_t RS485_RECEIVE = LOW;
static constexpr uint32_t RS485_LINE_SETTLE_MS = 5;

// ----- PACKET FRAMING -----
static constexpr uint8_t PACKET_START1 = 0xAA;
static constexpr uint8_t PACKET_START2 = 0x55;

// ----- I2C ADDRESSES -----
static constexpr uint8_t TCA9548A_6V_ADDR = 0x70;
static constexpr uint8_t TCA9548A_12V_ADDR = 0x71;
static constexpr uint8_t TCA9548A_16V_ADDR = 0x72;
static constexpr uint8_t RFID2_WS1850S_ADDR = 0x28;
static constexpr uint32_t I2C_CLOCK_SPEED = 100000; // 100kHz standard mode

// ----- TCA9548A I2C MUX Channels -----
const uint8_t NEGATIVE_TERMINAL_CHANNEL = 1;
const uint8_t POSITIVE_TERMINAL_CHANNEL = 2;
static constexpr uint32_t CHANNEL_SWITCH_SETTLE_MS = 5;

// ---------- RFID TAG/READER CONSTANTS ----------
static constexpr uint8_t READER_INIT_SETTLE_MS = 10;
static constexpr unsigned long TAG_DEBOUNCE_TIME =
    150; // debounce time for tag detection (ms) (3 * round robin polling
         // interval)
static constexpr unsigned long TAG_ABSENCE_TIMEOUT =
    450; // time before considering tag removed (ms) (3 * tag debounce)
static constexpr uint8_t TAG_PRESENCE_THRESHOLD =
    3; // consecutive reading fails before marking absent
static constexpr uint8_t TAG_START_READ_PAGE =
    4; // page # to begin reading data from in Tag

} // namespace config
