// firmware/src/net/wifi_portal.cpp
#include "net/wifi_portal.h"

#include <WiFiManager.h>

#include <array>
#include <memory>

namespace {

// WiFiManagerParameter keeps the id, label and custom-HTML pointers it is
// handed rather than copying them, so all of these need static lifetime.
constexpr const char* kCodeIds[kMaxBusStops] = {"code1", "code2", "code3",
                                                "code4"};
constexpr const char* kNameIds[kMaxBusStops] = {"name1", "name2", "name3",
                                                "name4"};
constexpr const char* kCodeLabels[kMaxBusStops] = {
    "Stop 1 code", "Stop 2 code", "Stop 3 code", "Stop 4 code"};
constexpr const char* kNameLabels[kMaxBusStops] = {
    "Stop 1 name (optional)", "Stop 2 name (optional)",
    "Stop 3 name (optional)", "Stop 4 name (optional)"};
constexpr char kIntroHtml[] =
    "<p>Up to 4 bus stops. The code is required: 3-5 digits, keeping any "
    "leading zeros (e.g. 00481). The name is optional and falls back to the "
    "code. A name with no code is ignored.</p>";
// The template already emits maxlength, so only extra attributes go here. The
// pattern only applies to non-empty fields, so unused slots stay valid.
constexpr char kCodeAttrs[] =
    "type='text' inputmode='numeric' pattern='[0-9]{3,5}' placeholder='00481'";
constexpr char kNameAttrs[] = "placeholder='Home'";

constexpr int kCodeFieldLen = static_cast<int>(kBusStopCodeMaxDigits);
constexpr int kNameFieldLen = static_cast<int>(kBusStopNameMaxChars);
constexpr unsigned long kPortalTimeoutSec = 180;

// Owns the parameter objects for as long as the portal is running, and reads
// the submitted values back out afterwards.
class BusStopForm {
  public:
    BusStopForm(WiFiManager& wm, const std::vector<BusStopConfig>& seed)
        : intro_(kIntroHtml) {
        wm.addParameter(&intro_);
        for (size_t i = 0; i < kMaxBusStops; ++i) {
            const char* code = i < seed.size() ? seed[i].code.c_str() : "";
            const char* name = i < seed.size() ? seed[i].name.c_str() : "";
            codes_[i] = std::make_unique<WiFiManagerParameter>(
                kCodeIds[i], kCodeLabels[i], code, kCodeFieldLen, kCodeAttrs);
            names_[i] = std::make_unique<WiFiManagerParameter>(
                kNameIds[i], kNameLabels[i], name, kNameFieldLen, kNameAttrs);
            wm.addParameter(codes_[i].get());
            wm.addParameter(names_[i].get());
        }
    }

    std::vector<BusStopConfig> stops() const {
        std::vector<BusStopConfig> rows;
        rows.reserve(kMaxBusStops);
        for (size_t i = 0; i < kMaxBusStops; ++i) {
            rows.push_back({codes_[i]->getValue(), names_[i]->getValue()});
        }
        return buildBusStopList(rows);
    }

  private:
    WiFiManagerParameter intro_;
    std::array<std::unique_ptr<WiFiManagerParameter>, kMaxBusStops> codes_;
    std::array<std::unique_ptr<WiFiManagerParameter>, kMaxBusStops> names_;
};

bool runPortal(const char* apSsid, std::vector<BusStopConfig>* stops,
               const PortalStartedCallback& onPortalStarted, bool onDemand) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(kPortalTimeoutSec);
    // Give the bus stops their own "Setup" page. Left on the WiFi page, a
    // returning user would have to retype their WiFi password just to edit a
    // stop, risking wiping working credentials.
    wm.setParamsPage(true);
    if (onPortalStarted) {
        wm.setAPCallback(
            [&onPortalStarted](WiFiManager*) { onPortalStarted(); });
    }

    BusStopForm form(wm, *stops);
    bool saved = false;
    wm.setSaveParamsCallback([&saved]() { saved = true; });

    bool connected =
        onDemand ? wm.startConfigPortal(apSsid) : wm.autoConnect(apSsid);

    if (saved) {
        *stops = form.stops();
    }
    // On the initial run the caller cares about WiFi; on demand it only cares
    // whether the stops were edited.
    return onDemand ? saved : connected;
}

}  // namespace

bool wifiPortalConnect(const char* apSsid, std::vector<BusStopConfig>* stops,
                       const PortalStartedCallback& onPortalStarted) {
    return runPortal(apSsid, stops, onPortalStarted, /*onDemand=*/false);
}

bool wifiPortalReconfigure(const char* apSsid,
                           std::vector<BusStopConfig>* stops,
                           const PortalStartedCallback& onPortalStarted) {
    return runPortal(apSsid, stops, onPortalStarted, /*onDemand=*/true);
}
