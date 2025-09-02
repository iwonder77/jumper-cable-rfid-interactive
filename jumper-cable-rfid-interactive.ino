/* 
* ----------------------------------------------
* PROJECT NAME: Jumper Cable RFID Interactive 
* Description: Using more organized code and better principles in OOP to create a working and clean version of the 
*              jumper cable + battery terminal interactive
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
const uint8_t TCA9548A_6V_ADDR = 0x70;
const uint8_t TCA9548A_12V_ADDR = 0x71;
const uint8_t TCA9548A_16V_ADDR = 0x72;
const uint8_t RFID2_WS1850S_ADDR = 0x28;

// ----- LED Test Pins -----
const uint8_t GREEN_LED_PIN = 2;
const uint8_t RED_LED_PIN = 3;

// ----- Timing Constants -----
const unsigned long TAG_POLL_INTERVAL = 100;  // how often to check for tags (ms)

// ----- HARDWARE INSTANCES -----
MFRC522DriverI2C driver{ RFID2_WS1850S_ADDR, Wire };
MFRC522 reader{ driver };

// ----- GLOBAL BATTERY ARRAY INSTANCE -----
const uint8_t NUM_BATTERIES = 3;
Battery batteries[NUM_BATTERIES] = { { TCA9548A_6V_ADDR, 0, RFID2_WS1850S_ADDR }, { TCA9548A_12V_ADDR, 1, RFID2_WS1850S_ADDR }, { TCA9548A_16V_ADDR, 2, RFID2_WS1850S_ADDR } };

// ========== LED CONTROL ==========
void updateLEDs() {
  static LEDState lastState = LED_OFF;
  LEDState currentState = LED_OFF;
  int activeBattery = -1;

  // Find the battery with the most complete configuration
  for (int i = 0; i < NUM_BATTERIES; i++) {
    bool posPresent = (batteries[i].getPositive().getTagState() == TAG_PRESENT);
    bool negPresent = (batteries[i].getNegative().getTagState() == TAG_PRESENT);

    if (posPresent && negPresent) {
      activeBattery = i;
      if (batteries[i].hasValidConfiguration()) {
        currentState = LED_GREEN;
        break;  // Found a valid one, stop looking
      } else {
        currentState = LED_RED;  // Both cables present but wrong config
      }
    } else if (posPresent || negPresent) {
      currentState = LED_OFF;
      activeBattery = i;
    }
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
        Serial.print("✅ ");
        Serial.print(batteries[activeBattery].getName());
        Serial.println(" battery ready for jumpstart!");
        break;
      case LED_RED:
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        Serial.print("❌ ");
        Serial.print(batteries[activeBattery].getName());
        Serial.println(" battery incorrect configuration");
        break;
    }

    if (activeBattery >= 0) {
      batteries[activeBattery].printBatteryStatus();
    }
    lastState = currentState;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== Jumper Cable Interactive v2 ===");

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  Wire.begin();
  Wire.setClock(100000);

  // disable all MUX channels initially
  for (int i = 0; i < NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries[i].getMuxAddr());
  }

  // initialize each battery
  Serial.println("\nInitializing batteries...");
  bool systemOK = true;

  for (int i = 0; i < NUM_BATTERIES; i++) {
    Serial.print("\n--- Initializing ");
    Serial.print(batteries[i].getName());
    Serial.println(" Battery ---");

    bool batteryOK = batteries[i].initialize(reader);
    batteries[i].printInitializationSummary();

    if (!batteryOK) {
      Serial.print("ERROR: ");
      Serial.print(batteries[i].getName());
      Serial.println(" battery initialization failed!");
      systemOK = false;
    }
  }

  // Handle system failure
  if (!systemOK) {
    Serial.println("\nSystem initialization failed! Check wiring.");
    while (1) {
      digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN));
      delay(500);
    }
  }

  // disable all channels
  for (int i = 0; i < NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries[i].getMuxAddr());
  }

  Serial.println("\n=== System Ready ===");
  Serial.println("Place jumper cable tags on terminals to test");
}

// ========== MAIN LOOP ==========
void loop() {
  // Round-robin approach to polling RFID readers at controlled intervals
  // (1 per loop cycle, we keep track of which battery to poll)
  static unsigned long lastPollTime = 0;
  static int currentBattery = 0;  // Track which battery to poll
  unsigned long currentTime = millis();

  if (currentTime - lastPollTime >= TAG_POLL_INTERVAL) {
    // Poll only one battery per cycle
    batteries[currentBattery].updateReaders(reader);

    // Move to next battery
    currentBattery = (currentBattery + 1) % NUM_BATTERIES;
    lastPollTime = currentTime;
  }

  updateLEDs();
  delay(10);  // Reduced delay since we're polling less frequently
}
