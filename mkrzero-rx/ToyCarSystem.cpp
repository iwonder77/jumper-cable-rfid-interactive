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
                config::GND_FRAME_CHANNEL) {
  prevToyCarTerminalState = {false, false, false, false, false, false};
  toyCarTerminalState = {false, false, false, false, false, false};
  prevWallBatteryState = {0, false, false, false, false};
  wallBatteryState = {0, false, false, false, false};
}

bool ToyCarSystem::initialize(MFRC522 &reader) {
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

  // --- send default command to led driver ---
  ledCommander.init();

  DEBUG_PRINTLN("ToyCarSystem: system started");
  return true;
}

void ToyCarSystem::update(MFRC522 &reader) {
  unsigned long now = millis();
  // poll RS485 receiver to consume available bytes
  rs485.update();

  if (now - lastRFIDCheck >= rfidCheckIntervalMs) {
    lastRFIDCheck = now;
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
  }

  bool stateChange = (toyCarTerminalState != prevToyCarTerminalState) ||
                     (wallBatteryState != prevWallBatteryState);

  // handle audio playing + LED strip animation logic here
  // NOTE:
  // - only play successful engine startup sound when 12V wall battery is
  //  chosen (with correct configuration) and GND Frame and Positive terminal of
  //  toy car is chosen
  // - only play "incorrect" sound when jumper cables are placed on both
  // terminals of one wall battery and both terminals of toy car battery
  if (stateChange) {
    // default animation mode to none
    mode = AnimationMode::None;
    if (wallBatteryState.successfulConnection()) {
      // wall battery side has successfull connection, check which one was
      // chosen AND check state of toy car terminals
      switch (wallBatteryState.id) {
      // 6V
      case 0:
        if ((toyCarTerminalState.posPolarity &&
             toyCarTerminalState.framePolarity) ||
            (toyCarTerminalState.posPolarity &&
             toyCarTerminalState.negPolarity)) {
          DEBUG_PRINTLN("6V Battery Connected!");
          audio.play(config::SPUTTER_AUDIO_TRIGGER);
          mode = AnimationMode::SixV;
        } else if ((toyCarTerminalState.posPresent &&
                    toyCarTerminalState.negPresent) ||
                   (toyCarTerminalState.posPresent &&
                    toyCarTerminalState.framePresent)) {
          DEBUG_PRINTLN("Wrong Connection!");
          audio.play(config::WRONG_CHOICE_AUDIO_TRIGGER);
          mode = AnimationMode::Wrong;
        }
        break;
      // 12V
      case 1:
        if (toyCarTerminalState.posPolarity &&
            toyCarTerminalState.framePolarity) {
          DEBUG_PRINTLN("12V Battery Connected!");
          audio.play(config::ENGINE_START_AUDIO_TRIGGER);
          mode = AnimationMode::TwelveV;
        } else if ((toyCarTerminalState.posPresent &&
                    toyCarTerminalState.negPresent) ||
                   (toyCarTerminalState.posPresent &&
                    toyCarTerminalState.framePresent)) {
          DEBUG_PRINTLN("Wrong Connection!");
          audio.play(config::WRONG_CHOICE_AUDIO_TRIGGER);
          mode = AnimationMode::Wrong;
        }
        break;
        // 16V
      case 2:
        if ((toyCarTerminalState.posPolarity &&
             toyCarTerminalState.framePolarity) ||
            (toyCarTerminalState.posPolarity &&
             toyCarTerminalState.negPolarity)) {
          DEBUG_PRINTLN("16V Battery Connected!");
          audio.play(config::ZAP_AUDIO_TRIGGER);
          mode = AnimationMode::SixteenV;
        } else if ((toyCarTerminalState.posPresent &&
                    toyCarTerminalState.negPresent) ||
                   (toyCarTerminalState.posPresent &&
                    toyCarTerminalState.framePresent)) {
          DEBUG_PRINTLN("Wrong Connection!");
          audio.play(config::WRONG_CHOICE_AUDIO_TRIGGER);
          mode = AnimationMode::Wrong;
        }
        break;
      }
    } else if ((wallBatteryState.negPresent && wallBatteryState.posPresent) &&
               ((toyCarTerminalState.negPresent &&
                 toyCarTerminalState.posPresent) ||
                (toyCarTerminalState.framePresent &&
                 toyCarTerminalState.posPresent))) {
      DEBUG_PRINTLN("Wrong Connection!");
      audio.play(config::WRONG_CHOICE_AUDIO_TRIGGER);
      mode = AnimationMode::Wrong;
    }
    prevToyCarTerminalState = toyCarTerminalState;
    prevWallBatteryState = wallBatteryState;
  }
  audio.update();

  // only send I2C command for animation when the animation mode CHANGES
  if (prevMode != mode) {
    // free the i2c bus
    MuxController::disableChannel(muxAddr);
    delay(5);
    switch (mode) {
    case AnimationMode::None:
      ledCommander.sendCommand(config::CMD_DEFAULT_ANIMATION);
      break;
    case AnimationMode::SixV:
      ledCommander.sendCommand(config::CMD_6V_ANIMATION);
      break;
    case AnimationMode::TwelveV:
      ledCommander.sendCommand(config::CMD_12V_ANIMATION);
      break;
    case AnimationMode::SixteenV:
      ledCommander.sendCommand(config::CMD_16V_ANIMATION);
      break;
    case AnimationMode::Wrong:
      ledCommander.sendCommand(config::CMD_WRONG_ANIMATION);
      break;
    default:
      break;
    }
    prevMode = mode;
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
