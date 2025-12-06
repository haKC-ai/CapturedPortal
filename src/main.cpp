#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "core/scanner.h"
#include "core/power.h"
#include "core/enumerator.h"
#include "display/ui.h"
#include "display/effects.h"
#include "web/server.h"
#include "llm/engine.h"

// Forward declarations
void setupDisplay();
void setupPower();
void setupButtons();
void handleButtons();

// Global state
PowerMode currentPowerMode = POWER_BATTERY;
AppState appState = STATE_BOOT;

void setup() {
    // Initialize serial for debugging
    #if DEBUG_SERIAL
    Serial.begin(DEBUG_BAUD);
    delay(100);
    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║       CAPTURED PORTAL v" VERSION "         ║");
    Serial.println("║     Captive Portal Hunter/Analyzer    ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.println();
    #endif

    // Initialize power management (detect USB vs battery)
    setupPower();

    // Initialize display with hacker boot sequence
    setupDisplay();

    // Show boot animation
    Effects::bootSequence();

    // Initialize buttons
    setupButtons();

    // Initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Initialize scanner
    Scanner::init();

    // Initialize enumerator with wordlists
    Enumerator::init();

    // Start web server and LLM based on power mode and settings
    bool startWebServer = false;
    bool startLLM = false;

    if (currentPowerMode == POWER_USB) {
        #if DEBUG_SERIAL
        Serial.println("[POWER] USB detected - enabling boosted mode");
        #endif
        startWebServer = WEB_SERVER_ENABLED;
        startLLM = LLM_ENABLED;
    } else {
        #if DEBUG_SERIAL
        Serial.println("[POWER] Battery mode");
        #endif
        #if WEB_SERVER_ON_BATTERY
        startWebServer = WEB_SERVER_ENABLED;
        Serial.println("[POWER] Web server enabled on battery (config override)");
        #else
        Serial.println("[POWER] Web server disabled to conserve power");
        #endif
        #if LLM_ON_BATTERY
        startLLM = LLM_ENABLED;
        Serial.println("[POWER] LLM enabled on battery (config override)");
        #else
        startLLM = false;
        Serial.println("[POWER] LLM disabled to conserve power");
        #endif
    }

    // Start web server if enabled
    if (startWebServer) {
        #if WEB_SERVER_ENABLED
        Serial.println("[WEB] Starting web server...");
        WebServer::init();
        Serial.printf("[WEB] Connect to: %s\n", WebServer::getAPSSID().c_str());
        Serial.printf("[WEB] Dashboard: http://%s\n", WebServer::getIP().c_str());
        #endif
    }

    // Initialize LLM engine if enabled
    if (startLLM) {
        #if LLM_ENABLED
        Serial.println("[LLM] Initializing LLM engine...");
        LLMEngine::init();
        #endif
    }

    #ifdef LILYGO_T_DECK
    // T-Deck specific initialization
    Serial.println("[T-DECK] Initializing peripherals...");
    pinMode(PERI_POWERON, OUTPUT);
    digitalWrite(PERI_POWERON, HIGH);  // Enable peripheral power

    // Initialize trackball pins
    pinMode(TRACKBALL_UP_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_DOWN_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_LEFT_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_RIGHT_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_CLICK_PIN, INPUT_PULLUP);

    Serial.println("[T-DECK] Keyboard and trackball ready");
    #endif

    // Transition to scanner screen
    appState = STATE_SCANNING;
    UI::showScreen(SCREEN_SCANNER);

    #if DEBUG_SERIAL
    Serial.println("[READY] System initialized. Starting scan...");
    Serial.println();
    #endif
}

void loop() {
    static unsigned long lastScan = 0;
    static unsigned long lastUIUpdate = 0;

    // Determine scan interval based on power mode
    unsigned long scanInterval = (currentPowerMode == POWER_USB)
        ? USB_SCAN_INTERVAL
        : BATTERY_SCAN_INTERVAL;

    // Handle button input
    handleButtons();

    // Periodic WiFi scan
    if (millis() - lastScan > scanInterval) {
        lastScan = millis();

        #if DEBUG_SERIAL && DEBUG_WIFI
        Serial.println("[SCAN] Starting network scan...");
        #endif

        Scanner::scan();

        // Check detected networks for captive portals
        int portalCount = Scanner::getPortalCount();
        if (portalCount > 0) {
            #if DEBUG_SERIAL
            Serial.printf("[PORTAL] Found %d captive portal(s)\n", portalCount);
            #endif
        }
    }

    // Update display at 20fps
    if (millis() - lastUIUpdate > ANIMATION_FRAME_DELAY) {
        lastUIUpdate = millis();
        UI::update();
        Effects::update();
    }

    // Power management - check for idle
    Power::checkIdle();

    // Small delay to prevent watchdog issues
    delay(1);
}

void setupPower() {
    currentPowerMode = Power::detectMode();
    Power::init(currentPowerMode);

    // Set display brightness based on power mode
    uint8_t brightness = (currentPowerMode == POWER_USB)
        ? BRIGHTNESS_USB
        : BRIGHTNESS_BATTERY;
    UI::setBrightness(brightness);
}

void setupDisplay() {
    UI::init();
    Effects::init();
}

void setupButtons() {
    #ifdef BTN_LEFT
    if (BTN_LEFT >= 0) {
        pinMode(BTN_LEFT, INPUT_PULLUP);
    }
    #endif

    #ifdef BTN_RIGHT
    if (BTN_RIGHT >= 0) {
        pinMode(BTN_RIGHT, INPUT_PULLUP);
    }
    #endif
}

void handleButtons() {
    static unsigned long leftPressTime = 0;
    static unsigned long rightPressTime = 0;
    static bool leftWasPressed = false;
    static bool rightWasPressed = false;

    #ifdef LILYGO_T_DECK
    // T-Deck trackball handling
    static unsigned long lastTrackballCheck = 0;
    static bool clickWasPressed = false;
    static unsigned long clickPressTime = 0;

    if (millis() - lastTrackballCheck > 50) {  // Debounce
        lastTrackballCheck = millis();

        // Trackball directions
        if (digitalRead(TRACKBALL_UP_PIN) == LOW) {
            UI::navigate(NAV_UP);
            Power::resetIdleTimer();
        }
        if (digitalRead(TRACKBALL_DOWN_PIN) == LOW) {
            UI::navigate(NAV_DOWN);
            Power::resetIdleTimer();
        }
        if (digitalRead(TRACKBALL_LEFT_PIN) == LOW) {
            UI::navigate(NAV_LEFT);
            Power::resetIdleTimer();
        }
        if (digitalRead(TRACKBALL_RIGHT_PIN) == LOW) {
            UI::navigate(NAV_RIGHT);
            Power::resetIdleTimer();
        }

        // Trackball click (center button)
        bool clickPressed = digitalRead(TRACKBALL_CLICK_PIN) == LOW;
        if (clickPressed && !clickWasPressed) {
            clickPressTime = millis();
        } else if (!clickPressed && clickWasPressed) {
            unsigned long pressDuration = millis() - clickPressTime;
            if (pressDuration > 1000) {
                // Long press - start scan
                Scanner::scan();
            } else {
                // Short press - select
                UI::navigate(NAV_SELECT);
            }
            Power::resetIdleTimer();
        }
        clickWasPressed = clickPressed;
    }
    #endif

    #ifdef BTN_LEFT
    if (BTN_LEFT >= 0) {
        bool leftPressed = digitalRead(BTN_LEFT) == LOW;

        if (leftPressed && !leftWasPressed) {
            leftPressTime = millis();
        } else if (!leftPressed && leftWasPressed) {
            unsigned long pressDuration = millis() - leftPressTime;
            if (pressDuration > 1000) {
                // Long press - start scan
                Scanner::scan();
            } else {
                // Short press - scroll up / previous
                UI::navigate(NAV_UP);
            }
        }
        leftWasPressed = leftPressed;
    }
    #endif

    #ifdef BTN_RIGHT
    if (BTN_RIGHT >= 0) {
        bool rightPressed = digitalRead(BTN_RIGHT) == LOW;

        if (rightPressed && !rightWasPressed) {
            rightPressTime = millis();
        } else if (!rightPressed && rightWasPressed) {
            unsigned long pressDuration = millis() - rightPressTime;
            if (pressDuration > 1000) {
                // Long press - select / connect
                UI::navigate(NAV_SELECT);
            } else {
                // Short press - scroll down / next
                UI::navigate(NAV_DOWN);
            }
        }
        rightWasPressed = rightPressed;
    }
    #endif
}
