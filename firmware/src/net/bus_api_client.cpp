// firmware/src/net/bus_api_client.cpp
#include "net/bus_api_client.h"

#include <ArduinoJson.h>
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
        "https://api.busaunty.com/api/v2/BusArrival?BusStopCode=" + busStopCode;
    if (!http.begin(client, url.c_str())) {
        return result;
    }

    result.httpStatus = http.GET();
    if (result.httpStatus == HTTP_CODE_OK) {
        result.ok = true;
        
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
            // Serialize filtered doc back to string for parser compatibility
            serializeJson(doc, result.body);
        } else {
            result.ok = false;
        }
    }

    http.end();
    return result;
}
