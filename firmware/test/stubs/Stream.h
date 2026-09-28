#pragma once
#include <stddef.h>
#include <stdint.h>

class Print;

// ArduinoJson's stream reader also compiles the Printable converter.
class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print& print) const = 0;
};

// Enough of Arduino's Stream for the host to compile BoundedStream and
// ArduinoJson's stream reader. timedRead matches the core: a read that
// returns -1 is retried until the timeout. millis() advances each call so
// that wait is a loop, not a sleep.
inline unsigned long millis() {
    static unsigned long now = 0;
    return now++;
}

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
};

class Stream : public Print {
public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    void setTimeout(unsigned long timeout) { timeout_ = timeout; }

    size_t readBytes(char* buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            const int c = timedRead();
            if (c < 0) {
                break;
            }
            buffer[count++] = static_cast<char>(c);
        }
        return count;
    }

protected:
    unsigned long timeout_ = 1000;

    int timedRead() {
        const unsigned long start = millis();
        do {
            const int c = read();
            if (c >= 0) {
                return c;
            }
        } while (millis() - start < timeout_);
        return -1;
    }
};
