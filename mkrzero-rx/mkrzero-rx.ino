/*
 This reads a wave file from an SD card and plays it using the I2S interface to
 a PCM5102A I2S DAC board.

 Circuit:
 * Arduino Zero, MKR Zero or MKR1000 board
 * SD breakout or shield connected
 * MAX98357:
    * GND connected GND
    * VIN connected 5V
    * DIN connected to pin 9 (Zero) or pin A6 (MKR1000, MKR Zero)
    * LRC connected to pin 0 (Zero) or pin 3 (MKR1000, MKR Zero)
    * BCLK connected to pin 1 (Zero) or pin 2 (MKR1000, MKR Zero)
 */

#include <SD.h>
#include <ArduinoSound.h>

const char filename[] = "music.wav";
SDWaveFile waveFile;

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  
  Serial.println("Testing I2S audio...");
  
  if (!SD.begin()) {
    Serial.println("SD card failed!");
    return;
  }
  
  waveFile = SDWaveFile(filename);
  
  if (!waveFile) {
    Serial.println("Wave file invalid!");
    return;
  }
  
  AudioOutI2S.volume(100); // Max volume for testing
  
  if (!AudioOutI2S.canPlay(waveFile)) {
    Serial.println("Cannot play file!");
    return;
  }
  
  Serial.println("Playing audio...");
  AudioOutI2S.play(waveFile);
}

void loop() {
  if (!AudioOutI2S.isPlaying()) {
    Serial.println("Playback finished");
    delay(2000);
    AudioOutI2S.play(waveFile); // Loop the audio
  }
}
