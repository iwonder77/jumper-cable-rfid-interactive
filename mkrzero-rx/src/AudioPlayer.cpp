#include "AudioPlayer.h"
#include "Config.h"
#include "Debug.h"
#include "api/Common.h"

AudioPlayer::AudioPlayer() {}

bool AudioPlayer::begin() {
  pinMode(config::SPUTTER_AUDIO_TRIGGER, OUTPUT);
  pinMode(config::ENGINE_START_AUDIO_TRIGGER, OUTPUT);
  pinMode(config::ZAP_AUDIO_TRIGGER, OUTPUT);
  pinMode(config::WRONG_CHOICE_AUDIO_TRIGGER, OUTPUT);

  delay(10);

  digitalWrite(config::SPUTTER_AUDIO_TRIGGER, LOW);
  digitalWrite(config::ENGINE_START_AUDIO_TRIGGER, LOW);
  digitalWrite(config::ZAP_AUDIO_TRIGGER, LOW);
  digitalWrite(config::WRONG_CHOICE_AUDIO_TRIGGER, LOW);

  delay(10);

  DEBUG_PRINTLN("AudioPlayer: initialized (SD + I2S)");
  return true;
}

void AudioPlayer::play(uint8_t triggerPin) {
  if (isPlaying) {
    DEBUG_PRINTLN("AudioPlayer: currently playing, cannot play");
    return;
  }

  DEBUG_PRINT("AudioPlayer: playing ");
  switch (triggerPin) {
  case (config::SPUTTER_AUDIO_TRIGGER):
    DEBUG_PRINTLN("6V sputter audio");
    break;
  case (config::ENGINE_START_AUDIO_TRIGGER):
    DEBUG_PRINTLN("12V engine start audio");
    break;
  case (config::ZAP_AUDIO_TRIGGER):
    DEBUG_PRINTLN("16V zap audio");
    break;
  case (config::WRONG_CHOICE_AUDIO_TRIGGER):
    DEBUG_PRINTLN("Wrong choice audio");
    break;
  default:
    break;
  }

  digitalWrite(triggerPin, HIGH);
  isPlaying = true;
  playStartTime = millis();

  return;
}

void AudioPlayer::update() {
  if (isPlaying) {
    if (millis() - playStartTime > config::PULSE_SEND_TIME_MS) {
      digitalWrite(config::SPUTTER_AUDIO_TRIGGER, LOW);
      digitalWrite(config::ENGINE_START_AUDIO_TRIGGER, LOW);
      digitalWrite(config::ZAP_AUDIO_TRIGGER, LOW);
      digitalWrite(config::WRONG_CHOICE_AUDIO_TRIGGER, LOW);
      isPlaying = false;
      playStartTime = 0;
    }
  }
  return;
}
