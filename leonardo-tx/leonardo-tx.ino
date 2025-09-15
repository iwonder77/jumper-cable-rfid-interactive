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

#include "WallBatterySystem.h"

// ----- MAIN RFID HARDWARE INSTANCES -----
MFRC522DriverI2C driver{ RFID2_WS1850S_ADDR, Wire };
MFRC522 reader{ driver };

// ----- WALL BATTERY SYSTEM INSTANCE -----
WallBatterySystem wallSystem;

// ========== SETUP ==========
void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!wallSystem.initializeSystem(reader)) {
    // system initialization failed - wallSystem handles error state
    return;
  }
}

// ========== MAIN LOOP ==========
void loop() {
  wallSystem.updateSystem(reader);
  wallSystem.processSystemLogic();
  delay(10);
}
