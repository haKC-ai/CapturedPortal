#ifndef SCANNER_H
#define SCANNER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

// Network info structure
struct NetworkInfo {
    String ssid;
    String bssid;
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encryption;
    bool isOpen;
    bool hasPortal;
    bool analyzed;
    String portalUrl;
    String portalHtml;
    unsigned long lastSeen;
};

// Portal analysis result
struct PortalAnalysis {
    String venueName;
    String venueType;
    String location;
    int roomCount;
    String networkProvider;
    std::vector<String> formFields;
    std::vector<String> insights;
    unsigned long timestamp;
};

class Scanner {
public:
    static void init();
    static void scan();
    static bool checkForPortal(int networkIndex);
    static String capturePortalPage(const String& url);

    // Getters
    static int getNetworkCount();
    static int getPortalCount();
    static NetworkInfo* getNetwork(int index);
    static NetworkInfo* getPortal(int index);
    static std::vector<NetworkInfo>& getNetworks();
    static std::vector<NetworkInfo*>& getPortals();

    // Connection management
    static bool connectToNetwork(int index);
    static void disconnect();
    static bool isConnected();

private:
    static std::vector<NetworkInfo> networks;
    static std::vector<NetworkInfo*> portals;
    static int currentNetwork;
    static bool connected;

    static bool detectCaptivePortal();
    static String getRedirectUrl(const String& response);
};

#endif // SCANNER_H
