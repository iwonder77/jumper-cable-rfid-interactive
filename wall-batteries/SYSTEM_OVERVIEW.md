# Wall Battery System — Logic Overview

**Board:** Arduino Leonardo  
**Role:** Polls all wall-mounted RFID readers, detects jumper cable tag placement,
and sends state-change packets to the toy car MCU over RS485.

---

## Hardware Topology

Three wall batteries are each managed by a dedicated TCA9548A I2C multiplexer.
Each multiplexer exposes two M5Stack WS1850S RFID readers — one per terminal.
All six readers share the same I2C address (`0x28`), isolated by their
respective multiplexer.

```
Arduino Leonardo
  │
  ├── I2C bus (SDA/SCL via level converter, 100kHz)
  │     ├── TCA9548A @ 0x70  (6V battery)
  │     │     ├── Ch 1 → WS1850S @ 0x28  (NEG terminal)
  │     │     └── Ch 2 → WS1850S @ 0x28  (POS terminal)
  │     ├── TCA9548A @ 0x71  (12V battery)
  │     │     ├── Ch 1 → WS1850S @ 0x28  (NEG terminal)
  │     │     └── Ch 2 → WS1850S @ 0x28  (POS terminal)
  │     └── TCA9548A @ 0x72  (16V battery)
  │           ├── Ch 1 → WS1850S @ 0x28  (NEG terminal)
  │           └── Ch 2 → WS1850S @ 0x28  (POS terminal)
  │
  ├── Serial1 → RS485 (DE pin 5) → Toy Car MCU (MKRZero)
  │
  ├── Pin 6 → GREEN LED
  └── Pin 7 → RED LED
```

---

## Source File Roles

| File                       | Responsibility                                                                                                                  |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| `leonardo-tx.ino`          | Entry point — watchdog setup, delegates entirely to `WallBatterySystem`                                                         |
| `Config.h`                 | All compile-time constants: pins, addresses, timing values                                                                      |
| `JumperCableTag.h`         | Tag domain types (`TagData` struct, `parseTagData()`, `typeToString()`)                                                         |
| `Reader.h/.cpp`            | Hardware driver for one RFID reader — owns its MFRC522 instance, manages mux channel via `MuxGuard`, runs the tag state machine |
| `Battery.h/.cpp`           | Coordinates a positive + negative `Reader` pair for one battery slot; validates polarity and cable ID pairing                   |
| `WallBatterySystem.h/.cpp` | System coordinator — round-robin polling, state-diff tracking, RS485 transmission, LED control, health monitoring               |
| `CommPacket.h`             | `WallStatusPacket` struct and XOR checksum shared between Leonardo (sender) and MKRZero (receiver)                              |
| `MuxController.h`          | Static helpers for TCA9548A channel selection/deselection over I2C                                                              |
| `Debug.h`                  | `DEBUG_PRINT`/`DEBUG_PRINTLN` macros, compile-time removable via `DEBUG_LEVEL`                                                  |

---

## Class Coordination

```
WallBatterySystem
  ├── Battery[0]  (6V,  mux 0x70)
  │     ├── Reader "Positive"  (mux 0x70, ch 2)
  │     └── Reader "Negative"  (mux 0x70, ch 1)
  ├── Battery[1]  (12V, mux 0x71)
  │     ├── Reader "Positive"  (mux 0x71, ch 2)
  │     └── Reader "Negative"  (mux 0x71, ch 1)
  └── Battery[2]  (16V, mux 0x72)
        ├── Reader "Positive"  (mux 0x72, ch 2)
        └── Reader "Negative"  (mux 0x72, ch 1)
```

`WallBatterySystem` owns the three `Battery` instances. Each `Battery` owns
two `Reader` instances. Each `Reader` owns its own `MFRC522DriverI2C` and
`MFRC522` objects — no shared hardware handle is passed around.

---

## Main Loop Timing

```
loop()  [every 5ms]
  │
  ├── wdt_reset()                      // reset 4s watchdog
  │
  ├── updateSystem()                   // RFID polling (rate-limited to 50ms)
  │     └── [if 50ms elapsed]
  │           batteries[N].updateReaders()   // poll ONE battery per tick
  │           N = (N + 1) % 3               // advance round-robin index
  │
  └── processSystemLogic()             // state evaluation (every 5ms)
        ├── updateCommunication()      // RS485 state-diff check
        ├── updateLEDs()               // LED state-diff check
        └── updateSystemHealth()       // health flag update
```

One full rotation through all three batteries takes 3 × 50ms = **150ms**.
Each individual reader is polled once every 150ms.

