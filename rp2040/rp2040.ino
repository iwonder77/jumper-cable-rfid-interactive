#include <Wire.h>
#include "LEDController.h"
#include "Config.h"

LEDController ledController;

volatile uint8_t currentAnimation = config::CMD_DEFAULT_ANIMATION;
volatile bool gotNewCommand = false;
uint8_t animation = config::CMD_DEFAULT_ANIMATION;
unsigned long animationEndTimestamp = 0;

void receiveEvent(int numBytes) {
  if (Wire.available()) {
    uint8_t cmd = Wire.read();
    if (cmd == config::CMD_6V_ANIMATION || cmd == config::CMD_12V_ANIMATION || cmd == config::CMD_16V_ANIMATION) {
      currentAnimation = cmd;
      gotNewCommand = true;
    } else if (cmd == config::CMD_DEFAULT_ANIMATION) {
      currentAnimation = cmd;
    }
    while (Wire.available()) {
      Wire.read();
    }
  }
}

void setup() {
  Wire.begin(config::LED_CONTROLLER_ADDR);
  Wire.onReceive(receiveEvent);

  Serial.begin(115200);
  ledController.initialize();
  Serial.println("LEDController initialized");
}

void loop() {
  // check if we got a new command to process
  if (gotNewCommand) {
    gotNewCommand = false;
    animationEndTimestamp = millis() + config::ANIMATION_DURATION_MS;
    noInterrupts();
    animation = currentAnimation;
    interrupts();
  }

  // determine which animation to play (default or 6V,12V,16V for specified amnt of time)
  if (animationEndTimestamp != 0 && millis() < animationEndTimestamp) {
    // special animation timer is active and has not expired yet
    ledController.update(animation);
    Serial.println("special anim");
  } else {
    // timer expired or no special animation - play default animation
    if (animationEndTimestamp != 0) {
      animationEndTimestamp = 0;
    }
    ledController.update(config::CMD_DEFAULT_ANIMATION);
    Serial.println("default anim");
  }
  Serial.println("man what");
}
