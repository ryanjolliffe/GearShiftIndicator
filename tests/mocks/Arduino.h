#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>

// Arduino primitive types
typedef uint8_t  byte;
typedef bool     boolean;

// Arduino constants
#define HIGH 1
#define LOW  0
#define INPUT 0
#define INPUT_PULLUP 2

// PROGMEM is a no-op on desktop
#define PROGMEM

// Mock digitalRead state - indexed by pin number
extern int mock_pin_state[14];

inline int  digitalRead(int pin) { return mock_pin_state[pin]; }
inline void pinMode(int, int) {}
inline int  analogRead(int) { return 0; }
inline void delay(unsigned long) {}
inline long random(long maxval) { return rand() % maxval; }
inline void randomSeed(unsigned long seed) { srand((unsigned int)seed); }

// Minimal Arduino String mock (enough to compile debugFunction)
class String {
public:
    std::string s;
    String()                    {}
    String(const char* cs)      : s(cs ? cs : "") {}  // implicit — matches Arduino API
    String(int v)               : s(std::to_string(v)) {}
    String(unsigned char v)     : s(std::to_string((int)v)) {}
    String(const String& o)     : s(o.s) {}
    String operator+(const String& o)  const { String r; r.s = s + o.s; return r; }
    String operator+(const char* cs)   const { String r; r.s = s + cs;  return r; }
    String& operator=(const String& o)       { s = o.s; return *this; }
    String& operator+=(const String& o)      { s += o.s; return *this; }
};
inline String operator+(const char* lhs, const String& rhs) {
    String r(lhs); r.s += rhs.s; return r;
}

// Minimal Serial mock
struct SerialMock {
    void begin(int) {}
    void println(const String&) {}
    void println(const char*) {}
} Serial;
