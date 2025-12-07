#include "server.h"
#include "config.h"
#include "core/scanner.h"
#include "core/enumerator.h"
#include "display/ui.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

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
    server.on("/api/screenshot", HTTP_GET, handleScreenshot);

    // Debug endpoints for testing
    server.on("/api/debug/testportal", HTTP_GET, handleTestPortal);

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

    #if DEBUG_SERIAL
    Serial.println("[WEB] Setting up Access Point...");
    Serial.printf("[WEB] SSID: %s\n", apSSID.c_str());
    Serial.printf("[WEB] Password: %s\n", strlen(AP_PASSWORD) > 0 ? AP_PASSWORD : "(open)");
    Serial.printf("[WEB] Hidden: %s\n", AP_HIDDEN ? "yes" : "no");
    #endif

    // Start AP - Set mode first
    WiFi.mode(WIFI_AP_STA);  // AP + Station mode for scanning
    delay(100);  // Give WiFi time to switch modes

    // softAP params: ssid, password, channel, hidden, max_connection
    // Use channel 6 (less crowded than 1)
    bool apStarted;
    if (strlen(AP_PASSWORD) > 0) {
        apStarted = WiFi.softAP(apSSID.c_str(), AP_PASSWORD, 6, AP_HIDDEN, 4);
    } else {
        apStarted = WiFi.softAP(apSSID.c_str(), NULL, 6, AP_HIDDEN, 4);
    }

    #if DEBUG_SERIAL
    Serial.printf("[WEB] softAP() returned: %s\n", apStarted ? "SUCCESS" : "FAILED");
    #endif

    if (apStarted) {
        delay(100);  // Give AP time to initialize
        apIP = WiFi.softAPIP().toString();

        #if DEBUG_SERIAL
        Serial.printf("[WEB] AP started successfully!\n");
        Serial.printf("[WEB] AP SSID: %s\n", apSSID.c_str());
        Serial.printf("[WEB] AP IP: %s\n", apIP.c_str());
        Serial.printf("[WEB] AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
        #endif
    } else {
        #if DEBUG_SERIAL
        Serial.println("[WEB] ERROR: Failed to start AP!");
        #endif
        apIP = "0.0.0.0";
    }
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

void WebServer::handleScreenshot(AsyncWebServerRequest* request) {
    #if DEBUG_SERIAL
    Serial.println("[WEB] Screenshot requested");
    #endif

    TFT_eSPI& tft = UI::getDisplay();
    int width = tft.width();
    int height = tft.height();

    // BMP file size calculation
    // Row size must be padded to 4-byte boundary
    int rowSize = ((width * 3 + 3) / 4) * 4;
    int imageSize = rowSize * height;
    int fileSize = 54 + imageSize;  // 54 byte header + pixel data

    // Allocate buffer for the entire BMP (display is small enough)
    // 320x240x3 = 230KB + 54 header = ~231KB
    uint8_t* bmpBuffer = (uint8_t*)ps_malloc(fileSize);
    if (!bmpBuffer) {
        // Fallback to regular malloc
        bmpBuffer = (uint8_t*)malloc(fileSize);
        if (!bmpBuffer) {
            request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
            return;
        }
    }

    // BMP File Header (14 bytes)
    bmpBuffer[0] = 'B';
    bmpBuffer[1] = 'M';
    bmpBuffer[2] = fileSize & 0xFF;
    bmpBuffer[3] = (fileSize >> 8) & 0xFF;
    bmpBuffer[4] = (fileSize >> 16) & 0xFF;
    bmpBuffer[5] = (fileSize >> 24) & 0xFF;
    bmpBuffer[6] = 0; bmpBuffer[7] = 0;  // Reserved
    bmpBuffer[8] = 0; bmpBuffer[9] = 0;  // Reserved
    bmpBuffer[10] = 54; bmpBuffer[11] = 0; bmpBuffer[12] = 0; bmpBuffer[13] = 0;  // Pixel offset

    // DIB Header (40 bytes - BITMAPINFOHEADER)
    bmpBuffer[14] = 40; bmpBuffer[15] = 0; bmpBuffer[16] = 0; bmpBuffer[17] = 0;  // Header size
    bmpBuffer[18] = width & 0xFF;
    bmpBuffer[19] = (width >> 8) & 0xFF;
    bmpBuffer[20] = (width >> 16) & 0xFF;
    bmpBuffer[21] = (width >> 24) & 0xFF;
    // Height is negative for top-down bitmap
    int negHeight = -height;
    bmpBuffer[22] = negHeight & 0xFF;
    bmpBuffer[23] = (negHeight >> 8) & 0xFF;
    bmpBuffer[24] = (negHeight >> 16) & 0xFF;
    bmpBuffer[25] = (negHeight >> 24) & 0xFF;
    bmpBuffer[26] = 1; bmpBuffer[27] = 0;   // Color planes
    bmpBuffer[28] = 24; bmpBuffer[29] = 0;  // Bits per pixel
    bmpBuffer[30] = 0; bmpBuffer[31] = 0; bmpBuffer[32] = 0; bmpBuffer[33] = 0;  // No compression
    bmpBuffer[34] = (imageSize) & 0xFF;
    bmpBuffer[35] = (imageSize >> 8) & 0xFF;
    bmpBuffer[36] = (imageSize >> 16) & 0xFF;
    bmpBuffer[37] = (imageSize >> 24) & 0xFF;
    bmpBuffer[38] = 0x13; bmpBuffer[39] = 0x0B; bmpBuffer[40] = 0; bmpBuffer[41] = 0;  // H res
    bmpBuffer[42] = 0x13; bmpBuffer[43] = 0x0B; bmpBuffer[44] = 0; bmpBuffer[45] = 0;  // V res
    bmpBuffer[46] = 0; bmpBuffer[47] = 0; bmpBuffer[48] = 0; bmpBuffer[49] = 0;  // Colors
    bmpBuffer[50] = 0; bmpBuffer[51] = 0; bmpBuffer[52] = 0; bmpBuffer[53] = 0;  // Important

    // Read pixels from display
    int bufferPos = 54;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel = tft.readPixel(x, y);

            // Convert RGB565 to RGB888 (BGR order for BMP)
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;

            bmpBuffer[bufferPos++] = b;
            bmpBuffer[bufferPos++] = g;
            bmpBuffer[bufferPos++] = r;
        }

        // Pad row to 4-byte boundary
        int padding = rowSize - (width * 3);
        for (int p = 0; p < padding; p++) {
            bmpBuffer[bufferPos++] = 0;
        }
    }

    // Send response with the buffer - AsyncWebServer will free it
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "image/bmp", bmpBuffer, fileSize
    );
    response->addHeader("Content-Disposition", "inline; filename=\"screenshot.bmp\"");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);

    // Note: bmpBuffer is copied by beginResponse_P, so we can free it
    free(bmpBuffer);
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
}

