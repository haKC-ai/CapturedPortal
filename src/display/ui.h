#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Screen types (matches web interface tabs)
enum Screen {
    SCREEN_BOOT,
    SCREEN_MAIN,       // Main dashboard with tabs
    SCREEN_SCANNER,    // Network scanner tab
    SCREEN_PORTALS,    // Portal analysis tab
    SCREEN_ENUM,       // Enumeration tab
    SCREEN_LLM,        // LLM insights tab
    SCREEN_NETWORK_DETAIL,  // Detail view for selected network
    SCREEN_SETTINGS
};

// Navigation actions
enum NavAction {
    NAV_UP,
    NAV_DOWN,
    NAV_LEFT,
    NAV_RIGHT,
    NAV_SELECT,
    NAV_BACK
};

// App states
enum AppState {
    STATE_BOOT,
    STATE_IDLE,
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_ANALYZING,
    STATE_ENUMERATING
};

// Tab indices (for horizontal tab navigation)
enum TabIndex {
    TAB_SCANNER = 0,
    TAB_PORTALS = 1,
    TAB_ENUM = 2,
    TAB_LLM = 3,
    TAB_COUNT = 4
};

// Color palette structure
struct ColorPalette {
    uint16_t background;
    uint16_t primary;
    uint16_t secondary;
    uint16_t accent;
    uint16_t text;
    uint16_t textDim;
    uint16_t success;
    uint16_t warning;
    uint16_t error;
};

class UI {
public:
    static void init();
    static void update();
    static void showScreen(Screen screen);
    static void navigate(NavAction action);
    static void setBrightness(uint8_t brightness);

    // Drawing utilities
    static void clear();
    static void drawHeader(const char* title);
    static void drawFooter(const char* left, const char* right);
    static void drawProgressBar(int x, int y, int w, int h, int percent);
    static void drawSignalStrength(int x, int y, int rssi);
    static void drawBattery(int x, int y, int percent);

    // Text utilities
    static void print(const char* text, int x, int y, uint16_t color = 0);
    static void printCentered(const char* text, int y, uint16_t color = 0);
    static void printTyped(const char* text, int x, int y, int delayMs = 30);

    // Get display reference
    static TFT_eSPI& getDisplay();
    static ColorPalette& getColors();
    static int getWidth();
    static int getHeight();

    // Tab navigation
    static void setTab(TabIndex tab);
    static TabIndex getCurrentTab();

    // Draw individual UI components
    static void drawCard(int x, int y, int w, int h, const char* title);
    static void drawStatsBar(int networks, int open, int portals);
    static void drawTabBar();
    static void drawNetworkItem(int index, int y, bool selected);
    static void drawStatusIcon(int x, int y, bool connected);

    // Modern button-based UI
    static void drawRoundButton(int x, int y, int radius, const char* label, bool selected, uint16_t color);
    static void drawSquareButton(int x, int y, int w, int h, const char* label, bool selected, uint16_t color);
    static void drawBootBanner();
    static void drawMainMenu();

private:
    static TFT_eSPI tft;
    static Screen currentScreen;
    static TabIndex currentTab;
    static int selectedIndex;
    static int scrollOffset;
    static ColorPalette colors;
    static bool inListMode;  // true = navigating list, false = navigating tabs

    // Screen drawing functions
    static void drawMainScreen();
    static void drawScannerScreen();
    static void drawPortalsScreen();
    static void drawEnumScreen();
    static void drawLLMScreen();
    static void drawNetworkDetailScreen();
    static void drawSettingsScreen();

    static void initColors();
};

#endif // UI_H
