// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "net/certs.h"

FetchResult fetchBusArrival(const std::string& busStopCode) {
    FetchResult result;

#ifdef ESP32
    // Log heap before fetch
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t maxAllocBefore = ESP.getMaxAllocHeap();
    Serial.printf("[heap] before fetch: free=%u max_alloc=%u\n", heapBefore, maxAllocBefore);
#endif

    bool heapLogged = false;
    
    WiFiClientSecure client;
    client.setCACert(kGtsRootR4Pem);

    HTTPClient http;
    http.setTimeout(8000);
    http.useHTTP10(true);

    std::string url =
        "https://api.busaunty.com/api/v2/BusArrival?BusStopCode=" + busStopCode;
    if (!http.begin(client, url.c_str())) {
#ifdef ESP32
        uint32_t heapAfter = ESP.getFreeHeap();
        uint32_t maxAllocAfter = ESP.getMaxAllocHeap();
        Serial.printf("[heap] after fetch: free=%u max_alloc=%u (delta: %d)\n", 
                      heapAfter, maxAllocAfter, 
                      static_cast<int32_t>(heapAfter) - static_cast<int32_t>(heapBefore));
        heapLogged = true;
#endif
        return result;
    }

    result.httpStatus = http.GET();
    if (result.httpStatus == HTTP_CODE_OK) {
        // Read response body directly as string
        // arrival_parser.cpp will parse it with its own filter
        result.body = http.getString().c_str();
        result.ok = !result.body.empty();
    }

    http.end();

#ifdef ESP32
    if (!heapLogged) {
        // Log heap after fetch
        uint32_t heapAfter = ESP.getFreeHeap();
        uint32_t maxAllocAfter = ESP.getMaxAllocHeap();
        Serial.printf("[heap] after fetch: free=%u max_alloc=%u (delta: %d)\n", 
                      heapAfter, maxAllocAfter, 
                      static_cast<int32_t>(heapAfter) - static_cast<int32_t>(heapBefore));
    }
#endif

    return result;
}
