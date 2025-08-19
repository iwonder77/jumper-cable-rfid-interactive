#include "TerminalReader.h"

void TerminalReader::initialize() {

  Serial.print("Initializing ");
  Serial.print(terminal.name);
  Serial.print(" (Channel ");
  Serial.print(terminal.channel);
  Serial.print(")...");

  TCA9548A_setChannel(terminal.channel);

  Wire.beginTransmission(RFID2_WS1850S_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("FAILED - I2C Communication ERROR");
    terminal.isReaderOK = false;
    return;
  }

  reader.PCD_Init();
  delay(50);

  Serial.println(" SUCCESS");
  isReaderOK = true;
}
