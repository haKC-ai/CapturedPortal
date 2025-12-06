#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Screen types
enum Screen {
    SCREEN_BOOT,
    SCREEN_SCANNER,
    SCREEN_PORTALS,
    SCREEN_ANALYSIS,
    SCREEN_LOG,
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
    STATE_SCANNING,
    STATE_VIEWING_PORTALS,
    STATE_ANALYZING,
    STATE_SETTINGS
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

private:
    static TFT_eSPI tft;
    static Screen currentScreen;
    static int selectedIndex;
    static int scrollOffset;
    static ColorPalette colors;

    static void drawScannerScreen();
    static void drawPortalsScreen();
    static void drawAnalysisScreen();
    static void drawLogScreen();
    static void drawSettingsScreen();

    static void initColors();
};

#endif // UI_H
