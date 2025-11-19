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
static constexpr uint8_t RS485_DE_PIN = 6;
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
static constexpr const uint8_t SPUTTER_AUDIO_TRIGGER = 5;      // 6V
static constexpr const uint8_t ENGINE_START_AUDIO_TRIGGER = 4; // 12V
static constexpr const uint8_t ZAP_AUDIO_TRIGGER = 3;          // 16V
static constexpr const uint8_t WRONG_CHOICE_AUDIO_TRIGGER = 2;
static constexpr unsigned long PULSE_SEND_TIME_MS = 100;
// static constexpr uint8_t AUDIO_VOLUME_PERCENT = 50;

// ----- I2C Addresses -----
static constexpr uint8_t MUX_ADDR = 0x70;
static constexpr uint8_t RFID2_WS1850S_ADDR = 0x28;

// ----- TCA9548A MUX Channels -----
const uint8_t GND_FRAME_CHANNEL = 0;
const uint8_t NEGATIVE_TERMINAL_CHANNEL = 1;
const uint8_t POSITIVE_TERMINAL_CHANNEL = 2;
static constexpr uint32_t CHANNEL_SWITCH_SETTLE_MS = 5;

// ----- RFID TAG/READER CONSTANTS -----
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

// ----- LED DRIVER (rp2040) CONSTANTS -----
static constexpr uint8_t LED_CONTROLLER_ADDR = 0x20;
static constexpr uint8_t CMD_6V_ANIMATION = 0x01;
static constexpr uint8_t CMD_12V_ANIMATION = 0x02;
static constexpr uint8_t CMD_16V_ANIMATION = 0x03;
static constexpr uint8_t CMD_DEFAULT_ANIMATION = 0x04;
static constexpr uint8_t CMD_WRONG_ANIMATION = 0x05;

} // namespace config
