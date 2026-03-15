#include <Arduino.h>
#include "globals.h"

// ============================================================================
// LED Blink Utility (using built-in LED if available)
// ============================================================================

void blinkLED(int times, int delayMs) {
    // XIAO ESP32S3 doesn't have a standard LED_BUILTIN, but we can try
    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
    #endif
}
