#define RS485_IO 5  // RS485 transmit/receive status (RE/DE) Pin (RE/DE: receive/data enable - these two are common together)

#define RS485_TRANSMIT HIGH
#define RS485_RECEIVE LOW
#define BAUD_RATE 9600

#define LED_PIN 13

struct __attribute__((packed)) WallStatusPacket {
  uint8_t START1;
  uint8_t START2;
  uint8_t BAT_ID;
  uint8_t NEG_PRESENT;  // 0 = absent, 1 = present
  uint8_t NEG_STATE;    // 0 = wrong polarity, 1 = correct
  uint8_t POS_PRESENT;  // 0 = absent, 1 = present
  uint8_t POS_STATE;    // 0 = wrong polarity, 1 = correct
  uint8_t CHK;
};

enum RxState { WAIT_START1,
               WAIT_START2,
               READ_DATA };
RxState packetState = WAIT_START1;

uint8_t buffer[sizeof(WallStatusPacket)];
uint8_t packetIndex = 0;
uint32_t out1Timer = 0;
uint32_t out2Timer = 0;
uint32_t out3Timer = 0;
const uint16_t pulseDuration = 200;

bool validateChecksum(const uint8_t* buffer, uint8_t length) {
  WallStatusPacket pkt;
  memcpy(&pkt, buffer, sizeof(WallStatusPacket));

  uint8_t sum = 0;
  sum ^= pkt.BAT_ID;
  sum ^= pkt.NEG_PRESENT;
  sum ^= pkt.NEG_STATE;
  sum ^= pkt.POS_PRESENT;
  sum ^= pkt.POS_STATE;

  if (sum != pkt.CHK) {
    Serial.print("Checksum mismatch. Expected: ");
    Serial.print(sum, HEX);
    Serial.print(", Got: ");
    Serial.println(pkt.CHK, HEX);
    return false;
  }

  return sum == pkt.CHK;
}

void processPacket(const uint8_t* buffer) {
  WallStatusPacket pkt = { 0 };
  memcpy(&pkt, buffer, sizeof(WallStatusPacket));

  Serial.print("Valid Packet Received -> BAT:");
  Serial.print(pkt.BAT_ID);
  Serial.print(", NEG_PRESENT: ");
  Serial.print(pkt.NEG_PRESENT);
  Serial.print(", NEG_STATE: ");
  Serial.print(pkt.NEG_STATE);
  Serial.print(", POS_PRESENT: ");
  Serial.print(pkt.POS_PRESENT);
  Serial.print(", POS_STATE: ");
  Serial.println(pkt.POS_STATE);
}

// ========== SETUP ===========
void setup() {
  Serial.begin(9600);

  pinMode(RS485_IO, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // ensure onboard led is on
  digitalWrite(LED_PIN, LOW);

  // set RS485 device to start in RECEIVE mode
  digitalWrite(RS485_IO, RS485_RECEIVE);

  // set the baud rate
  // the longer the wire the slower you should set the transmission rate
  // anything here (300, 1200, 2400, 14400, 19200, etc) MUST BE THE SAME
  // AS THE SENDER UNIT
  Serial1.begin(BAUD_RATE);

  Serial.println("RS485 Receiver Ready");
}

// ========== LOOP =========
void loop() {
  // look for something on the serial pin
  if (Serial1.available()) {
    // read the incoming byte
    uint8_t byteIn = Serial1.read();

    switch (packetState) {
      case WAIT_START1:
        if (byteIn == 0xAA) {
          // first header byte valid, store and proceed to wait for second header byte
          buffer[0] = byteIn;
          packetIndex = 1;
          packetState = WAIT_START2;
        } else {
          packetIndex = 0;
        }
        break;

      case WAIT_START2:
        if (byteIn == 0x55) {
          // all header bytes valid, store and proceed to read packet data
          buffer[1] = byteIn;
          packetIndex++;
          packetState = READ_DATA;
        } else {
          packetState = WAIT_START1;
          packetIndex = 0;
        }
        break;

      case READ_DATA:
        buffer[packetIndex] = byteIn;
        packetIndex++;
        if (packetIndex >= sizeof(WallStatusPacket)) {
          // complete packet received, now validate and process data
          if (validateChecksum(buffer, sizeof(WallStatusPacket))) {
            processPacket(buffer);
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
          } else {
            Serial.println("Checksum failed");
          }
          // reset state for next packet
          packetState = WAIT_START1;  // ready to read again
          packetIndex = 0;
        }
        break;
    }
  }
}
