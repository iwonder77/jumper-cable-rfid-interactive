#include "ToyCarSystem.h"
#include "Config.h"
#include "Debug.h"
#include "MuxController.h"
#include <Arduino.h>
#include <Wire.h>

ToyCarSystem::ToyCarSystem(HardwareSerial &serialPort)
    : rs485(serialPort, config::RS485_DE_PIN), muxAddr(config::MUX_ADDR),
      positive(config::RFID2_WS1850S_ADDR, "Positive",
               config::POSITIVE_TERMINAL_CHANNEL),
      negative(config::RFID2_WS1850S_ADDR, "Negative",
               config::NEGATIVE_TERMINAL_CHANNEL),
      gnd_frame(config::RFID2_WS1850S_ADDR, "Frame",
                config::GND_FRAME_CHANNEL) {}

bool ToyCarSystem::initialize(MFRC522 &reader) {
  prevToyCarTerminalState = {false, false, false, false, false, false};
  toyCarTerminalState = {false, false, false, false, false, false};
  prevWallBatteryState = {0, false, false, false, false};
  wallBatteryState = {0, false, false, false, false};
  // ----- setup LED -----
  pinMode(config::ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(config::ONBOARD_LED_PIN, LOW);

  // ----- setup RS485 -----
  // start RS485 receiver and register the callback function
  rs485.begin(config::RS485_BAUD_RATE);
  rs485.setPacketHandler(packetHandlerStatic, this);

  // ----- start audio system (SD + I2S) -----
  if (!audio.begin()) {
    DEBUG_PRINTLN(
        "ToyCarSystem: audio initialization failed (continuing without audio)");
    // continue - audio is optional but useful
  }

  // ----- test MUX communication -----
  DEBUG_PRINT("Testing mux communication - ");
  Wire.beginTransmission(muxAddr);
  byte result = Wire.endTransmission();
  if (result != 0) {
    DEBUG_PRINTLN("FAILED");
    muxCommunicationOK = false;
    return false;
  } else {
    DEBUG_PRINTLN("SUCCESS");
    muxCommunicationOK = true;
  }

  // ----- Initialize the RFID readers -----
  DEBUG_PRINT("Initializing readers");
  MuxController::selectChannel(muxAddr, config::POSITIVE_TERMINAL_CHANNEL);
  delay(config::CHANNEL_SWITCH_SETTLE_MS);
  positive.init(reader);

  MuxController::selectChannel(muxAddr, config::NEGATIVE_TERMINAL_CHANNEL);
  delay(config::CHANNEL_SWITCH_SETTLE_MS);
  negative.init(reader);

  MuxController::selectChannel(muxAddr, config::GND_FRAME_CHANNEL);
  delay(config::CHANNEL_SWITCH_SETTLE_MS);
  gnd_frame.init(reader);

  MuxController::disableChannel(muxAddr);

  if (!positive.getReaderStatus() || !negative.getReaderStatus()) {
    DEBUG_PRINT("Warning: Battery ");
    DEBUG_PRINT(id);
    DEBUG_PRINTLN(" has failed terminal(s)");
  }

  DEBUG_PRINTLN("ToyCarSystem: system started");
  return true;
}

void ToyCarSystem::update(MFRC522 &reader) {
  // poll RS485 receiver to consume available bytes
  rs485.update();

  // update positive terminal
  MuxController::selectChannel(muxAddr, config::POSITIVE_TERMINAL_CHANNEL);
  reader.PCD_Init();
  positive.update(reader);

  // update negative terminal
  MuxController::selectChannel(muxAddr, config::NEGATIVE_TERMINAL_CHANNEL);
  reader.PCD_Init();
  negative.update(reader);

  // update gnd frame terminal
  MuxController::selectChannel(muxAddr, config::GND_FRAME_CHANNEL);
  reader.PCD_Init();
  gnd_frame.update(reader);

  MuxController::disableChannel(muxAddr);

  toyCarTerminalState = getCurrentState();

  bool stateChange = (toyCarTerminalState != prevToyCarTerminalState) ||
                     (wallBatteryState != prevWallBatteryState);

  // play audio depending on battery (6V, 12V, 16V) choice and correct
  // configuration on BOTH ends, play "wrong" sound otherwise
  if (stateChange) {
    if (wallBatteryState.successfulConnection()) {
      switch (wallBatteryState.id) {
      case 0:
        if ((toyCarTerminalState.posPolarity &&
             toyCarTerminalState.framePresent) ||
            (toyCarTerminalState.posPolarity &&
             toyCarTerminalState.negPolarity)) {
          if (!audio.isPlaying()) {
            audio.play(config::SPUTTER_AUDIO_FILE);
          }
        }
        break;
      case 1:
        if (toyCarTerminalState.posPolarity &&
            toyCarTerminalState.framePolarity) {
          if (!audio.isPlaying()) {
            audio.play(config::ENGINE_START_AUDIO_FILE);
          }
        }
        break;
      case 2:
        if ((toyCarTerminalState.posPolarity &&
             toyCarTerminalState.framePresent) ||
            (toyCarTerminalState.posPolarity &&
             toyCarTerminalState.negPolarity)) {
          if (!audio.isPlaying()) {
            audio.play(config::ZAP_AUDIO_FILE);
          }
        }
        break;
      default:
        break;
      }
      if (wallBatteryState.presentConnection() &&
          ((toyCarTerminalState.negPresent && toyCarTerminalState.posPresent) ||
           (toyCarTerminalState.framePresent &&
            toyCarTerminalState.posPresent))) {
        if (!audio.isPlaying()) {
          audio.play(config::WRONG_CHOICE_AUDIO_FILE);
        }
      }
    }
    prevToyCarTerminalState = toyCarTerminalState;
    prevWallBatteryState = wallBatteryState;
  }
}

// BRIDGE FUNCTION
// acts as an adapter between the expected callback signature and the actual
// class member function
void ToyCarSystem::packetHandlerStatic(const WallStatusPacket &pkt, void *ctx) {
  ToyCarSystem *self = reinterpret_cast<ToyCarSystem *>(ctx);
  if (self)
    self->onPacketReceived(pkt);
}

// Packet Processing Function
void ToyCarSystem::onPacketReceived(const WallStatusPacket &pkt) {
  DEBUG_PRINT("ToyCarSystem: Packet -> BAT:");
  DEBUG_PRINT(pkt.BAT_ID);
  DEBUG_PRINT(", NEG_PRESENT:");
  DEBUG_PRINT(pkt.NEG_PRESENT);
  DEBUG_PRINT(", NEG_STATE:");
  DEBUG_PRINT(pkt.NEG_STATE);
  DEBUG_PRINT(", POS_PRESENT:");
  DEBUG_PRINT(pkt.POS_PRESENT);
  DEBUG_PRINT(", POS_STATE:");
  DEBUG_PRINTLN(pkt.POS_STATE);

  // update state:
  wallBatteryState.id = pkt.BAT_ID;
  wallBatteryState.posPresent = pkt.POS_PRESENT;
  wallBatteryState.negPresent = pkt.NEG_PRESENT;
  wallBatteryState.posPolarity = pkt.POS_STATE;
  wallBatteryState.negPolarity = pkt.NEG_STATE;
}

TerminalState ToyCarSystem::getCurrentState() const {
  bool posPresent = (positive.getTagState() == TAG_PRESENT);
  bool negPresent = (negative.getTagState() == TAG_PRESENT);
  bool framePresent = (gnd_frame.getTagState() == TAG_PRESENT);

  return {
      posPresent,
      negPresent,
      framePresent,
      posPresent ? positive.polarityOK() : false,
      negPresent ? negative.polarityOK() : false,
      framePresent ? gnd_frame.polarityOK() : false,
  };
}

void ToyCarSystem::pulseLed(uint16_t durationMs) {
  digitalWrite(config::ONBOARD_LED_PIN, HIGH);
  delay(durationMs);
  digitalWrite(config::ONBOARD_LED_PIN, LOW);
}
