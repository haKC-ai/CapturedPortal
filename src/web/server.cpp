#include "server.h"
#include "config.h"
#include "core/scanner.h"
#include "core/enumerator.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Static member initialization
AsyncWebServer WebServer::server(WEB_SERVER_PORT);
bool WebServer::running = false;
String WebServer::apSSID = "";
String WebServer::apIP = "";

// Enumeration progress tracking
static int enumCurrent = 0;
static int enumTotal = 0;
static String enumStatus = "";
static bool enumComplete = false;

void WebServer::init() {
    if (running) return;

    // Initialize SPIFFS for serving web files
    if (!SPIFFS.begin(true)) {
        #if DEBUG_SERIAL
        Serial.println("[WEB] SPIFFS mount failed");
        #endif
    }

    // Setup Access Point
    setupAP();

    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("index.html");

    // API Routes
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/scan", HTTP_GET, handleScan);
    server.on("/api/networks", HTTP_GET, handleNetworks);
    server.on("/api/analyze", HTTP_GET, handleAnalyze);
    server.on("/api/enumerate", HTTP_GET, handleEnumerate);
    server.on("/api/enum/progress", HTTP_GET, handleEnumProgress);
    server.on("/api/llm", HTTP_GET, handleLLM);

    // Fallback
    server.onNotFound(handleNotFound);

    // Start server
    server.begin();
    running = true;

    #if DEBUG_SERIAL
    Serial.println("[WEB] Server started");
    Serial.printf("[WEB] AP SSID: %s\n", apSSID.c_str());
    Serial.printf("[WEB] IP: %s\n", apIP.c_str());
    #endif
}

void WebServer::setupAP() {
    // Generate unique SSID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    apSSID = String(AP_SSID_PREFIX) + String(mac[4], HEX) + String(mac[5], HEX);
    apSSID.toUpperCase();

    // Start AP
    WiFi.mode(WIFI_AP_STA);  // AP + Station mode for scanning

    // softAP params: ssid, password, channel, hidden, max_connection
    if (strlen(AP_PASSWORD) > 0) {
        WiFi.softAP(apSSID.c_str(), AP_PASSWORD, 1, AP_HIDDEN, 4);
    } else {
        WiFi.softAP(apSSID.c_str(), NULL, 1, AP_HIDDEN, 4);
    }

    apIP = WiFi.softAPIP().toString();

    #if DEBUG_SERIAL
    Serial.printf("[WEB] AP started: %s\n", apSSID.c_str());
    Serial.printf("[WEB] AP IP: %s\n", apIP.c_str());
    #endif
}

void WebServer::stop() {
    if (!running) return;

    server.end();
    WiFi.softAPdisconnect(true);
    running = false;

    #if DEBUG_SERIAL
    Serial.println("[WEB] Server stopped");
    #endif
}

bool WebServer::isRunning() {
    return running;
}

String WebServer::getIP() {
    return apIP;
}

String WebServer::getAPSSID() {
    return apSSID;
}

void WebServer::handleRoot(AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/web/index.html", "text/html");
}