void WebServer::handleTestPortal(AsyncWebServerRequest* request) {
    // Debug endpoint to inject a fake test portal for testing enumeration
    // Usage: /api/debug/testportal?url=http://192.168.4.2:8080&ssid=TestPortal&type=hotel

    String portalUrl = "http://192.168.4.2:8080";  // Default test_portal.py URL
    String fakeSsid = "TestPortal_DEBUG";
    String portalType = "hotel";

    if (request->hasParam("url")) {
        portalUrl = request->getParam("url")->value();
    }
    if (request->hasParam("ssid")) {
        fakeSsid = request->getParam("ssid")->value();
    }
    if (request->hasParam("type")) {
        portalType = request->getParam("type")->value();
    }

    #if DEBUG_SERIAL
    Serial.printf("[DEBUG] Injecting test portal: %s (SSID: %s, Type: %s)\n",
        portalUrl.c_str(), fakeSsid.c_str(), portalType.c_str());
    #endif

    // Fetch the HTML from the test portal server
    HTTPClient http;
    WiFiClient client;
    String portalHtml = "";

    http.begin(client, portalUrl);
    http.setTimeout(5000);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        portalHtml = http.getString();
        #if DEBUG_SERIAL
        Serial.printf("[DEBUG] Fetched %d bytes from test portal\n", portalHtml.length());
        #endif
    } else {
        #if DEBUG_SERIAL
        Serial.printf("[DEBUG] Failed to fetch test portal: %d\n", httpCode);
        #endif

        // Use sample HTML if fetch fails
        if (portalType == "hotel") {
            portalHtml = R"(
<!DOCTYPE html>
<html><head><title>Hotel WiFi Login</title></head>
<body>
<h1>Welcome to Test Hotel</h1>
<form method="post" action="/login">
    <label>Room Number:</label>
    <input type="text" name="room" placeholder="e.g. 101">
    <label>Last Name:</label>
    <input type="text" name="lastname" placeholder="Guest surname">
    <button type="submit">Connect</button>
</form>
</body></html>
)";
        } else if (portalType == "airport") {
            portalHtml = R"(
<!DOCTYPE html>
<html><head><title>Airport WiFi</title></head>
<body>
<h1>Airport Free WiFi</h1>
<form method="post" action="/login">
    <label>Email:</label>
    <input type="email" name="email" placeholder="your@email.com">
    <label>Flight Number:</label>
    <input type="text" name="flight" placeholder="e.g. AA123">
    <button type="submit">Connect</button>
</form>
</body></html>
)";
        } else {
            portalHtml = R"(
<!DOCTYPE html>
<html><head><title>WiFi Login</title></head>
<body>
<h1>Guest WiFi Access</h1>
<form method="post" action="/login">
    <label>Access Code:</label>
    <input type="text" name="code" placeholder="Enter code">
    <button type="submit">Connect</button>
</form>
</body></html>
)";
        }
    }
    http.end();

    // Create a fake NetworkInfo entry
    NetworkInfo fakeNet;
    fakeNet.ssid = fakeSsid;
    fakeNet.bssid = "DE:AD:BE:EF:00:01";  // Fake BSSID
    fakeNet.rssi = -50;  // Good signal
    fakeNet.channel = 6;
    fakeNet.encryption = WIFI_AUTH_OPEN;
    fakeNet.isOpen = true;
    fakeNet.hasPortal = true;
    fakeNet.analyzed = false;
    fakeNet.portalUrl = portalUrl;
    fakeNet.portalHtml = portalHtml;
    fakeNet.lastSeen = millis();

    // Check if we already have this test network (by SSID)
    auto& networks = Scanner::getNetworks();
    auto& portals = Scanner::getPortals();
    bool found = false;

    for (size_t i = 0; i < networks.size(); i++) {
        if (networks[i].ssid == fakeSsid) {
            // Update existing entry
            networks[i] = fakeNet;
            found = true;

            // Update portals list pointer
            for (size_t j = 0; j < portals.size(); j++) {
                if (portals[j]->ssid == fakeSsid) {
                    portals[j] = &networks[i];
                    break;
                }
            }
            if (portals.empty() || portals.back()->ssid != fakeSsid) {
                portals.push_back(&networks[i]);
            }
            break;
        }
    }

    if (!found) {
        // Add new entry
        networks.push_back(fakeNet);
        portals.push_back(&networks.back());
    }

    #if DEBUG_SERIAL
    Serial.printf("[DEBUG] Test portal injected. Networks: %d, Portals: %d\n",
        networks.size(), portals.size());
    #endif

    // Return success response
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Test portal injected";
    doc["ssid"] = fakeSsid;
    doc["portalUrl"] = portalUrl;
    doc["htmlLength"] = portalHtml.length();
    doc["networkCount"] = networks.size();
    doc["portalCount"] = portals.size();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
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
