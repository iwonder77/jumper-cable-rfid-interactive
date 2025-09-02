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
*     3V3/GND pins, and since the Arduino's SDA/SCL lines are at 5V logic level, a level converter was used to communicate between 
*     Arduino <---> RFID2 Readers
* ----------------------------------------------
*/

#include <Wire.h>
#include <MFRC522v2.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522Debug.h>
#include <SoftwareSerial.h>

#include "Battery.h"
#include "MuxController.h"
#include "Debug.h"

#define RS485_IO 5  // RS485 transmit/receive status (RE/DE) Pin (RE/DE: receive/data enable - these two are common together)
#define RS485_RX 6  // RS485 receive (RO) Pin (RO: Receive Out)
#define RS485_TX 7  // RS456 transmit (DI) Pin (DI: Data In)
#define RS485_TRANSMIT HIGH
#define BAUD_RATE 9600

// RS485 object instance
SoftwareSerial RS485(RS485_RX, RS485_TX);

enum LEDState {
  LED_OFF,
  LED_GREEN,
  LED_RED
};

struct BatteryState {
  bool posPresent;
  bool negPresent;
  uint8_t posPolarity;
  uint8_t negPolarity;
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
BatteryState lastStates[NUM_BATTERIES];

// ----- WALL STATUS DATA PACKET -----
// data packet being sent to toy car, force packing to avoid struct padding
// only sent when both terminals of any battery detect a tag
struct __attribute__((packed)) WallStatusPacket {
  uint8_t START1;
  uint8_t START2;
  uint8_t BAT_ID;
  uint8_t NEG_STATE;
  uint8_t POS_STATE;
  uint8_t CHK;
};

uint8_t calculateChecksum(const WallStatusPacket &pkt) {
  uint8_t sum = 0;
  sum ^= pkt.BAT_ID;
  sum ^= pkt.NEG_STATE;
  sum ^= pkt.POS_STATE;
  return sum;
}

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
        DEBUG_PRINTLN("LEDs OFF - waiting for both tags");
        break;
      case LED_GREEN:
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        DEBUG_PRINT("✅ ");
        DEBUG_PRINT(batteries[activeBattery].getName());
        DEBUG_PRINTLN(" battery ready for jumpstart!");
        break;
      case LED_RED:
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        DEBUG_PRINT("❌ ");
        DEBUG_PRINT(batteries[activeBattery].getName());
        DEBUG_PRINTLN(" battery incorrect configuration");
        break;
    }

    if (activeBattery >= 0) {
      batteries[activeBattery].printBatteryStatus();
    }
    lastState = currentState;
  }
}

void updatePacket() {
  for (int i = 0; i < NUM_BATTERIES; i++) {
    bool posPresent = (batteries[i].getPositive().getTagState() == TAG_PRESENT);
    bool negPresent = (batteries[i].getNegative().getTagState() == TAG_PRESENT);
    uint8_t posPolarity = batteries[i].getPositive().polarityOK();
    uint8_t negPolarity = batteries[i].getNegative().polarityOK();

    // Check for any change in state
    if (posPresent != lastStates[i].posPresent || negPresent != lastStates[i].negPresent || posPolarity != lastStates[i].posPolarity || negPolarity != lastStates[i].negPolarity) {

      // Update the stored state
      lastStates[i].posPresent = posPresent;
      lastStates[i].negPresent = negPresent;
      lastStates[i].posPolarity = posPolarity;
      lastStates[i].negPolarity = negPolarity;

      // Only send if both tags present
      if (posPresent && negPresent) {
        WallStatusPacket packet = { 0 };
        packet.START1 = 0xAA;
        packet.START2 = 0x55;
        packet.BAT_ID = batteries[i].getId();
        packet.NEG_STATE = negPolarity;
        packet.POS_STATE = posPolarity;
        packet.CHK = calculateChecksum(packet);

        RS485.write((uint8_t *)&packet, sizeof(WallStatusPacket));
        RS485.flush();

        DEBUG_PRINT("📤 Packet sent for battery ");
        DEBUG_PRINTLN(batteries[i].getName());
      }
    }
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  DEBUG_PRINTLN("=== Jumper Cable Interactive v2 ===");

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  pinMode(RS485_IO, OUTPUT);

  // set RS485 device to TRANSMIT mode at startup
  digitalWrite(RS485_IO, RS485_TRANSMIT);

  // set the baud rate
  // the longer the wire the slower you should set the transmission rate
  // anything here (300, 1200, 2400, 14400, 19200, etc) MUST BE THE SAME
  // AS THE SENDER UNIT
  RS485.begin(BAUD_RATE);

  Wire.begin();
  Wire.setClock(100000);

  for (int i = 0; i < NUM_BATTERIES; i++) {
    lastStates[i] = { false, false, 0, 0 };
  }

  // disable all MUX channels initially
  for (int i = 0; i < NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries[i].getMuxAddr());
  }

  // initialize each battery
  DEBUG_PRINTLN("\nInitializing batteries...");
  bool systemOK = true;

  for (int i = 0; i < NUM_BATTERIES; i++) {
    DEBUG_PRINT("\n--- Initializing ");
    DEBUG_PRINT(batteries[i].getName());
    DEBUG_PRINTLN(" Battery ---");

    bool batteryOK = batteries[i].initialize(reader);
    batteries[i].printInitializationSummary();

    if (!batteryOK) {
      DEBUG_PRINT("ERROR: ");
      DEBUG_PRINT(batteries[i].getName());
      DEBUG_PRINTLN(" battery initialization failed!");
      systemOK = false;
    }
  }

  // Handle system failure
  if (!systemOK) {
    DEBUG_PRINTLN("\nSystem initialization failed! Check wiring.");
    while (1) {
      digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN));
      delay(500);
    }
  }

  // disable all channels
  for (int i = 0; i < NUM_BATTERIES; i++) {
    MuxController::disableChannel(batteries[i].getMuxAddr());
  }

  DEBUG_PRINTLN("\n=== System Ready ===");
  DEBUG_PRINTLN("Place jumper cable tags on terminals to test");
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

  updatePacket();
  updateLEDs();
  delay(10);  // Reduced delay since we're polling less frequently
}
