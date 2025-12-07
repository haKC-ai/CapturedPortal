#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "core/scanner.h"
#include "core/power.h"
#include "core/enumerator.h"
#include "display/ui.h"
#include "display/effects.h"
#include "web/server.h"
#include "llm/engine.h"

// ANSI color codes for serial output
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_BG_BLACK "\033[40m"

// Forward declarations
void setupDisplay();
void setupPower();
void setupButtons();
void handleButtons();
#ifdef LILYGO_T_DECK
void setupKeyboard();
void handleKeyboard();
#endif

// Global state
PowerMode currentPowerMode = POWER_BATTERY;
AppState appState = STATE_BOOT;

void setup() {
    // T-Deck: Enable peripheral power FIRST (before display, keyboard, etc.)
    #ifdef LILYGO_T_DECK
    pinMode(PERI_POWERON, OUTPUT);
    digitalWrite(PERI_POWERON, HIGH);
    delay(100);  // Give peripherals time to power up
    #endif

    // Initialize serial for debugging
    #if DEBUG_SERIAL
    Serial.begin(DEBUG_BAUD);
    delay(100);
    Serial.println("\n");
    Serial.println(ANSI_CYAN "╔═══════════════════════════════════════╗" ANSI_RESET);
    Serial.println(ANSI_CYAN "║" ANSI_GREEN ANSI_BOLD "       CAPTURED PORTAL v" VERSION "         " ANSI_RESET ANSI_CYAN "║" ANSI_RESET);
    Serial.println(ANSI_CYAN "║" ANSI_WHITE "     Captive Portal Hunter/Analyzer    " ANSI_CYAN "║" ANSI_RESET);
    Serial.println(ANSI_CYAN "╚═══════════════════════════════════════╝" ANSI_RESET);
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

    // Initialize WiFi - start AP FIRST before scanning
    WiFi.disconnect();
    delay(100);

    // Determine what features to enable based on power mode
    bool startWebServer = false;
    bool startLLM = false;

    if (currentPowerMode == POWER_USB) {
        #if DEBUG_SERIAL
        Serial.println(ANSI_GREEN "[POWER]" ANSI_RESET " USB detected - " ANSI_BOLD "boosted mode" ANSI_RESET);
        #endif
        startWebServer = WEB_SERVER_ENABLED;
        startLLM = LLM_ENABLED;
    } else {
        #if DEBUG_SERIAL
        Serial.println(ANSI_YELLOW "[POWER]" ANSI_RESET " Battery mode");
        #endif
        #if WEB_SERVER_ON_BATTERY
        startWebServer = WEB_SERVER_ENABLED;
        Serial.println(ANSI_YELLOW "[POWER]" ANSI_RESET " Web server enabled on battery");
        #else
        Serial.println(ANSI_DIM "[POWER] Web server disabled to conserve power" ANSI_RESET);
        #endif
        #if LLM_ON_BATTERY
        startLLM = LLM_ENABLED;
        Serial.println(ANSI_YELLOW "[POWER]" ANSI_RESET " LLM enabled on battery");
        #else
        startLLM = false;
        Serial.println(ANSI_DIM "[POWER] LLM disabled to conserve power" ANSI_RESET);
        #endif
    }

    // Start web server/AP FIRST (before scanner init)
    // The AP must be up before scanning starts
    if (startWebServer) {
        #if WEB_SERVER_ENABLED
        Serial.println(ANSI_CYAN "[WEB]" ANSI_RESET " Starting Access Point and web server...");
        WebServer::init();
        Serial.println();
        Serial.println(ANSI_GREEN "╔═══════════════════════════════════════╗" ANSI_RESET);
        Serial.printf(ANSI_GREEN "║" ANSI_CYAN "  WiFi AP: " ANSI_WHITE ANSI_BOLD "%-24s" ANSI_RESET ANSI_GREEN "   ║" ANSI_RESET "\n", WebServer::getAPSSID().c_str());
        Serial.printf(ANSI_GREEN "║" ANSI_CYAN "  Password: " ANSI_WHITE "%-23s" ANSI_RESET ANSI_GREEN "   ║" ANSI_RESET "\n", strlen(AP_PASSWORD) > 0 ? AP_PASSWORD : "(open)");
        Serial.printf(ANSI_GREEN "║" ANSI_CYAN "  Dashboard: " ANSI_MAGENTA "http://%-15s" ANSI_RESET ANSI_GREEN "   ║" ANSI_RESET "\n", WebServer::getIP().c_str());
        Serial.println(ANSI_GREEN "╚═══════════════════════════════════════╝" ANSI_RESET);
        Serial.println();
        #endif
    } else {
        // No web server, just set to station mode for scanning
        WiFi.mode(WIFI_STA);
        Serial.println(ANSI_DIM "[WIFI] Station mode only (no AP)" ANSI_RESET);
    }

    // Now initialize scanner (after AP is up)
    Scanner::init();

    // Initialize enumerator with wordlists
    Enumerator::init();

    // Initialize LLM engine if enabled
    if (startLLM) {
        #if LLM_ENABLED
        Serial.println(ANSI_MAGENTA "[LLM]" ANSI_RESET " Initializing LLM engine...");
        LLMEngine::init();
        #endif
    }

    #ifdef LILYGO_T_DECK
    // T-Deck trackball initialization (power already enabled at start)
    Serial.println(ANSI_BLUE "[T-DECK]" ANSI_RESET " Initializing trackball...");
    pinMode(TRACKBALL_UP_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_DOWN_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_LEFT_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_RIGHT_PIN, INPUT_PULLUP);
    pinMode(TRACKBALL_CLICK_PIN, INPUT_PULLUP);
    Serial.println(ANSI_BLUE "[T-DECK]" ANSI_GREEN " Trackball ready" ANSI_RESET);

    // T-Deck keyboard initialization
    setupKeyboard();
    #endif

    // Show boot banner then transition to main menu
    appState = STATE_IDLE;
    UI::showScreen(SCREEN_BOOT);  // Shows banner, then auto-transitions to MAIN

    #if DEBUG_SERIAL
    Serial.println(ANSI_GREEN ANSI_BOLD "[READY]" ANSI_RESET " System initialized. Starting scan...");
    Serial.println();
    #endif
}

