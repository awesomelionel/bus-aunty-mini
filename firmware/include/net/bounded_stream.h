#pragma once
#include <stddef.h>

#include <Stream.h>

// Caps how many bytes a reader can pull from an inner stream. The Arduino
// Stream timeout stays at its 1000 ms default unless the caller sets it;
// ArduinoJson's reader uses that timeout on every readBytes().
class BoundedStream : public Stream {
public:
    BoundedStream(Stream& inner, size_t cap) : inner_(inner), left_(cap) {}

    int available() override {
        if (left_ == 0) {
            return 0;
        }
        const int n = inner_.available();
        if (n <= 0) {
            return 0;
        }
        if (static_cast<size_t>(n) > left_) {
            return static_cast<int>(left_);
        }
        return n;
    }

    int read() override {
        if (left_ == 0) {
            return -1;
        }
        const int c = inner_.read();
        if (c >= 0) {
            --left_;
        }
        return c;
    }

    int peek() override {
        if (left_ == 0) {
            return -1;
        }
        return inner_.peek();
    }

    size_t write(uint8_t) override { return 0; }

private:
    Stream& inner_;
    size_t left_;
};
