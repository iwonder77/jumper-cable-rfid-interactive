/* 
* ----------------------------------------------
* PROJECT NAME: Better Battery Terminal Interactive w/ RFID 
* Description: Using more organized code and better principles in OOP to create a working and smooth version of the battery terminal interactive
*
* Author: Isai Sanchez 
* Date: 8-19-25 
* Board(s) Used: Arduino Nano 
* Libraries: 
*   - Wire.h (I2C communication library): https://docs.arduino.cc/language-reference/en/functions/communication/wire/ 
*   - MFRC522v2.h (Main RFID library): https://github.com/OSSLibraries/Arduino_MFRC522v2 
*
* Notes: 
*   - The M5Stack RFID2 readers we're using have a WS1850S chip rather than the MFRC522, so there are subtle differences that the library
*     doesn't play nice with, however reading the datasheet for the MFRC522 and the src code for the library seems to help and work out alright
*       sidenote: coudln't for the life of me figure out how to download the datasheet for these RFID readers from M5Stack, 
*       definitely will go with other options if we use RFID again
*   - Found through trial and error that the RFID2 boards have internal pull-up resistors for the SDA/SCL lines. So these were 
*     connected straight to the TCA9548A multiplexer's output channels (SD1/SC1 and SD2/SC2) without the use of an external pull up resistor
*   - Also found out that the SDA/SCL lines for the RFID2 readers are at 3.3V logic level, so the multiplexer was powered with Arduino's
*     3V3/GND pins
* ----------------------------------------------
*/

#include <Wire.h>
#include <MFRC522v2.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522Debug.h>

#include "Battery.h"
#include "MuxController.h"

enum LEDState {
  LED_OFF,
  LED_GREEN,
  LED_RED
};

// ----- I2C ADDRESSES -----
const uint8_t TCA9548A_ADDR = 0x70;
const uint8_t RFID2_WS1850S_ADDR = 0x28;

// ----- LED Test Pins -----
const uint8_t GREEN_LED_PIN = 5;
const uint8_t RED_LED_PIN = 6;

// ----- Timing Constants -----
const unsigned long TAG_POLL_INTERVAL = 250;  // how often to check for tags (ms)

// ----- HARDWARE INSTANCES -----
MFRC522DriverI2C driver{ RFID2_WS1850S_ADDR, Wire };
MFRC522 reader{ driver };

// ----- GLOBAL BATTERY INSTANCE -----
Battery battery(TCA9548A_ADDR, 1);

// ========== LED CONTROL ==========
void updateLEDs() {
  static LEDState lastState = LED_OFF;
  LEDState currentState;

  // Count terminals with present tags
  uint8_t presentCount = 0;
  if (battery.getPositive().getTagState() == TAG_PRESENT) presentCount++;
  if (battery.getNegative().getTagState() == TAG_PRESENT) presentCount++;

  if (presentCount < 2) {
    currentState = LED_OFF;
  } else if (battery.hasValidConfiguration()) {
    currentState = LED_GREEN;
  } else {
    currentState = LED_RED;
  }

  if (currentState != lastState) {
    switch (currentState) {
      case LED_OFF:
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        Serial.println("LEDs OFF - waiting for both tags");
        break;
      case LED_GREEN:
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        Serial.println("✅ Correct configuration - Green ON");
        break;
      case LED_RED:
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        Serial.println("❌ Incorrect configuration - Red ON");
        break;
    }

    battery.printStatus();
    lastState = currentState;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== Battery RFID System v3.0 ===");
  Serial.println("Features: Persistent tag detection with state machine");

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  Wire.begin();
  reader.PCD_Init();

  MuxController::disableAll(TCA9548A_ADDR);
  delay(50);

  battery.initializeReaders(reader);

  if (!battery.getPositive().getReaderStatus() && !battery.getNegative().getReaderStatus()) {
    Serial.println("ERROR: No readers responding! Check wiring.");
    while (1) {
      digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN));
      delay(500);
    }
  }

  if (!battery.getPositive().getReaderStatus()) {
    Serial.println("Warning: Positive Terminal reader not responding");
  }
  if (!battery.getNegative().getReaderStatus()) {
    Serial.println("Warning: Negative Terminal reader not responding");
  }

  Serial.println("\n=== System Ready ===");
  Serial.println("Place jumper cable tags on terminals to test");

  MuxController::disableAll(TCA9548A_ADDR);
}

// ========== MAIN LOOP ==========
void loop() {
  // Poll RFID readers at controlled intervals
  static unsigned long lastPollTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastPollTime >= TAG_POLL_INTERVAL) {
    // Update both terminals
    battery.updateReaders(reader);
    lastPollTime = currentTime;
  }

  updateLEDs();
  delay(10);
}
