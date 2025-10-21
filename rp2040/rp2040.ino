#include <Wire.h>
#include "LEDController.h"
#include "Config.h"

LEDController ledController;

volatile uint8_t currentAnimation = 0xFF;
volatile bool gotNewCommand = false;
uint8_t animation = 0xFF;
uint8_t lastPlayedAnimation = 0xFF;  // Track last animation to detect changes
unsigned long animationEndTimestamp = 0;

void receiveEvent(int numBytes) {
  if (numBytes <= 0) return;
  uint8_t cmd = Wire.read();
  if (cmd == config::CMD_6V_ANIMATION || cmd == config::CMD_12V_ANIMATION || cmd == config::CMD_16V_ANIMATION || cmd == config::CMD_WRONG_ANIMATION) {
    currentAnimation = cmd;
    gotNewCommand = true;
  } else if (cmd == config::CMD_DEFAULT_ANIMATION) {
    currentAnimation = cmd;
  }
  while (Wire.available()) {
    Wire.read();
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("RP2040 booting...");

  Wire.begin(config::LED_CONTROLLER_ADDR);
  Wire.onReceive(receiveEvent);

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
  uint8_t activeAnimation;
  if (animationEndTimestamp != 0 && millis() < animationEndTimestamp) {
    // special animation timer is active and has not expired yet
    activeAnimation = animation;
  } else {
    // timer expired or no special animation - play default animation
    if (animationEndTimestamp != 0) {
      animationEndTimestamp = 0;
    }
    activeAnimation = config::CMD_DEFAULT_ANIMATION;
  }

  // Print debug message only when animation changes
  if (activeAnimation != lastPlayedAnimation) {
    Serial.print("Animation changed to: ");
    switch (activeAnimation) {
      case config::CMD_DEFAULT_ANIMATION:
        Serial.println("DEFAULT");
        break;
      case config::CMD_6V_ANIMATION:
        Serial.println("6V");
        break;
      case config::CMD_12V_ANIMATION:
        Serial.println("12V");
        break;
      case config::CMD_16V_ANIMATION:
        Serial.println("16V");
        break;
      default:
        Serial.print("UNKNOWN (");
        Serial.print(activeAnimation);
        Serial.println(")");
        break;
    }
    lastPlayedAnimation = activeAnimation;
  }

  ledController.update(activeAnimation);
}
