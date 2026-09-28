// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "net/certs.h"

namespace {

// Stops a Stream once `left` bytes have been read, so a missing
// Content-Length cannot pull an unbounded body into the JSON parser.
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

}  // namespace

FetchResult fetchBusArrival(const std::string& busStopCode) {
    FetchResult result;

    WiFiClientSecure client;
    client.setCACert(kGtsRootR4Pem);

    HTTPClient http;
    http.setTimeout(8000);
    http.useHTTP10(true);

    std::string url =
        "https://api.busaunty.com/api/v2/BusArrival?BusStopCode=" + busStopCode;
    if (!http.begin(client, url.c_str())) {
        return result;
    }

    result.httpStatus = http.GET();
    if (result.httpStatus != HTTP_CODE_OK) {
        http.end();
        return result;
    }

    const int length = http.getSize();
    if (length > kMaxArrivalBodyBytes) {
        http.end();
        return result;
    }

    BoundedStream capped(http.getStream(),
                         static_cast<size_t>(kMaxArrivalBodyBytes));
    result.parsed = parseBusArrivalStream(capped, busStopCode);
    http.end();
    return result;
}
