// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <ArduinoJson.h>
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
    if (result.httpStatus == HTTP_CODE_OK) {
        // Use streaming deserialization with filter to reduce peak RAM usage.
        // The filter tells ArduinoJson which fields to keep, discarding others
        // during parse instead of allocating memory for them.
        JsonDocument filter;
        filter["busStops"][0]["BusStopCode"] = true;
        filter["busStops"][0]["Services"][0]["ServiceNo"] = true;
        filter["busStops"][0]["Services"][0]["Loop"]["IsLoop"] = true;
        filter["busStops"][0]["Services"][0]["NextBus"]["EstimatedArrival"] = true;
        filter["busStops"][0]["Services"][0]["NextBus"]["Load"] = true;
        filter["busStops"][0]["Services"][0]["NextBus"]["Type"] = true;
        filter["busStops"][0]["Services"][0]["NextBus"]["VisitNumber"] = true;
        filter["busStops"][0]["Services"][0]["NextBus"]["Label"] = true;
        filter["busStops"][0]["Services"][0]["NextBus2"]["EstimatedArrival"] = true;
        filter["busStops"][0]["Services"][0]["NextBus2"]["Load"] = true;
        filter["busStops"][0]["Services"][0]["NextBus2"]["Type"] = true;
        filter["busStops"][0]["Services"][0]["NextBus2"]["VisitNumber"] = true;
        filter["busStops"][0]["Services"][0]["NextBus2"]["Label"] = true;
        filter["busStops"][0]["Services"][0]["NextBus3"]["EstimatedArrival"] = true;
        filter["busStops"][0]["Services"][0]["NextBus3"]["Load"] = true;
        filter["busStops"][0]["Services"][0]["NextBus3"]["Type"] = true;
        filter["busStops"][0]["Services"][0]["NextBus3"]["VisitNumber"] = true;
        filter["busStops"][0]["Services"][0]["NextBus3"]["Label"] = true;
        
        JsonDocument doc;
        WiFiClient* stream = http.getStreamPtr();
        DeserializationError err = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));
        
        if (!err) {
            // Serialize directly to result.body string
            serializeJson(doc, result.body);
            result.ok = true;
        } else {
            // JSON parse error - report as bad data, not fetch failure
            result.ok = false;
            result.body = "Bad data";
        }
    }

    http.end();

#ifdef ESP32
    // Log heap after fetch
    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t maxAllocAfter = ESP.getMaxAllocHeap();
    Serial.printf("[heap] after fetch: free=%u max_alloc=%u (delta: %d)\n", 
                  heapAfter, maxAllocAfter, 
                  static_cast<int32_t>(heapAfter) - static_cast<int32_t>(heapBefore));
#endif

    return result;
}
