// firmware/src/net/config_page.cpp
#include "config_page.h"

#include <cstdio>

#include "core/html_escape.h"

namespace config_page {
namespace {

// A Windows 95 dialog, rebuilt in CSS. The look is not nostalgia for its own
// sake: this page is served to a phone that has joined the device's own access
// point and therefore has no route to the internet, so no webfont, icon set or
// stylesheet can be fetched. Everything here has to come from borders, system
// colours and inline SVG -- which is exactly what that era's chrome was made
// of, so the constraint and the aesthetic want the same thing.
//
// The bevels are the canonical four-layer inset shadows: outer highlight,
// inner highlight, inner shadow, outer shadow. `groove` borders on the group
// boxes are a real CSS border-style that browsers still render the 1995 way.
const char kCss[] =
    "<style>"
    "*{box-sizing:border-box}"
    "html{-webkit-text-size-adjust:100%}"
    "body{margin:0;padding:10px 8px 24px;"
    "font-family:'MS Sans Serif','Microsoft Sans Serif',Tahoma,Geneva,Verdana,"
    "Arial,sans-serif;font-size:13px;line-height:1.35;color:#000;"
    "background-color:#008080;"
    // A 2px dither over the teal, the way a 256-colour desktop faked a shade
    // it did not have.
    "background-image:linear-gradient(45deg,rgba(0,0,0,.07) 25%,"
    "transparent 25% 75%,rgba(0,0,0,.07) 75%),"
    "linear-gradient(45deg,rgba(0,0,0,.07) 25%,transparent 25% 75%,"
    "rgba(0,0,0,.07) 75%);"
    "background-size:4px 4px;background-position:0 0,2px 2px}"
    "svg{image-rendering:pixelated;shape-rendering:crispEdges;"
    "vertical-align:-3px}"
    ".raised{box-shadow:inset -1px -1px 0 #0a0a0a,inset 1px 1px 0 #fff,"
    "inset -2px -2px 0 #808080,inset 2px 2px 0 #dfdfdf}"
    ".sunken{box-shadow:inset -1px -1px 0 #fff,inset 1px 1px 0 #0a0a0a,"
    "inset -2px -2px 0 #dfdfdf,inset 2px 2px 0 #808080}"
    ".win{max-width:540px;margin:0 auto 14px;background:#c0c0c0;padding:3px;"
    "box-shadow:inset -1px -1px 0 #0a0a0a,inset 1px 1px 0 #fff,"
    "inset -2px -2px 0 #808080,inset 2px 2px 0 #dfdfdf}"
    ".bar{background:linear-gradient(90deg,#000080,#1084d0);color:#fff;"
    "font-weight:bold;padding:3px 3px 4px 5px;display:flex;"
    "align-items:center;gap:6px;letter-spacing:.2px}"
    ".bar .sp{margin-left:auto;display:flex;gap:2px}"
    ".bar b{display:block;width:16px;height:14px;background:#c0c0c0;"
    "color:#000;font:bold 11px/12px 'MS Sans Serif',Tahoma,sans-serif;"
    "text-align:center;box-shadow:inset -1px -1px 0 #0a0a0a,"
    "inset 1px 1px 0 #fff,inset -2px -2px 0 #808080,inset 2px 2px 0 #dfdfdf}"
    ".body{padding:12px 11px 11px}"
    "fieldset{border:2px groove #dfdfdf;margin:0 0 14px;padding:10px 10px 4px}"
    "legend{padding:0 4px;font-weight:bold}"
    "p{margin:0 0 9px}"
    ".hint{color:#404040;font-size:12px}"
    "label{display:block;margin-bottom:3px}"
    "input[type=text],input[type=password],select{background:#fff;border:0;"
    "padding:4px 5px;font-family:inherit;font-size:13px;width:100%;"
    "box-shadow:inset -1px -1px 0 #fff,inset 1px 1px 0 #0a0a0a,"
    "inset -2px -2px 0 #dfdfdf,inset 2px 2px 0 #808080}"
    "input:focus,select:focus,button:focus{outline:1px dotted #000;"
    "outline-offset:-4px}"
    ".code{font-family:ui-monospace,'Courier New',monospace;letter-spacing:1px}"
    "button{background:#c0c0c0;border:0;padding:5px 12px;min-width:86px;"
    "font-family:inherit;font-size:13px;color:#000;cursor:pointer;"
    "box-shadow:inset -1px -1px 0 #0a0a0a,inset 1px 1px 0 #fff,"
    "inset -2px -2px 0 #808080,inset 2px 2px 0 #dfdfdf}"
    "button:active{box-shadow:inset 1px 1px 0 #0a0a0a,inset -1px -1px 0 #fff,"
    "inset 2px 2px 0 #808080,inset -2px -2px 0 #dfdfdf;"
    "padding:6px 11px 4px 13px}"
    "button.sm{min-width:0;padding:3px 8px;font-size:12px}"
    "button.sm:active{padding:4px 7px 2px 9px}"
    "button.go{font-weight:bold}"
    ".row{display:grid;grid-template-columns:34px 1fr 1fr;gap:6px;"
    "align-items:center;margin-bottom:7px}"
    ".row .n{font-weight:bold;text-align:center}"
    ".net{background:#fff;padding:7px 8px;margin-bottom:7px;"
    "box-shadow:inset -1px -1px 0 #fff,inset 1px 1px 0 #0a0a0a,"
    "inset -2px -2px 0 #dfdfdf,inset 2px 2px 0 #808080}"
    ".net .t{display:flex;align-items:center;gap:6px;margin-bottom:6px}"
    ".net .t strong{word-break:break-all}"
    ".net .t .ix{background:#000080;color:#fff;font-size:11px;padding:1px 5px}"
    ".acts{display:flex;flex-wrap:wrap;gap:4px}"
    ".acts input[type=password]{flex:1 1 130px;width:auto}"
    ".err{background:#c0c0c0;padding:9px 10px;margin:0 0 12px;display:flex;"
    "gap:9px;align-items:flex-start;"
    "box-shadow:inset -1px -1px 0 #fff,inset 1px 1px 0 #0a0a0a,"
    "inset -2px -2px 0 #dfdfdf,inset 2px 2px 0 #808080}"
    ".note{background:#ffffe1;padding:8px 10px;margin:0 0 12px;"
    "border:1px solid #000;font-size:12px}"
    ".foot{display:flex;flex-wrap:wrap;gap:7px;justify-content:flex-end;"
    "padding-top:3px}"
    ".status{margin-top:10px;display:flex;gap:3px}"
    ".status span{background:#c0c0c0;padding:3px 7px;font-size:12px;"
    "color:#404040;box-shadow:inset -1px -1px 0 #fff,inset 1px 1px 0 #808080}"
    ".status span:first-child{flex:1}"
    "</style>";

// 16x16, drawn on whole pixels so it keeps the blocky look of an icon from a
// machine that only had 16 colours to spend.
const char kBusIcon[] =
    "<svg width='16' height='16' viewBox='0 0 16 16' aria-hidden='true'>"
    "<rect x='2' y='2' width='12' height='10' fill='#ffff00'/>"
    "<rect x='3' y='4' width='4' height='3' fill='#1084d0'/>"
    "<rect x='9' y='4' width='4' height='3' fill='#1084d0'/>"
    "<rect x='3' y='9' width='10' height='2' fill='#404040'/>"
    "<rect x='3' y='12' width='3' height='2' fill='#000'/>"
    "<rect x='10' y='12' width='3' height='2' fill='#000'/>"
    "</svg>";

const char kWarnIcon[] =
    "<svg width='16' height='16' viewBox='0 0 16 16' aria-hidden='true'>"
    "<rect x='7' y='1' width='2' height='2' fill='#ff0000'/>"
    "<rect x='6' y='3' width='4' height='2' fill='#ff0000'/>"
    "<rect x='5' y='5' width='6' height='2' fill='#ff0000'/>"
    "<rect x='4' y='7' width='8' height='2' fill='#ff0000'/>"
    "<rect x='3' y='9' width='10' height='2' fill='#ff0000'/>"
    "<rect x='2' y='11' width='12' height='2' fill='#ff0000'/>"
    "<rect x='7' y='5' width='2' height='4' fill='#fff'/>"
    "<rect x='7' y='10' width='2' height='2' fill='#fff'/>"
    "</svg>";

// The page is accumulated here and sent as one response carrying a real
// Content-Length.
//
// It used to stream out in about thirty chunked writes, to spare the heap.
// That works in an ordinary browser and truncates inside the captive-portal
// mini-browsers: they cope badly with chunked transfer encoding and stop
// reading early, so whatever sat at the tail of the page -- Display Options
// and the status bar -- simply never arrived. Buffering costs ~12KB on a
// device using a sixth of its RAM, which is much the cheaper side of that
// trade.
String pageBuffer;

void chunk(WebServer&, const String& html) { pageBuffer += html; }

void openDocument(WebServer& server, const char* title) {
    pageBuffer = "";
    // One allocation up front rather than a reallocation per section as the
    // page grows past it.
    pageBuffer.reserve(12288);
    String head = "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,"
                  "initial-scale=1'><title>";
    head += title;
    head += "</title>";
    head += kCss;
    head += "</head><body>";
    chunk(server, head);
}

void closeDocument(WebServer& server) {
    chunk(server, "</body></html>");
    server.send(200, "text/html; charset=utf-8", pageBuffer);
    // Hand the heap straight back instead of holding a page-sized String
    // until whenever the next request happens to arrive.
    pageBuffer = String();
}

void openWindow(WebServer& server, const char* title) {
    String html = "<div class='win'><div class='bar'>";
    html += kBusIcon;
    html += "<span>";
    html += title;
    html += "</span><span class='sp'><b>_</b><b>&#9633;</b><b>&times;</b>"
            "</span></div><div class='body'>";
    chunk(server, html);
}

void closeWindow(WebServer& server) { chunk(server, "</div></div>"); }

void writeStopRows(WebServer& server, const std::vector<BusStopConfig>& stops) {
    for (size_t i = 0; i < kMaxBusStops; ++i) {
        const char* code = i < stops.size() ? stops[i].code.c_str() : "";
        const char* name = i < stops.size() ? stops[i].name.c_str() : "";
        const unsigned n = static_cast<unsigned>(i + 1);

        String row = "<div class='row'><span class='n'>";
        row += String(n);
        row += "</span>";
        row += "<input class='code' form='setup' type='text' name='code";
        row += String(n);
        row += "' value='";
        row += escapeHtml(code).c_str();
        row += "' maxlength='5' inputmode='numeric' autocomplete='off' "
               "placeholder='00481' aria-label='Stop ";
        row += String(n);
        row += " code'>";
        row += "<input form='setup' type='text' name='name";
        row += String(n);
        row += "' value='";
        row += escapeHtml(name).c_str();
        row += "' maxlength='16' autocomplete='off' placeholder='Home' "
               "aria-label='Stop ";
        row += String(n);
        row += " name'></div>";
        chunk(server, row);
    }
}

void writeNetworkList(WebServer& server,
                      const std::vector<WifiNetwork>& nets) {
    if (nets.empty()) {
        chunk(server,
              "<p class='hint'>No networks saved yet. Add one below.</p>");
        return;
    }
    for (size_t i = 0; i < nets.size(); ++i) {
        const bool open = nets[i].password.empty();
        String html = "<div class='net'><div class='t'><span class='ix'>";
        html += String(static_cast<unsigned>(i + 1));
        html += "</span><strong>";
        html += escapeHtml(nets[i].ssid).c_str();
        html += "</strong></div>";
        html += "<form class='acts' method='post' action='/networks' "
                "accept-charset='utf-8'><input type='hidden' name='index' "
                "value='";
        html += String(static_cast<unsigned>(i));
        html += "'>";
        html += "<input type='password' name='password' value='' "
                "autocomplete='off' placeholder='";
        html += open ? "open network" : "saved &mdash; blank keeps it";
        html += "' aria-label='Password for ";
        html += escapeHtml(nets[i].ssid).c_str();
        html += "'>";
        html += "<button class='sm' name='action' value='save_pass'>Set</button>";
        if (i > 0) {
            html += "<button class='sm' name='action' value='up'>&uarr;</button>";
        }
        if (i + 1 < nets.size()) {
            html +=
                "<button class='sm' name='action' value='down'>&darr;</button>";
        }
        html +=
            "<button class='sm' name='action' value='delete'>Delete</button>";
        html += "</form></div>";
        chunk(server, html);
    }
}

void writeAddNetwork(WebServer& server,
                     const std::vector<std::string>& visibleSsids) {
    String html =
        "<form method='post' action='/networks' accept-charset='utf-8'>"
        "<input type='hidden' name='action' value='add'>"
        "<p><label for='pick'>Nearby network</label>"
        "<select id='pick' name='scan_ssid'>"
        "<option value=''>&mdash; choose &mdash;</option>";
    for (const std::string& ssid : visibleSsids) {
        html += "<option value='";
        html += escapeHtml(ssid).c_str();
        html += "'>";
        html += escapeHtml(ssid).c_str();
        html += "</option>";
    }
    html +=
        "</select></p>"
        "<p><label for='ssid'>&hellip;or type the name</label>"
        "<input id='ssid' type='text' name='ssid' autocomplete='off' "
        "placeholder='Network name'></p>"
        "<p><label for='pw'>Password</label>"
        "<input id='pw' type='password' name='password' autocomplete='off' "
        "placeholder='Leave blank if open'></p>"
        "<div class='foot'><button>Add network</button></div></form>";
    chunk(server, html);
}

}  // namespace

void sendSettings(WebServer& server, const Model& model) {
    openDocument(server, "Bus Aunty Setup");

    // Declared empty and up front so the stop inputs and the buttons at the
    // very bottom can both join it with form=. That is what lets the page read
    // top to bottom -- stops, then WiFi, then one button that commits both --
    // without nesting the per-network forms inside it, which HTML forbids.
    chunk(server,
          "<form id='setup' method='post' action='/stops' "
          "accept-charset='utf-8'></form>");

    openWindow(server, "Bus Aunty Setup");

    if (!model.error.empty()) {
        String err = "<div class='err'>";
        err += kWarnIcon;
        err += "<span>";
        err += escapeHtml(model.error).c_str();
        err += "</span></div>";
        chunk(server, err);
    }

    if (model.apMode) {
        chunk(server,
              "<div class='note'><b>You are connected to the device.</b> "
              "Fill in both steps below, then press <b>Save &amp; "
              "Connect</b>. This network will disappear at that point &mdash; "
              "that is the device joining your WiFi, and the screen takes "
              "over from there.</div>");
    }

    chunk(server,
          "<fieldset><legend>Step 1 &mdash; Bus stops</legend>"
          "<p class='hint'>At least one is required. The code is 3&ndash;5 "
          "digits and keeps its leading zeros, like 00481. The name is "
          "optional and shows on screen instead of the code.</p>");
    writeStopRows(server, *model.stops);
    chunk(server, "</fieldset>");

    chunk(server,
          "<fieldset><legend>Step 2 &mdash; WiFi networks</legend>"
          "<p class='hint'>Tried top first, so put home above a phone "
          "hotspot. 2.4GHz only.</p>");
    writeNetworkList(server, *model.networks);
    chunk(server,
          "<div class='foot'><form method='post' action='/scan'>"
          "<button class='sm'>Rescan</button></form></div>");
    writeAddNetwork(server, model.visibleSsids);
    chunk(server, "</fieldset>");

    chunk(server,
          "<div class='foot'>"
          "<button form='setup' name='action' value='save'>Save stops</button>"
          "<button class='go' form='setup' name='action' value='apply' "
          "formaction='/apply'>Save &amp; Connect</button>"
          "</div>");

    closeWindow(server);

    // A second dialog, because it is a different decision from setup and the
    // era had no qualms about another window.
    openWindow(server, "Display Options");
    chunk(server,
          "<form method='post' action='/display'>"
          "<p class='hint'>The device cannot always tell mains power from a "
          "full battery. Left unticked, a permanently plugged-in device will "
          "still dim and sleep.</p>"
          "<p><label><input type='checkbox' name='alwayson' value='T'");
    chunk(server, model.alwaysOn ? " checked" : "");
    chunk(server, "> Always on (skip dimming and sleep)</label></p>");

    if (model.supportsFramedTheme) {
        chunk(server,
              "<p><label><input type='checkbox' name='win95' value='T'");
        chunk(server, model.win95Theme ? " checked" : "");
        chunk(server,
              "> Windows 95 arrivals screen</label></p>"
              "<p class='hint'>Draws the arrivals as a window &mdash; title "
              "bar, column headings, status bar with the time. Costs one "
              "service row, so four show instead of five. Turns dark by "
              "itself between 19:00 and 07:00.</p>");
    }

    chunk(server, "<div class='foot'><button>Apply</button></div></form>");
    closeWindow(server);

    String status = "<div class='win'><div class='status'><span>";
    status += String(static_cast<unsigned>(model.stops->size()));
    status += " of 4 stops &middot; ";
    status += String(static_cast<unsigned>(model.networks->size()));
    status += " of 5 networks</span><span>";
    status += model.apMode ? "Setup AP" : "On your WiFi";
    status += "</span></div></div>";
    chunk(server, status);

    closeDocument(server);
}

void sendConnecting(WebServer& server, const std::string& ssid) {
    openDocument(server, "Connecting");
    openWindow(server, "Connecting");
    String html = "<p>Settings saved. The device is joining <b>";
    html += ssid.empty() ? "your network" : escapeHtml(ssid).c_str();
    html += "</b>.</p>";
    html +=
        "<p class='hint'>This setup network is shutting down, so this page "
        "will stop responding &mdash; that is expected. Watch the device "
        "screen: it shows the connection and then the next bus times.</p>"
        "<p class='hint'>If it cannot connect, the screen says so and tells "
        "you which button to hold to start setup again.</p>";
    chunk(server, html);
    closeWindow(server);
    closeDocument(server);
}

void sendLocked(WebServer& server) {
    openDocument(server, "Bus Aunty");
    openWindow(server, "Locked");
    chunk(server,
          "<p>Settings are locked.</p>"
          "<p class='hint'>Hold the config button on the device for 3 seconds, "
          "then reload this page. The screen names the button.</p>");
    closeWindow(server);
    closeDocument(server);
}

}  // namespace config_page
