#pragma once

#define DEBUG_LEVEL 0 // 0 = none, 1 = debugging mode (use serial monitor)

// Error-level messages (always more important)
#if DEBUG_LEVEL >= 1
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif
