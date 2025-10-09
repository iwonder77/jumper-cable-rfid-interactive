/* 
************************************************
* File: mkrzero-rx.ino
* Project: Jumper Cable RFID Interactive - MKR Zero (Toy Car) Rx System 
*
* Author: Isai Sanchez
* Created: 2025-09
*
* Description:  Entry point for software that'll integrate RS-485 receiver + audio player 
*               + terminal reading + LED logic
* Notes:
*************************************************
*/

#include <Wire.h>
#include <MFRC522v2.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522Debug.h>

#include "ToyCarSystem.h"
#include "Debug.h"

// ----- MAIN RFID HARDWARE INSTANCES -----
MFRC522DriverI2C driver{ config::RFID2_WS1850S_ADDR, Wire };
MFRC522 reader{ driver };

ToyCarSystem toyCar(Serial1);

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin();

  DEBUG_PRINTLN("Toy Car MKRZero starting...");
  if (!toyCar.initialize(reader)) {
    DEBUG_PRINTLN("System failed to initialize");
    while (1);
  }
}

void loop() {
  toyCar.update(reader);
  // keep loop free for quick responses (no heavy blocking)
}