`MuxGuard` (RAII struct in `Reader.h`) ensures the mux channel is always
deselected when `Reader::update()` returns, regardless of which path the
state machine takes through the function.

---

## Tag Data Format

Each RFID tag carries 6 bytes written at MIFARE page 4:

```
Offset  Size  Field     Values
──────  ────  ────────  ──────────────────────────────
0       4     type      "POS\0" or "NEG\0"
4       1     id        1, 2, 3, or 4 (cable end ID)
5       1     checksum  XOR of bytes 0–4
```

Parsed into:

```cpp
struct jumper_cable_tag::TagData {
  char    type[4];
  uint8_t id;
  uint8_t checksum;
};
```

`parseTagData()` validates that `type` is either "POS" or "NEG", then verifies
the XOR checksum. Both conditions must pass for a tag to be considered valid.

---

## Reader Tag State Machine

Each `Reader` runs this state machine independently during `update()`.

```
        ┌────────────────────────────────────┐
        │                                    │
        ▼   tag seen                         │ tag seen (same UID, re-appeared)
    [Absent] ────────────► [Detecting]       │
        ▲                      │             │
        │                      │ 150ms of    │
        │  ≥3 consecutive      │ continuous  │
        │  read failures       │ detection   │
        │                      ▼             │
        │              [Confirmed] ──────────┘
        │                    │  │
        │                    │  └─ different UID → back to [Detecting]
        │  >900ms since      │
        │  last seen         │ ≥3 consecutive read failures
        │                    ▼
        └────────── [Departing]
```

### State Descriptions

**Absent** — No tag. Waiting for the first RFID detection.

**Detecting** — A tag was just seen for the first time. Debounce is running
(150ms). The system waits to confirm the tag is genuinely present, not a
transient RF reflection. If ≥3 consecutive read failures occur here before the
debounce completes, the state reverts to `Absent`.

**Confirmed** — Tag is stably present. Tag identification is attempted once per
poll cycle until `parseTagData()` succeeds (max 5 attempts). Once identified,
`tag_identified_` is set and the data is stable in `tag_`. If the same tag
stays present, nothing further happens in this state until it departs. If a
different UID appears (cable swapped), the state resets to `Detecting` and the
new tag goes through debounce.

**Departing** — Tag has stopped responding (≥3 consecutive read failures from
`Confirmed`). Awaiting confirmation that it is truly removed. If the tag
reappears within 900ms, the state returns to `Detecting`. If no tag is seen for
900ms (`TAG_ABSENCE_TIMEOUT × 2`), the state settles to `Absent`. The doubled
timeout provides a safety margin for momentary RF occlusion (e.g. a hand
briefly passing over the reader).

### Tag Identification within Confirmed

```
On each update() while in Confirmed state:

  if (!tag_identified_ && read_attempts_ < MAX_READ_ATTEMPTS):
    result = MIFARE_Read(page 4)
    if read succeeded:
      parseTagData(buffer, tag_)
      if parse succeeded (type valid + checksum OK):
        tag_identified_ = true   ← identification complete, stop retrying
      else:
        read_attempts_++         ← bad data, will retry next poll
    else:
      read_attempts_++           ← hardware read failed, will retry next poll

  if read_attempts_ == MAX_READ_ATTEMPTS:
    log "unreadable" once, stop retrying
    tag_ remains zeroed, tag_identified_ stays false
    system treats slot as occupied but unknown (no polarity, no ID)
```

---

## Battery: Polarity and Pairing Validation

`Battery::hasValidConfiguration()` is the domain logic layer. It is called
by `WallBatterySystem` to determine LED color and is also the source of
polarity truth for the RS485 state snapshot.

```
hasValidConfiguration():
  1. Both terminals must be in TagState::Confirmed         → else false
  2. Positive terminal's tag.type must be "POS"            → else false (wrong polarity)
  3. Negative terminal's tag.type must be "NEG"            → else false (wrong polarity)
  4. Cable ID pair must be valid:
       (posID=1, negID=3)  or  (posID=2, negID=4)
       (posID=2, negID=3)  or  (posID=1, negID=4)         → else false (IDs don't match)
  → true  (valid jumpstart configuration)
```

Valid ID pairs represent the four cable ends on two physical jumper cables.
Connecting both ends of the same cable across a battery is a valid connection.

---

## WallBatterySystem: State-Diff and RS485 Transmission

`WallBatterySystem` does not react to hardware events directly. Instead it
takes a snapshot of each battery's state every 5ms and compares it to the
previously stored snapshot. A packet is sent only when something changed.

