// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "net/certs.h"

FetchResult fetchBusArrival(const std::string& busStopCode) {
    FetchResult result;

    WiFiClientSecure client;
    client.setCACert(kGtsRootR4Pem);

    HTTPClient http;
    http.setTimeout(8000);

    std::string url =
        "https://api.busaunty.com/api/v1/BusArrival?BusStopCode=" + busStopCode;
    if (!http.begin(client, url.c_str())) {
        return result;
    }

    result.httpStatus = http.GET();
    if (result.httpStatus == HTTP_CODE_OK) {
        result.ok = true;
        result.body = http.getString().c_str();
    }

    http.end();
    return result;
}