void loop() {
    static unsigned long lastScan = 0;
    static unsigned long lastUIUpdate = 0;
    static unsigned long lastAPStatus = 0;

    // Periodically print AP status (every 30 seconds) so user can see connection info
    #if WEB_SERVER_ENABLED
    if (millis() - lastAPStatus > 30000) {
        lastAPStatus = millis();
        if (WebServer::isRunning()) {
            int clients = WiFi.softAPgetStationNum();
            Serial.println();
            Serial.println(ANSI_CYAN "┌─────────────────────────────────────┐" ANSI_RESET);
            Serial.printf(ANSI_CYAN "│" ANSI_WHITE "  WiFi AP: " ANSI_BOLD "%-24s" ANSI_RESET ANSI_CYAN " │" ANSI_RESET "\n", WebServer::getAPSSID().c_str());
            Serial.printf(ANSI_CYAN "│" ANSI_WHITE "  Password: %-24s" ANSI_RESET ANSI_CYAN " │" ANSI_RESET "\n", strlen(AP_PASSWORD) > 0 ? AP_PASSWORD : "(open)");
            Serial.printf(ANSI_CYAN "│" ANSI_WHITE "  Dashboard: " ANSI_MAGENTA "http://%-14s" ANSI_RESET ANSI_CYAN " │" ANSI_RESET "\n", WebServer::getIP().c_str());
            Serial.printf(ANSI_CYAN "│" ANSI_WHITE "  Clients: " ANSI_GREEN "%-25d" ANSI_RESET ANSI_CYAN " │" ANSI_RESET "\n", clients);
            Serial.println(ANSI_CYAN "└─────────────────────────────────────┘" ANSI_RESET);
        }
    }
    #endif

    // Determine scan interval based on power mode
    unsigned long scanInterval = (currentPowerMode == POWER_USB)
        ? USB_SCAN_INTERVAL
        : BATTERY_SCAN_INTERVAL;

    // Handle button/trackball input
    handleButtons();

    // Handle keyboard input (T-Deck)
    #ifdef LILYGO_T_DECK
    handleKeyboard();
    #endif

    // Periodic WiFi scan
    if (millis() - lastScan > scanInterval) {
        lastScan = millis();

        #if DEBUG_SERIAL && DEBUG_WIFI
        Serial.println(ANSI_BLUE "[SCAN]" ANSI_RESET " Starting network scan...");
        #endif

        Scanner::scan();

        // Check detected networks for captive portals
        int portalCount = Scanner::getPortalCount();
        if (portalCount > 0) {
            #if DEBUG_SERIAL
            Serial.printf(ANSI_RED ANSI_BOLD "[PORTAL]" ANSI_RESET " Found " ANSI_YELLOW "%d" ANSI_RESET " captive portal(s)\n", portalCount);
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
    // T-Deck trackball handling with edge detection (not level)
    // Only trigger on transition from HIGH to LOW
    static bool upWas = true, downWas = true, leftWas = true, rightWas = true;
    static unsigned long lastInput = 0;
    static bool clickWasPressed = false;
    static unsigned long clickPressTime = 0;
    const unsigned long debounce = 200;  // 200ms minimum between any input

    unsigned long now = millis();

    // Read all pins
    bool upNow = digitalRead(TRACKBALL_UP_PIN) == LOW;
    bool downNow = digitalRead(TRACKBALL_DOWN_PIN) == LOW;
    bool leftNow = digitalRead(TRACKBALL_LEFT_PIN) == LOW;
    bool rightNow = digitalRead(TRACKBALL_RIGHT_PIN) == LOW;

    // Only process if enough time has passed since last input
    if (now - lastInput > debounce) {
        // Trackball UP - edge detection (was HIGH, now LOW)
        if (upNow && !upWas) {
            lastInput = now;
            #if DEBUG_SERIAL
            Serial.println(ANSI_DIM "[INPUT] Trackball UP" ANSI_RESET);
            #endif
            UI::navigate(NAV_UP);
            Power::resetIdleTimer();
        }

        // Trackball DOWN
        if (downNow && !downWas) {
            lastInput = now;
            #if DEBUG_SERIAL
            Serial.println(ANSI_DIM "[INPUT] Trackball DOWN" ANSI_RESET);
            #endif
            UI::navigate(NAV_DOWN);
            Power::resetIdleTimer();
        }

        // Trackball LEFT
        if (leftNow && !leftWas) {
            lastInput = now;
            #if DEBUG_SERIAL
            Serial.println(ANSI_DIM "[INPUT] Trackball LEFT" ANSI_RESET);
            #endif
            UI::navigate(NAV_LEFT);
            Power::resetIdleTimer();
        }

        // Trackball RIGHT
        if (rightNow && !rightWas) {
            lastInput = now;
            #if DEBUG_SERIAL
            Serial.println(ANSI_DIM "[INPUT] Trackball RIGHT" ANSI_RESET);
            #endif
            UI::navigate(NAV_RIGHT);
            Power::resetIdleTimer();
        }
    }

    // Save current state for edge detection
    upWas = upNow;
    downWas = downNow;
    leftWas = leftNow;
    rightWas = rightNow;

    // Trackball click (center button) - edge detection
    bool clickPressed = digitalRead(TRACKBALL_CLICK_PIN) == LOW;
    if (clickPressed && !clickWasPressed) {
        clickPressTime = millis();
    } else if (!clickPressed && clickWasPressed) {
        unsigned long pressDuration = millis() - clickPressTime;
        if (pressDuration > 1000) {
            // Long press - start scan
            #if DEBUG_SERIAL
            Serial.println(ANSI_YELLOW "[INPUT] Long press - SCAN" ANSI_RESET);
            #endif
            Scanner::scan();
        } else if (pressDuration > 50) {  // Filter noise
            // Short press - select
            #if DEBUG_SERIAL
            Serial.println(ANSI_GREEN "[INPUT] Click - SELECT" ANSI_RESET);
            #endif
            UI::navigate(NAV_SELECT);
        }
        Power::resetIdleTimer();
    }
    clickWasPressed = clickPressed;
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

// ============================================================
// T-DECK KEYBOARD HANDLING
// ============================================================

#ifdef LILYGO_T_DECK

// Keyboard state
static bool keyboardReady = false;

void setupKeyboard() {
    Serial.println(ANSI_BLUE "[T-DECK]" ANSI_RESET " Initializing keyboard...");

    // Initialize I2C for keyboard (uses same bus as other peripherals)
    // T-Deck keyboard is at I2C address 0x55
    Wire.begin(43, 44);  // SDA=43, SCL=44 for T-Deck
    Wire.setClock(400000);

    // Check if keyboard responds
    Wire.beginTransmission(KEYBOARD_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        keyboardReady = true;
        Serial.println(ANSI_BLUE "[T-DECK]" ANSI_GREEN " Keyboard ready at 0x55" ANSI_RESET);
        Serial.println(ANSI_DIM "  Keys: WASD/Arrows=Navigate, Enter=Select, Q/Esc=Back" ANSI_RESET);
        Serial.println(ANSI_DIM "  Keys: S=Scan, P=Portals, E=Enum, H=Help" ANSI_RESET);
    } else {
        Serial.println(ANSI_YELLOW "[T-DECK]" ANSI_RED " Keyboard not found!" ANSI_RESET);
    }
}

void handleKeyboard() {
    if (!keyboardReady) return;

    // Read from keyboard
    Wire.requestFrom(KEYBOARD_I2C_ADDR, 1);
    if (Wire.available()) {
        char key = Wire.read();
        if (key == 0) return;  // No key pressed

        Power::resetIdleTimer();

        #if DEBUG_SERIAL
        Serial.printf(ANSI_CYAN "[KEY]" ANSI_RESET " '%c' (0x%02X)\n", key >= 32 ? key : '?', key);
        #endif

        // Navigation keys
        switch (key) {
            // Arrow keys (T-Deck sends these as special codes)
            case 0x11:  // Up arrow
            case 'w':
            case 'W':
                UI::navigate(NAV_UP);
                break;

            case 0x12:  // Down arrow
            case 's':
            case 'S':
                UI::navigate(NAV_DOWN);
                break;

            case 0x13:  // Left arrow
            case 'a':
            case 'A':
                UI::navigate(NAV_LEFT);
                break;

            case 0x14:  // Right arrow
            case 'd':
            case 'D':
                UI::navigate(NAV_RIGHT);
                break;

            // Selection
            case '\r':   // Enter
            case '\n':
            case ' ':    // Space
                UI::navigate(NAV_SELECT);
                break;

            // Back/Escape
            case 0x1B:   // ESC
            case 'q':
            case 'Q':
            case 0x08:   // Backspace
                UI::navigate(NAV_BACK);
                break;

            // Quick actions
            case '1':
                UI::showScreen(SCREEN_SCANNER);
                break;
            case '2':
                UI::showScreen(SCREEN_PORTALS);
                break;
            case '3':
                UI::showScreen(SCREEN_ENUM);
                break;
            case '4':
                UI::showScreen(SCREEN_SETTINGS);
                break;

            case 'r':
            case 'R':
                // Force refresh/rescan
                Serial.println(ANSI_YELLOW "[KEY]" ANSI_RESET " Forcing network scan...");
                Scanner::scan();
                break;

            case 'h':
            case 'H':
            case '?':
                // Print help to serial
                Serial.println();
                Serial.println(ANSI_CYAN "╔═══════════════════════════════════════╗" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_WHITE ANSI_BOLD "         KEYBOARD SHORTCUTS            " ANSI_RESET ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "╠═══════════════════════════════════════╣" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " W/↑" ANSI_WHITE "  - Navigate Up                  " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " S/↓" ANSI_WHITE "  - Navigate Down                " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " A/←" ANSI_WHITE "  - Navigate Left / Back         " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " D/→" ANSI_WHITE "  - Navigate Right               " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " Enter" ANSI_WHITE " - Select                       " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_GREEN " Q/Esc" ANSI_WHITE " - Back                         " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "╠═══════════════════════════════════════╣" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " 1" ANSI_WHITE "     - Scanner Screen              " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " 2" ANSI_WHITE "     - Portals Screen              " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " 3" ANSI_WHITE "     - Enumeration Screen          " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " 4" ANSI_WHITE "     - Settings Screen             " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " R" ANSI_WHITE "     - Force Rescan                " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "║" ANSI_YELLOW " H/?" ANSI_WHITE "   - Show this help              " ANSI_CYAN "║" ANSI_RESET);
                Serial.println(ANSI_CYAN "╚═══════════════════════════════════════╝" ANSI_RESET);
                break;

            default:
                // Unknown key - could be used for command input later
                break;
        }
    }
}

#endif // LILYGO_T_DECK
