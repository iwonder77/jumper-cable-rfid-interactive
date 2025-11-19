# Jumper Cable RFID Battery Interactive

## Overview

Source code for the jumper cable interactive firmware in Thanksgiving Point's Auto Shop Kidopolis exhibit. Aims to teach children how to utilize jumper cables to connect a healthy battery from a **wall mounted battery system** (Arduino Leonardo) to an unhealthy **toy car battery system** (Arduino MKRZERO). The Wall Battery System detects when RFID-tagged jumper cable ends are correctly placed on the terminals of a 6V, 12V, and 16V battery, validates polarity, and communicates battery status over RS-485. The Toy Car System receives these packets, interprets the configuration, handles its own jumper cable placement/polarity detection, and provides feedback by animating the engine bay’s lights (LED strip + rp2040) and playing sounds on a 4 Ohm 50W rated speaker using a high level, trigger-based DY-HL30T sound module.

RFID tags are embedded within the jumper cables, I wrote lightweight data onto them using this [sketch](https://github.com/iwonder77/rw-NTAG203-rfid-tag) to differentiate between positive and negative jumper cable ends.

## Hardware Components

Wall Battery System:

- **Microcontroller**: Arduino Leonardo
- **RFID Readers**: 6 x RFID2 WS1850S M5Stack readers (fixed I2C address of 0x28) - 2 per battery
- **I2C Multiplexer**: 3 x Adafruit's TCA9548A 8 Channel I2C Multiplexer (variable I2C address of 0x70, 0x71, and 0x72) - 1 per battery
- **Level Converter**: 1 x 5V -> 3V3 Level Converter to allow Leonardo's 5V I2C lines to work with RFID2 reader's 3V3 level I2C lines
- **Status LEDs**: Green (correct polarity configuration) and Red (incorrect polarity configuration) indicators

Toy Car System:

- **Microcontroller**: Arduino MKRZERO
- **RFID Readers**: 3 x RFID2 WS1850S M5Stack readers (fixed I2C address of 0x28) - 2 on the unhealthy battery and 1 on the car frame to represent GND
- **I2C Multiplexer**: 1 x Adafruit's TCA9548A 8 Channel I2C Multiplexer (variable I2C address of 0x70, 0x71, and 0x72)
- **Playback Sound Module**: 1 x DY-HL30T high level
- **Speaker**: 1 x 4 Ohm 50 W Visatron Speaker
- **LEDs**: integration soon to come...

Jumper Cables:

- **RFID Tags**: 4 x Adafruit's NTAG203 tags, 1 per clamp on jumper cable pair

## Hardware Architecture (Schematic)

...coming soon...

## Software Architecture

### Behavior Summary

Two systems that handle their own battery and terminal configuration by polling RFID readers in a round-robin approach. RS485 communication is unidirectional from the wall battery system to the toy car, where the main LED strip and sound playback logic lie. State machines defined for seamless and predictable transitions.

### Arduino Leonardo (Wall Battery System)

#### **`WallBatterySystem`** Class

Coordinates all classes, handles round-robin polling of each battery, captures and updates each battery's state, communicates to Toy Car via RS485, handles system health and small UI visualization with LEDs. For future improvements I would break this class down further, but I digress man.

#### **`Battery`** Class

Encapsulates the initialization and update of the two RFID readers per battery, ensures proper I2C communication to the MUX, and provides other handy methods for configuration status, its terminal readers' states, and more.

#### **`TerminalReader`** Class

Handles updates to the state machine of the RFID readers via the `update()` method and reads RFID tag data with the MFRC522 library.

Our four defined states are:

1. TAG_ABSENT: No tag near the reader
2. TAG_DETECTED: Tag found but not yet confirmed (debouncing)
3. TAG_PRESENT: Tag confirmed and data read successfully
4. TAG_REMOVED: Tag was present but now missing (confirming removal)

The `update()` method has 2 main sections:

1. Tag Detection Logic (when a tag IS found)

In the case that `reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()` returns true, a tag has been detected. We quickly check its UID to determine if it is a **new** tag or the same one, and promptly update the timing variables for debouncing. We then check the previous state of the reader's tag data with a switch statement to act accordingly.

TAG_ABSENT -> TAG_DETECTED: first detection of a tag, advance to "detected" state and start debounce timer

```cpp
case TAG_ABSENT:
  tagState = TAG_DETECTED;
  firstSeenTime = currentTime;  // start debounce timer
  Serial.println(": New tag detected!");
```

TAG_DETECTED -> TAG_PRESENT: if the tag is still there after 100ms (debounce time) it's confirmed as real and present, continue to reading its data

```cpp
case TAG_DETECTED:
  if (currentTime - firstSeenTime > TAG_DEBOUNCE_TIME) {
    tagState = TAG_PRESENT;
    Serial.println(": Tag confirmed present");
    readTagData(reader);  // read the tag's data
  }
```

TAG_PRESENT (Same Tag): if it's the same tag, do nothing, if it is a different tag, go back to the TAG_DETECTED state, restart the debouncing timer, and clear the previous tag's data

```cpp
case TAG_PRESENT:
  if (!isSameTag) {
    // different tag detected
    tagState = TAG_DETECTED;
    firstSeenTime = currentTime;
    clearTagData();
  }
  // otherwise same tag still present - no action needed
  break;
```

TAG_REMOVED -> TAG_DETECTED: a tag that was removed has come back (or a new one appeared), start fresh new detection

```cpp
case TAG_REMOVED:
  tagState = TAG_DETECTED;
  firstSeenTime = currentTime;
  Serial.println(": Tag returned!");
```

2. Absence Detection Logic (when a tag is NOT found)

In the case that `reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()` returns false, then the reader did NOT detect a tag. We quickly add to the failure-to-read count and use the current state of the reader to act accordingly.

TAG_DETECTED -> TAG_ABSENT: if we were trying to detect a tag but it disappeared quickly, most likely we had a false positive, go straight to ABSENT

```cpp
if (tagState == TAG_DETECTED) {
  if (consecutiveFails >= 2) {  // Failed 2 times in a row
    tagState = TAG_ABSENT;
    clearTagData();
    Serial.println(": Tag detection failed");
  }
}
```

TAG_PRESENT -> TAG_REMOVED: for an already established tag, we're more lenient, it needs to fail 3 times OR be gone for 1 second before we consider it "removed" (to prevent brief disconnections from resetting everything)

```cpp
else if (tagState == TAG_PRESENT) {
  if (consecutiveFails >= TAG_PRESENCE_THRESHOLD ||  // 3 consecutive fails
      (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT)) {  // OR 1 second timeout
    tagState = TAG_REMOVED;
    Serial.println(": Tag removed!");
  }
}
```

TAG_REMOVED -> TAG_ABSENT: after a tag is marked as remove, wait another little bit to confirm it's really gone, then clear all data and return to ABSENT

```cpp
else if (tagState == TAG_REMOVED) {
  if (currentTime - lastSeenTime > TAG_ABSENCE_TIMEOUT * 2) {
    tagState = TAG_ABSENT;
    clearTagData();
    Serial.println(": Tag removal confirmed");
  }
}
```

#### **`MuxController`** Class

Simple and isolated Mux helpers for switching and disabling channels.

### Toy Car System

#### **`ToyCarSystem`** Class

BRIDGE FUNCTION
because the onPacketReceived() function itself is a class member function,
the compiler automatically adds a hidden 'this' parameter:
void onPacketReceived(ToyCarSystem* this, const WallStatusPacket &pkt)
^^^^^^^^^^^^^^^^^ Hidden parameter!
this doesn't match the callback signature the RS485Receiver expects:
using PacketHandlerFn = void (*)(const WallStatusPacket &, void \*context);

trust me, we could try to modify the callback signature to handle member
functions directly, but that would drag us into POINTER TO MEMBER FUNCTION
territory, which has some scary af syntax and gets complex fast.

instead, this static bridge function acts as an adapter:

1.  takes the standard callback parameters (pkt, ctx)
2.  casts the generic 'ctx' pointer to point back to the correct ToyCarSystem type
3.  calls the real member function onPacketReceived on that object

## Maintenance Notes

- 11/02/2025: far too many power supplies feeding off of one outlet, toy car system now feeds off its own outlet
- 11/12/2025: replaced 12V-5V step down converter with LM2596 based module
- 11/18/2025: switched from an i2s audio system to a simpler trigger based system using a DY-HL30T module