### BatteryState Snapshot

```cpp
struct BatteryState {
  bool posPresent;   // positive Reader is in TagState::Confirmed
  bool negPresent;   // negative Reader is in TagState::Confirmed
  bool posPolarity;  // posPresent && tag_.type == "POS"
  bool negPolarity;  // negPresent && tag_.type == "NEG"
};
```

`getCurrentBatteryState()` reads from Reader getters and derives polarity via
`strncmp` on `tag_.type` — the polarity judgment lives here at the system layer,
not inside the hardware driver.

### State-Diff Loop

```
updateCommunication()  [called every 5ms]:

  for each battery i in [0, 1, 2]:
    currentState = getCurrentBatteryState(i)

    if currentState != lastStates[i]:
      lastStates[i] = currentState
      sendBatteryPacket(i, currentState)
      delay(5ms)              ← RS485 line settle time
```

This fires a packet for **any** state change on any battery — a tag appearing,
a tag departing, a polarity flip (tag swapped for wrong-polarity cable). Each
battery is tracked independently.

### RS485 Packet Format

```
Byte  Field        Description
────  ───────────  ─────────────────────────────────────────────
0     START1       0xAA  (framing)
1     START2       0x55  (framing)
2     BAT_ID       0=6V, 1=12V, 2=16V
3     NEG_PRESENT  1 if negative terminal has a confirmed tag
4     NEG_STATE    1 if that tag has correct polarity ("NEG")
5     POS_PRESENT  1 if positive terminal has a confirmed tag
6     POS_STATE    1 if that tag has correct polarity ("POS")
7     CHK          XOR of bytes 2–6
```

Total: 8 bytes. Transmitted via hardware `Serial1` at 9600 baud. The DE pin
(pin 5) is held HIGH for transmit. The MKRZero receiver uses the same struct
definition from `CommPacket.h`.

---

## LED State Machine

LEDs are updated in `updateLEDs()` via state-diff — `setLEDState()` is only
called when `determineLEDState()` returns a value different from `lastLEDState`.

```
determineLEDState():
  activeBattery = first battery with BOTH terminals in TagState::Confirmed

  if no active battery:
    → LED_OFF    (waiting for both tags on any battery)

  if activeBattery.hasValidConfiguration():
    → LED_GREEN  (correct polarity + valid cable ID pair)
  else:
    → LED_RED    (both tags present but wrong polarity or mismatched IDs)
```

Only one battery drives the LED at a time — the first one (by index 0→2) that
has both terminals confirmed. If multiple batteries are cabled simultaneously,
index priority determines which one is displayed.

---

## End-to-End Example

```
Guest places "POS id=1" cable end on the 6V battery positive terminal:

T=0ms    Tag physically placed on reader

T=50ms   Battery 0 polled (its turn in the round-robin)
         Reader "Positive" (mux 0x70, ch 2):
           MuxGuard selects ch 2 on mux 0x70
           PICC_IsNewCardPresent() → true
           TagState: Absent → Detecting
           first_seen_time_ = now
           MuxGuard destructor deselects ch 2

T=200ms  Battery 0 polled again (3rd poll for this reader, 150ms elapsed)
         (now - first_seen_time_) = 150ms > TAG_DEBOUNCE_TIME
         TagState: Detecting → Confirmed
         tag_identified_ = false, read_attempts_ = 0

         readTagData() called:
           MIFARE_Read(page 4) → buffer
           parseTagData(): type="POS", id=1, checksum OK → true
           PCD_StopCrypto1()
         tag_identified_ = true
         tag_.type = "POS", tag_.id = 1

T=200ms  processSystemLogic() → updateCommunication():
         getCurrentBatteryState(0):
           posPresent = true  (Confirmed)
           negPresent = false (still Absent)
           posPolarity = true (type == "POS" on positive channel)
           negPolarity = false
         currentState {true, false, true, false} != lastState {false,false,false,false}
         → sendBatteryPacket(0, currentState)

         Packet bytes: AA 55 00 00 00 01 01 00
                        S1 S2 ID NP NS PP PS CK
           BAT_ID=0, NEG_PRESENT=0, NEG_STATE=0,
           POS_PRESENT=1, POS_STATE=1, CHK=0^0^0^1^1=0

         Serial1.write(8 bytes) → RS485 → MKRZero toy car MCU
         lastStates[0] updated

T=200ms  updateLEDs():
         findActiveBattery(): both terminals not yet confirmed → -1
         → LED_OFF (no change from last state, setLEDState not called)
```
