#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebServer {
public:
    static void init();
    static void stop();
    static bool isRunning();
    static String getIP();
    static String getAPSSID();

private:
    static AsyncWebServer server;
    static bool running;
    static String apSSID;
    static String apIP;

    // Route handlers
    static void handleRoot(AsyncWebServerRequest* request);
    static void handleStatus(AsyncWebServerRequest* request);
    static void handleScan(AsyncWebServerRequest* request);
    static void handleAnalyze(AsyncWebServerRequest* request);
    static void handleEnumerate(AsyncWebServerRequest* request);
    static void handleEnumProgress(AsyncWebServerRequest* request);
    static void handleLLM(AsyncWebServerRequest* request);
    static void handleNetworks(AsyncWebServerRequest* request);
    static void handleNotFound(AsyncWebServerRequest* request);

    // Utility
    static String getContentType(const String& filename);
    static void setupAP();
};

#endif // WEB_SERVER_H
