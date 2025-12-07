#include "scanner.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Static member initialization
std::vector<NetworkInfo> Scanner::networks;
std::vector<NetworkInfo*> Scanner::portals;
int Scanner::currentNetwork = -1;
bool Scanner::connected = false;

// Portal check URLs
const char* portalCheckUrls[] = PORTAL_CHECK_URLS;
const int portalCheckUrlCount = sizeof(portalCheckUrls) / sizeof(portalCheckUrls[0]);

void Scanner::init() {
    networks.reserve(MAX_NETWORKS);
    portals.reserve(20);

    #if DEBUG_SERIAL && DEBUG_WIFI
    Serial.println("[SCANNER] Initialized");
    #endif
}

void Scanner::scan() {
    static bool scanInProgress = false;
    static unsigned long scanStartTime = 0;

    // Check if async scan is already in progress
    int scanResult = WiFi.scanComplete();

    // Scan actively running - wait for it
    if (scanResult == WIFI_SCAN_RUNNING) {
        // Check for timeout
        if (millis() - scanStartTime > 10000) {
            #if DEBUG_SERIAL
            Serial.println("[SCANNER] Scan timeout, resetting...");
            #endif
            WiFi.scanDelete();
            scanInProgress = false;
        }
        return;
    }

    // Scan completed with results - process them first
    if (scanResult >= 0) {
        scanInProgress = false;
        // Fall through to process results below
    }
    // No scan running or scan failed - start a new one
    else if (scanResult == WIFI_SCAN_FAILED || scanResult == -2) {
        if (!scanInProgress) {
            #if DEBUG_SERIAL && DEBUG_WIFI
            Serial.println("[SCANNER] Starting async scan...");
            #endif
            WiFi.scanNetworks(true, true);  // async=true, show_hidden=true
            scanInProgress = true;
            scanStartTime = millis();
        } else {
            // Scan was started but not running yet - give it more time
            if (millis() - scanStartTime > 5000) {
                // Took too long to start, reset
                #if DEBUG_SERIAL
                Serial.println("[SCANNER] Scan failed to start, retrying...");
                #endif
                scanInProgress = false;
            }
        }
        return;
    }

    // Process scan results (scanResult >= 0)
    if (scanResult < 0) {
        return;  // Safety check
    }

    int numNetworks = scanResult;

    #if DEBUG_SERIAL && DEBUG_WIFI
    Serial.printf("[SCANNER] Found %d networks\n", numNetworks);
    #endif

    // Clear portals list (will rebuild)
    portals.clear();

    // Process each network
    for (int i = 0; i < numNetworks && i < MAX_NETWORKS; i++) {
        String ssid = WiFi.SSID(i);
        String bssid = WiFi.BSSIDstr(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t channel = WiFi.channel(i);
        wifi_auth_mode_t encryption = WiFi.encryptionType(i);
        bool isOpen = (encryption == WIFI_AUTH_OPEN);

        // Check if we already know this network
        bool found = false;
        for (auto& net : networks) {
            if (net.bssid == bssid) {
                // Update existing entry
                net.rssi = rssi;
                net.lastSeen = millis();
                found = true;

                // Track portals
                if (net.hasPortal) {
                    portals.push_back(&net);
                }
                break;
            }
        }

        // Add new network
        if (!found && networks.size() < MAX_NETWORKS) {
            NetworkInfo net;
            net.ssid = ssid.length() > 0 ? ssid : "[Hidden]";
            net.bssid = bssid;
            net.rssi = rssi;
            net.channel = channel;
            net.encryption = encryption;
            net.isOpen = isOpen;
            net.hasPortal = false;
            net.analyzed = false;
            net.lastSeen = millis();

            networks.push_back(net);

            #if DEBUG_SERIAL && DEBUG_WIFI
            Serial.printf("  [%d] %s (%s) %ddBm CH%d %s\n",
                i,
                net.ssid.c_str(),
                net.bssid.c_str(),
                net.rssi,
                net.channel,
                net.isOpen ? "OPEN" : "SECURED"
            );
            #endif
        }
    }

    // Clean up scan results from WiFi library
    WiFi.scanDelete();

    #if DEBUG_SERIAL && DEBUG_WIFI
    Serial.printf("[SCANNER] Tracking %d networks, %d with portals\n",
        networks.size(), portals.size());
    #endif
}

bool Scanner::checkForPortal(int networkIndex) {
    if (networkIndex < 0 || networkIndex >= networks.size()) {
        return false;
    }

    NetworkInfo& net = networks[networkIndex];

    // Only check open networks
    if (!net.isOpen) {
        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.printf("[PORTAL] Skipping secured network: %s\n", net.ssid.c_str());
        #endif
        return false;
    }

    #if DEBUG_SERIAL && DEBUG_PORTAL
    Serial.printf("[PORTAL] Checking %s for captive portal...\n", net.ssid.c_str());
    #endif

    // Connect to the network
    if (!connectToNetwork(networkIndex)) {
        return false;
    }

    // Wait for connection
    int timeout = 10000;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(100);
        timeout -= 100;
    }

    if (WiFi.status() != WL_CONNECTED) {
        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.println("[PORTAL] Connection failed");
        #endif
        disconnect();
        return false;
    }

    #if DEBUG_SERIAL && DEBUG_PORTAL
    Serial.printf("[PORTAL] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    #endif

    // Check for captive portal
    bool hasPortal = detectCaptivePortal();
    net.hasPortal = hasPortal;

    if (hasPortal) {
        #if DEBUG_SERIAL
        Serial.printf("[PORTAL] *** CAPTIVE PORTAL DETECTED on %s ***\n", net.ssid.c_str());
        #endif

        // Capture the portal page
        if (net.portalUrl.length() > 0) {
            net.portalHtml = capturePortalPage(net.portalUrl);
        }

        // Add to portals list
        portals.push_back(&net);
    }

    disconnect();
    return hasPortal;
}

bool Scanner::detectCaptivePortal() {
    HTTPClient http;
    WiFiClient client;

    for (int i = 0; i < portalCheckUrlCount; i++) {
        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.printf("[PORTAL] Testing: %s\n", portalCheckUrls[i]);
        #endif

        http.begin(client, portalCheckUrls[i]);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setTimeout(PORTAL_CHECK_TIMEOUT);

        int httpCode = http.GET();

        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.printf("[PORTAL] Response code: %d\n", httpCode);
        #endif

        // Check for redirect (captive portal typically redirects)
        if (httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
            httpCode == HTTP_CODE_FOUND ||
            httpCode == HTTP_CODE_SEE_OTHER ||
            httpCode == HTTP_CODE_TEMPORARY_REDIRECT ||
            httpCode == HTTP_CODE_PERMANENT_REDIRECT) {

            String location = http.header("Location");
            if (location.length() > 0) {
                #if DEBUG_SERIAL && DEBUG_PORTAL
                Serial.printf("[PORTAL] Redirect to: %s\n", location.c_str());
                #endif

                // Store the portal URL
                if (currentNetwork >= 0 && currentNetwork < networks.size()) {
                    networks[currentNetwork].portalUrl = location;
                }

                http.end();
                return true;
            }
        }

        // Check for 200 but with different content (some portals don't redirect)
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            // Google's check returns 204 on success, 200 with content means portal
            if (String(portalCheckUrls[i]).indexOf("generate_204") >= 0) {
                http.end();
                return true;  // Got 200 instead of 204 = portal
            }

            // Check for common portal indicators in response
            if (payload.indexOf("<html") >= 0 ||
                payload.indexOf("login") >= 0 ||
                payload.indexOf("accept") >= 0 ||
                payload.indexOf("terms") >= 0) {

                // This might be a portal injection
                if (currentNetwork >= 0 && currentNetwork < networks.size()) {
                    networks[currentNetwork].portalUrl = portalCheckUrls[i];
                    networks[currentNetwork].portalHtml = payload;
                }

                http.end();
                return true;
            }
        }

        http.end();
    }

    return false;
}

