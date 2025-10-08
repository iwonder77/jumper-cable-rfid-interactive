#pragma once
/**
 * AudioPlayer.h
 *
 * Lightweight wrapper around ArduinoSound (AudioOutI2S + SDWaveFile) that:
 *  - initializes SD and I2S,
 *  - plays a SDWaveFile by filename,
 *  - exposes play/stop/isPlaying methods.
 *
 * Note:
 * - On MKRZero the SD library typically uses the built-in SD slot; if you
 * have an external module, pass the CS pin to SD.begin(...) in begin().
 * - Connections to PCM5102:
 *  * GND connected GND
 *  * VIN connected 5V
 *  * DIN connected to pin A6 (MKR Zero)
 *  * WSEL connected to pin 3 (MKR Zero)
 *  * BCK connected to pin 2 (MKR Zero)
 */

#include <Arduino.h>
#include <ArduinoSound.h>
#include <SD.h>

class AudioPlayer {
public:
  AudioPlayer();
  bool begin();                    // initialize SD + I2S
  bool play(const char *filename); // plays given wav file
  void stop();
  bool isPlaying() const;

private:
  SDWaveFile waveFile; // must remain alive while AudioOutI2S plays it
  bool initialized;
  String currentFile;
};
