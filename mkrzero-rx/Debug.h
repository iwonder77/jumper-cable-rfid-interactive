#pragma once
/**
 * Debug.h
 *
 * Simple debug logging macro (same pattern used in the Leonardo code).
 * Set DEBUG_LEVEL to 0 to compile out debug prints.
 *
 * Note: On MKRZero Serial refers to the USB CDC serial port used for logs.
 */

#define DEBUG_LEVEL 1

#if DEBUG_LEVEL >= 1
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif
