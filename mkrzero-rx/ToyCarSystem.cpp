#include "ToyCarSystem.h"
#include "Config.h"
#include "Debug.h"

ToyCarSystem::ToyCarSystem(HardwareSerial &serialPort)
    : rs485(serialPort, config::RS485_DE_PIN) {}

bool ToyCarSystem::begin() {
  // setup LED
  pinMode(config::ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(config::ONBOARD_LED_PIN, LOW);

  // start RS485 receiver and register the callback function
  rs485.begin(config::RS485_BAUD_RATE);
  rs485.setPacketHandler(packetHandlerStatic, this);

  // start audio system (SD + I2S)
  if (!audio.begin()) {
    DEBUG_PRINTLN(
        "ToyCarSystem: audio initialization failed (continuing without audio)");
    // continue - audio is optional but useful
  }

  DEBUG_PRINTLN("ToyCarSystem: system started");
  return true;
}

void ToyCarSystem::update() {
  // poll RS485 receiver to consume available bytes
  rs485.update();

  // NOTE: we can add audio loop checks or other non-blocking tasks here
  // e.g. handle periodic UI updates, sensors, etc.
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

  // decide outcome:
  bool negPresent = pkt.NEG_PRESENT != 0;
  bool posPresent = pkt.POS_PRESENT != 0;
  bool negOK = pkt.NEG_STATE != 0;
  bool posOK = pkt.POS_STATE != 0;
  bool successfulConnection = (posPresent && negPresent && posOK && negOK);

  // TODO: this logic below for success and failure will need to change,
  // but for testing purposes lets go to the following:

  // play audio depending on battery (6V, 12V, 16V) BUT
  // only play audio for correct jumper cable connections
  switch (pkt.BAT_ID) {
  case 0:
    if (successfulConnection) {
      audio.play(config::SPUTTER_AUDIO_FILE);
    }
    break;
  case 1:
    if (successfulConnection) {
      audio.play(config::ENGINE_START_AUDIO_FILE);
    }
    break;
  case 2:
    if (successfulConnection) {
      audio.play(config::ZAP_AUDIO_FILE);
    }
    break;
  default:
    break;
  }
}

void ToyCarSystem::pulseLed(uint16_t durationMs) {
  digitalWrite(config::ONBOARD_LED_PIN, HIGH);
  delay(durationMs);
  digitalWrite(config::ONBOARD_LED_PIN, LOW);
}
