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

// Analog pin constants (Uno: A0=14 … A5=19)
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19

// Mock pin state — covers digital (0-13) and analog (14-19) pins
// analogRead(pin) reads mock_pin_state[pin], so set it to 0-1023 for pot tests
extern int  mock_pin_state[20];
// Optional second state applied after delay() — used to simulate noisy/transient reads
extern int  mock_pin_state_post_delay[20];
extern bool mock_pin_change_on_delay;

// Controllable millis() clock for timer-based tests
extern unsigned long mock_millis_value;

inline int  digitalRead(int pin) { return mock_pin_state[pin]; }
inline int  analogRead(int pin)  { return mock_pin_state[pin]; }
inline void pinMode(int, int) {}
inline void delay(unsigned long) {
    if (mock_pin_change_on_delay)
        for (int i = 0; i < 20; i++) mock_pin_state[i] = mock_pin_state_post_delay[i];
}
inline unsigned long millis() { return mock_millis_value; }
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
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
