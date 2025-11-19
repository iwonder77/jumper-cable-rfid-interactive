#pragma once

#include <Arduino.h>

class AudioPlayer {
public:
  AudioPlayer();
  bool begin();                  // initialize SD + I2S
  void play(uint8_t triggerPin); // plays given wav file
  void update();

private:
  bool isPlaying = false;
  unsigned long playStartTime = 0;
};