String Scanner::capturePortalPage(const String& url) {
    HTTPClient http;
    WiFiClient client;

    #if DEBUG_SERIAL && DEBUG_PORTAL
    Serial.printf("[PORTAL] Capturing portal page: %s\n", url.c_str());
    #endif

    http.begin(client, url);
    http.setTimeout(PORTAL_CHECK_TIMEOUT);

    int httpCode = http.GET();
    String html = "";

    if (httpCode == HTTP_CODE_OK) {
        html = http.getString();

        // Truncate if too large
        if (html.length() > MAX_PORTAL_CAPTURE_SIZE) {
            html = html.substring(0, MAX_PORTAL_CAPTURE_SIZE);
        }

        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.printf("[PORTAL] Captured %d bytes\n", html.length());
        #endif
    } else {
        #if DEBUG_SERIAL && DEBUG_PORTAL
        Serial.printf("[PORTAL] Capture failed with code: %d\n", httpCode);
        #endif
    }

    http.end();
    return html;
}

bool Scanner::connectToNetwork(int index) {
    if (index < 0 || index >= networks.size()) {
        return false;
    }

    disconnect();  // Ensure clean state

    NetworkInfo& net = networks[index];
    currentNetwork = index;

    #if DEBUG_SERIAL && DEBUG_WIFI
    Serial.printf("[WIFI] Connecting to %s...\n", net.ssid.c_str());
    #endif

    // For open networks
    if (net.isOpen) {
        WiFi.begin(net.ssid.c_str());
    } else {
        // Would need password - skip for now
        return false;
    }

    return true;
}

void Scanner::disconnect() {
    WiFi.disconnect();
    connected = false;
    currentNetwork = -1;
}

bool Scanner::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Getters
int Scanner::getNetworkCount() {
    return networks.size();
}

int Scanner::getPortalCount() {
    return portals.size();
}

NetworkInfo* Scanner::getNetwork(int index) {
    if (index >= 0 && index < networks.size()) {
        return &networks[index];
    }
    return nullptr;
}

NetworkInfo* Scanner::getPortal(int index) {
    if (index >= 0 && index < portals.size()) {
        return portals[index];
    }
    return nullptr;
}

std::vector<NetworkInfo>& Scanner::getNetworks() {
    return networks;
}

std::vector<NetworkInfo*>& Scanner::getPortals() {
    return portals;
}