void WebServer::handleStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["networkCount"] = Scanner::getNetworkCount();
    doc["portalCount"] = Scanner::getPortalCount();

    // Include network list
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (auto& net : Scanner::getNetworks()) {
        JsonObject netObj = networks.add<JsonObject>();
        netObj["ssid"] = net.ssid;
        netObj["bssid"] = net.bssid;
        netObj["rssi"] = net.rssi;
        netObj["channel"] = net.channel;
        netObj["isOpen"] = net.isOpen;
        netObj["hasPortal"] = net.hasPortal;
        netObj["analyzed"] = net.analyzed;
        if (net.hasPortal) {
            netObj["portalUrl"] = net.portalUrl;
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleScan(AsyncWebServerRequest* request) {
    #if DEBUG_SERIAL
    Serial.println("[WEB] Scan requested");
    #endif

    // Perform scan
    Scanner::scan();

    // Return results
    JsonDocument doc;
    doc["success"] = true;
    doc["count"] = Scanner::getNetworkCount();

    JsonArray networks = doc["networks"].to<JsonArray>();
    for (auto& net : Scanner::getNetworks()) {
        JsonObject netObj = networks.add<JsonObject>();
        netObj["ssid"] = net.ssid;
        netObj["bssid"] = net.bssid;
        netObj["rssi"] = net.rssi;
        netObj["channel"] = net.channel;
        netObj["isOpen"] = net.isOpen;
        netObj["hasPortal"] = net.hasPortal;
        netObj["analyzed"] = net.analyzed;
        if (net.hasPortal) {
            netObj["portalUrl"] = net.portalUrl;
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleNetworks(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (auto& net : Scanner::getNetworks()) {
        JsonObject netObj = networks.add<JsonObject>();
        netObj["ssid"] = net.ssid;
        netObj["bssid"] = net.bssid;
        netObj["rssi"] = net.rssi;
        netObj["channel"] = net.channel;
        netObj["isOpen"] = net.isOpen;
        netObj["hasPortal"] = net.hasPortal;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleAnalyze(AsyncWebServerRequest* request) {
    if (!request->hasParam("ssid")) {
        request->send(400, "application/json", "{\"error\":\"Missing ssid parameter\"}");
        return;
    }

    String ssid = request->getParam("ssid")->value();

    #if DEBUG_SERIAL
    Serial.printf("[WEB] Analyze requested for: %s\n", ssid.c_str());
    #endif

    // Find the network
    NetworkInfo* target = nullptr;
    auto& networks = Scanner::getNetworks();
    for (int i = 0; i < networks.size(); i++) {
        if (networks[i].ssid == ssid) {
            target = &networks[i];
            break;
        }
    }

    if (!target) {
        request->send(404, "application/json", "{\"error\":\"Network not found\"}");
        return;
    }

    // Check for portal
    int idx = target - &networks[0];
    bool hasPortal = Scanner::checkForPortal(idx);

    JsonDocument doc;
    doc["success"] = hasPortal;
    doc["ssid"] = target->ssid;
    doc["hasPortal"] = target->hasPortal;
    doc["portalUrl"] = target->portalUrl;
    doc["portalHtml"] = target->portalHtml.length() > 0 ?
        target->portalHtml.substring(0, 500) + "..." : "";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleEnumerate(AsyncWebServerRequest* request) {
    if (!request->hasParam("ssid")) {
        request->send(400, "application/json", "{\"error\":\"Missing ssid parameter\"}");
        return;
    }

    String ssid = request->getParam("ssid")->value();
    int maxAttempts = 50;
    if (request->hasParam("max")) {
        maxAttempts = request->getParam("max")->value().toInt();
    }

    #if DEBUG_SERIAL
    Serial.printf("[WEB] Enumerate requested for: %s (max: %d)\n", ssid.c_str(), maxAttempts);
    #endif

    // Find the portal
    NetworkInfo* target = nullptr;
    for (auto* portal : Scanner::getPortals()) {
        if (portal->ssid == ssid) {
            target = portal;
            break;
        }
    }

    if (!target) {
        request->send(404, "application/json", "{\"error\":\"Portal not found\"}");
        return;
    }

    // Reset progress
    enumCurrent = 0;
    enumTotal = maxAttempts;
    enumStatus = "Starting...";
    enumComplete = false;

    // Set progress callback
    Enumerator::setProgressCallback([](int current, int total, const String& status) {
        enumCurrent = current;
        enumTotal = total;
        enumStatus = status;
    });

    // Run enumeration
    EnumResult result = Enumerator::enumerate(target, maxAttempts);
    enumComplete = true;

    // Build response
    JsonDocument doc;
    doc["success"] = true;
    doc["totalAttempts"] = result.totalAttempts;
    doc["successfulAttempts"] = result.successfulAttempts;
    doc["failedAttempts"] = result.failedAttempts;
    doc["estimatedRoomCount"] = result.estimatedRoomCount;
    doc["venueInsights"] = result.venueInsights;

    JsonArray successes = doc["successes"].to<JsonArray>();
    for (const auto& s : result.successes) {
        JsonObject sObj = successes.add<JsonObject>();
        sObj["fieldValues"] = s.fieldValues;
        sObj["success"] = s.success;
    }

    JsonArray patterns = doc["discoveredPatterns"].to<JsonArray>();
    for (const auto& p : result.discoveredPatterns) {
        patterns.add(p);
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleEnumProgress(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["current"] = enumCurrent;
    doc["total"] = enumTotal;
    doc["status"] = enumStatus;
    doc["complete"] = enumComplete;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleLLM(AsyncWebServerRequest* request) {
    if (!request->hasParam("ssid")) {
        request->send(400, "application/json", "{\"error\":\"Missing ssid parameter\"}");
        return;
    }

    String ssid = request->getParam("ssid")->value();

    #if DEBUG_SERIAL
    Serial.printf("[WEB] LLM analysis requested for: %s\n", ssid.c_str());
    #endif

    // Find the portal
    NetworkInfo* target = nullptr;
    for (auto* portal : Scanner::getPortals()) {
        if (portal->ssid == ssid) {
            target = portal;
            break;
        }
    }

    if (!target) {
        request->send(404, "application/json", "{\"error\":\"Portal not found\"}");
        return;
    }

    // Analyze form fields
    auto fields = Enumerator::analyzePortalForm(target->portalHtml);

    // TODO: Integrate actual LLM inference
    // For now, return analyzed form structure

    JsonDocument doc;
    doc["success"] = true;
    doc["ssid"] = target->ssid;

    // Detect venue type from HTML hints
    String html = target->portalHtml;
    html.toLowerCase();

    if (html.indexOf("hotel") >= 0 || html.indexOf("room") >= 0) {
        doc["venueType"] = "Hotel/Hospitality";
    } else if (html.indexOf("airport") >= 0 || html.indexOf("flight") >= 0) {
        doc["venueType"] = "Airport";
    } else if (html.indexOf("cafe") >= 0 || html.indexOf("coffee") >= 0) {
        doc["venueType"] = "Cafe/Restaurant";
    } else if (html.indexOf("hospital") >= 0 || html.indexOf("patient") >= 0) {
        doc["venueType"] = "Healthcare";
    } else if (html.indexOf("conference") >= 0 || html.indexOf("event") >= 0) {
        doc["venueType"] = "Conference/Event";
    } else {
        doc["venueType"] = "Unknown";
    }

    // Extract venue name hints
    // Look for title tag
    int titleStart = target->portalHtml.indexOf("<title>");
    int titleEnd = target->portalHtml.indexOf("</title>");
    if (titleStart >= 0 && titleEnd > titleStart) {
        doc["venueName"] = target->portalHtml.substring(titleStart + 7, titleEnd);
    } else {
        doc["venueName"] = "Unknown";
    }

    // Form fields
    JsonArray formFields = doc["formFields"].to<JsonArray>();
    for (const auto& field : fields) {
        String fieldDesc = field.name;
        switch (field.detectedType) {
            case FIELD_ROOM_NUMBER: fieldDesc += " (Room Number)"; break;
            case FIELD_LAST_NAME: fieldDesc += " (Last Name)"; break;
            case FIELD_FIRST_NAME: fieldDesc += " (First Name)"; break;
            case FIELD_EMAIL: fieldDesc += " (Email)"; break;
            case FIELD_PHONE: fieldDesc += " (Phone)"; break;
            case FIELD_CODE: fieldDesc += " (Access Code)"; break;
            default: break;
        }
        formFields.add(fieldDesc);
    }

    // Analysis summary
    String analysis = "Portal uses ";
    bool hasRoom = false, hasName = false;
    for (const auto& field : fields) {
        if (field.detectedType == FIELD_ROOM_NUMBER) hasRoom = true;
        if (field.detectedType == FIELD_LAST_NAME) hasName = true;
    }

    if (hasRoom && hasName) {
        analysis += "room number + last name authentication. ";
        analysis += "This is typical for hotels. Enumeration may reveal guest information.";
    } else if (hasRoom) {
        analysis += "room number only authentication. ";
        analysis += "Weak security - any valid room number grants access.";
    } else if (hasName) {
        analysis += "name-based authentication. ";
        analysis += "May be vulnerable to common surname enumeration.";
    } else {
        analysis += "unknown authentication method. ";
        analysis += "Further analysis required.";
    }

    doc["analysis"] = analysis;

    target->analyzed = true;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
}

String WebServer::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}
