#pragma once
#include <stddef.h>

// Faithful reimplementation of rlogiacco/CircularBuffer used by the sketch.
// Supports: push(), last(), operator[], isFull(), clear()
template<typename T, size_t S>
class CircularBuffer {
    T      buf[S];
    size_t head  = 0;   // index of oldest element
    size_t count = 0;
public:
    void push(T val) {
        if (count < S) {
            buf[(head + count) % S] = val;
            count++;
        } else {
            buf[head] = val;
            head = (head + 1) % S;
        }
    }

    T last() const {
        return buf[(head + count - 1) % S];
    }

    T operator[](size_t i) const {
        return buf[(head + i) % S];
    }

    bool isFull() const { return count == S; }

    void clear() { head = 0; count = 0; }

    size_t size() const { return count; }
};
