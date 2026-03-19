#pragma once
#include "MD_MAX72xx.h"
#include <stdint.h>

// Text alignment
enum textPosition_t { PA_LEFT, PA_CENTER, PA_RIGHT };

// Text/sprite effects
enum textEffect_t {
    PA_PRINT, PA_NO_EFFECT,
    PA_SCROLL_LEFT, PA_SCROLL_RIGHT, PA_SCROLL_UP, PA_SCROLL_DOWN,
    PA_SPRITE
};

// Call counters — reset between tests via parola_reset_counts()
inline int     parola_setSpriteData_count  = 0;  // incremented when animation plays
inline int     parola_displayScroll_count  = 0;  // incremented when scroll text plays
inline int     parola_displayText_count    = 0;
inline int     parola_displayAnimate_count = 0;
inline uint8_t parola_setIntensity_last    = 0;  // last value passed to setIntensity()

inline void parola_reset_counts() {
    parola_setSpriteData_count  = 0;
    parola_displayScroll_count  = 0;
    parola_displayText_count    = 0;
    parola_displayAnimate_count = 0;
    parola_setIntensity_last    = 0;
}

class MD_Parola {
public:
    MD_Parola(MD_MAX72XX::hardwareType_t, uint8_t, uint8_t) {}

    void begin()                     {}
    void setIntensity(uint8_t i)     { parola_setIntensity_last = i; }
    void displayClear()              {}
    void displayReset()              {}

    // Always returns true so that while(!displayAnimate()) loops terminate immediately
    bool displayAnimate()            { parola_displayAnimate_count++; return true; }

    void displayText(char*, textPosition_t, uint16_t, uint16_t,
                     textEffect_t, textEffect_t) {
        parola_displayText_count++;
    }

    void displayScroll(const char*, textPosition_t, textEffect_t, uint16_t) {
        parola_displayScroll_count++;
    }

    void setSpriteData(const uint8_t*, uint8_t, uint8_t,
                       const uint8_t*, uint8_t, uint8_t) {
        parola_setSpriteData_count++;
    }
};
