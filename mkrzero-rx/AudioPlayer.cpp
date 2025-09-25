#include "AudioPlayer.h"
#include "Config.h"
#include "Debug.h"

AudioPlayer::AudioPlayer() : initialized(false), currentFile("") {}

bool AudioPlayer::begin() {
  if (!SD.begin()) {
    DEBUG_PRINTLN("AudioPlayer: SD begin failed");
    initialized = false;
    return false;
  }

  AudioOutI2S.volume(config::AUDIO_VOLUME_PERCENT);
  initialized = true;
  DEBUG_PRINTLN("AudioPlayer: initialized (SD + I2S)");
  return true;
}

bool AudioPlayer::play(const char *filename) {
  if (!initialized) {
    DEBUG_PRINTLN("AudioPlayer: not initialized, cannot play");
    return false;
  }

  if (!filename) {
    DEBUG_PRINTLN("AudioPlayer: null filename");
    return false;
  }

  currentFile = filename;

  // create SDWaveFile instance and store in member (must outlive
  // AudioOutI2S.play)
  waveFile = SDWaveFile(currentFile.c_str());
  if (!waveFile) {
    DEBUG_PRINT("AudioPlayer: wave file invalid: ");
    DEBUG_PRINTLN(currentFile);
    return false;
  }

  if (!AudioOutI2S.canPlay(waveFile)) {
    DEBUG_PRINT("AudioPlayer: cannot play file format: ");
    DEBUG_PRINTLN(currentFile);
    return false;
  }

  DEBUG_PRINT("AudioPlayer: playing ");
  DEBUG_PRINTLN(currentFile);
  AudioOutI2S.play(waveFile);
  return true;
}

void AudioPlayer::stop() {
  AudioOutI2S.stop();
  DEBUG_PRINTLN("AudioPlayer: stopped");
}

bool AudioPlayer::isPlaying() const { return AudioOutI2S.isPlaying(); }
