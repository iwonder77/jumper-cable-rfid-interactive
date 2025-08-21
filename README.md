# Jumper Cable RFID Battery Interactive

## Overview

Source code for the Arduino Nano that handles RFID readings of 3 batteries (each with 2 readers under the positive and negative terminals) to determine if jumper cable polarity is correct and acts accordingly. RFID tags are embedded within the jumper cables, I wrote lighteweight data onto them using [this](https://github.com/iwonder77/rw-NTAG203-rfid-tag) sketch to allow us to differentiate between positive and negative jumper cable ends. 

## Hardware Components

### Electronics

-   Microcontroller: Arduino Nano
-   RFID Readers: 6 x RFID2 WS1850S M5Stack readers (fixed I2C address of 0x28) - 2 per battery
-   I2C Multiplexer: 3 x Adafruit's TCA9548A 8 Channel I2C Multiplexer (variable I2C address of 0x70, 0x71, and 0x72) - 1 per battery
-   Status LEDs: Green (correct polarity configuration) and Red (incorrect polarity configuration) indicators
-   RFID Tags: 4 x Adafruit's NTAG203 tags

### Hardware Architecture (Schematic)

...

### Software Architecture

#### **`TerminalReader`** Class

`update()`: The **State Machine**

Four defined states:
1. TAG_ABSENT: No tag near the reader
2. TAG_DETECTED: Tag found but not yet confirmed (debouncing)
3. TAG_PRESENT: Tag confirmed and data read successfully
4. TAG_REMOVED: Tag was present but now missing (confirming removal)

2 main sections:
1. Tag Detection Logic (when a tag IS found)

In the case that `reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()` returns true, a tag has been detected. We quickly check its UID to verify if it is a new one or the same one, and update the timing variables. We then check the previous state of the reader's tag data and use a switch statement to act accordingly.

TAG_ABSENT -> TAG_DETECTED: first detection of a tag, advance to "detected" state and start debounce timer

```cpp
case TAG_ABSENT:
  tagState = TAG_DETECTED;
  firstSeenTime = currentTime;  // Start debounce timer
  Serial.println(": New tag detected!");
```

TAG_DETECTED -> TAG_PRESENT: if the tag is still there after 100ms (debounce time) it's confirmed as real and present, continue to reading its data

```cpp
case TAG_DETECTED:
  if (currentTime - firstSeenTime > TAG_DEBOUNCE_TIME) {
    tagState = TAG_PRESENT;
    Serial.println(": Tag confirmed present");
    readTagData(reader);  // Actually read the tag's data
  }
```
TAG_PRESENT (Same Tag): if it's the same tag, do nothing, if it is a different tag, go back to TAG_DETECTED to debounce this new tag

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

In the case that `reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()` returns false, then the reader did NOT detect a tag. We quickly add to the failure count and use the current state of the reader to act accordingly.

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
#### **`Battery`** Class
