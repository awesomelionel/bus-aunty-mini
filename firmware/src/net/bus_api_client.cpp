// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "net/bounded_stream.h"
#include "net/certs.h"

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
    // Stream's own timeout is 1000 ms and is what ArduinoJson waits on.
    // ReadBufferingStream is not in ArduinoJson 7.4, which still reads one
    // byte at a time, so there is no buffer to add here.
    capped.setTimeout(8000);
    result.parsed = parseBusArrivalStream(capped, busStopCode);
    http.end();
    return result;
}
