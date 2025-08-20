# Jumper Cable RFID Battery Interactive

## Overview

Source code for the Arduino Nano that handles RFID readings of 3 batteries (each with 2 readers under the positive and negative terminals) to determine if jumper cable polarity is correct and acts accordingly. RFID tags are embedded within the jumper cables, I wrote lighteweight data onto them using [this](https://github.com/iwonder77/rw-NTAG203-rfid-tag) sketch to allow us to differentiate between positive and negative jumper cable ends. 

## Hardware Components

### Electronics

-   Microcontroller: Arduino Nano
-   RFID Readers: 6 x RFID2 WS1850S M5Stack readers (fixed I2C address of 0x28) - 2 per battery
-   I2C Multiplexer: 3 x Adafruit's TCA9548A 8 Channel I2C Multiplexer (variable I2C address of 0x70, 0x71, and 0x72) - 1 per battery
-   Status LEDs: Green (correct polarity configuration) and Red (incorrect polarity configuration) indicators
-   RFID Tags: 4 x Adafruit's NTAG203 tags

### Hardware Architecture (Schematic)

...

### Software Architecture

#### Class Structure

1. **TerminalReader** Class
