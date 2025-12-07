#include "ui.h"
#include "config.h"
#include "core/scanner.h"
#include "core/power.h"
#include <SPI.h>

// Static member initialization
TFT_eSPI UI::tft = TFT_eSPI();
Screen UI::currentScreen = SCREEN_BOOT;
TabIndex UI::currentTab = TAB_SCANNER;
int UI::selectedIndex = 0;
int UI::scrollOffset = 0;
ColorPalette UI::colors;
bool UI::inListMode = false;

// Button/menu state
static int selectedButton = 0;
static int prevSelectedButton = -1;  // Track previous selection for incremental updates
static unsigned long lastInputTime = 0;
static bool needsFullRedraw = true;
static bool contentNeedsUpdate = true;  // For dynamic content like network lists
static int prevNetworkCount = -1;       // Track network count changes
static int prevSelectedIndex = -1;      // Track selection changes

// Forward declarations for static helper functions
static void drawMenuButton(int x, int y, int w, int h, const char* label, bool sel, uint16_t color, int iconType);
static void drawNetworkListItem(int x, int y, int w, int h, NetworkInfo& net, bool sel);

void UI::initColors() {
    switch (COLOR_SCHEME) {
        case COLOR_MATRIX:
            colors.background = TFT_BLACK;
            colors.primary = tft.color565(0, 255, 65);      // Matrix green
            colors.secondary = tft.color565(0, 180, 45);    // Darker green
            colors.accent = tft.color565(150, 255, 150);    // Light green
            colors.text = tft.color565(0, 255, 65);
            colors.textDim = tft.color565(0, 100, 30);
            colors.success = tft.color565(0, 255, 65);
            colors.warning = tft.color565(255, 255, 0);
            colors.error = tft.color565(255, 50, 50);
            break;

        case COLOR_SYNTHWAVE:
            colors.background = tft.color565(13, 2, 33);    // Deep purple/black
            colors.primary = tft.color565(0, 255, 255);     // Cyan
            colors.secondary = tft.color565(255, 0, 255);   // Magenta
            colors.accent = tft.color565(255, 100, 255);    // Pink
            colors.text = tft.color565(0, 255, 255);
            colors.textDim = tft.color565(80, 80, 120);
            colors.success = tft.color565(0, 255, 200);
            colors.warning = tft.color565(255, 200, 0);
            colors.error = tft.color565(255, 50, 100);
            break;

        case COLOR_CYBERPUNK:
            colors.background = tft.color565(10, 10, 20);   // Near black
            colors.primary = tft.color565(255, 50, 150);    // Hot pink
            colors.secondary = tft.color565(255, 255, 0);   // Yellow
            colors.accent = tft.color565(0, 255, 255);      // Cyan
            colors.text = tft.color565(255, 255, 0);
            colors.textDim = tft.color565(150, 150, 50);
            colors.success = tft.color565(0, 255, 100);
            colors.warning = tft.color565(255, 150, 0);
            colors.error = tft.color565(255, 0, 50);
            break;

        case COLOR_DRACULA:
            colors.background = tft.color565(40, 42, 54);   // Background
            colors.primary = tft.color565(189, 147, 249);   // Purple
            colors.secondary = tft.color565(139, 233, 253); // Cyan
            colors.accent = tft.color565(255, 121, 198);    // Pink
            colors.text = tft.color565(248, 248, 242);      // Foreground
            colors.textDim = tft.color565(98, 114, 164);    // Comment
            colors.success = tft.color565(80, 250, 123);    // Green
            colors.warning = tft.color565(255, 184, 108);   // Orange
            colors.error = tft.color565(255, 85, 85);       // Red
            break;

        default:
            colors.background = TFT_BLACK;
            colors.primary = TFT_GREEN;
            colors.secondary = TFT_DARKGREEN;
            colors.accent = TFT_LIGHTGREY;
            colors.text = TFT_GREEN;
            colors.textDim = TFT_DARKGREY;
            colors.success = TFT_GREEN;
            colors.warning = TFT_YELLOW;
            colors.error = TFT_RED;
    }
}

void UI::init() {
    #if DEBUG_SERIAL
    Serial.println("[UI] Initializing display...");
    #endif

    #ifdef LILYGO_T_DECK
    Serial.println("[UI] T-Deck: Setting up SPI bus...");
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
    #endif

    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(TFT_BLACK);
    initColors();

    #ifdef TFT_BL
    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }
    #endif

    #if DEBUG_SERIAL
    Serial.printf("[UI] Display initialized: %dx%d\n", tft.width(), tft.height());
    #endif
}

// ============================================================
// BOOT SEQUENCE WITH BANNER
// ============================================================

void UI::drawBootBanner() {
    int w = tft.width();
    int h = tft.height();

    tft.fillScreen(colors.background);

    // Animated scanlines
    for (int y = 0; y < h; y += 4) {
        tft.drawFastHLine(0, y, w, colors.textDim);
    }

    // Draw stylized "CP" logo in center
    int centerX = w / 2;
    int centerY = h / 2 - 30;

    // Large circle outline
    tft.drawCircle(centerX, centerY, 45, colors.primary);
    tft.drawCircle(centerX, centerY, 44, colors.primary);
    tft.drawCircle(centerX, centerY, 43, colors.secondary);

    // Inner design - "CP" letters stylized
    tft.setTextSize(3);
    tft.setTextColor(colors.primary, colors.background);
    tft.setCursor(centerX - 24, centerY - 12);
    tft.print("CP");

    // Glowing effect (dimmed primary color)
    tft.drawCircle(centerX, centerY, 50, colors.textDim);
    tft.drawCircle(centerX, centerY, 55, colors.textDim);

    // Title below
    tft.setTextSize(1);
    tft.setTextColor(colors.text, colors.background);

    const char* title = "CAPTURED PORTAL";
    int titleW = strlen(title) * 6;
    tft.setCursor(centerX - titleW / 2, centerY + 55);
    tft.print(title);

    // Version
    tft.setTextColor(colors.textDim, colors.background);
    char ver[16];
    snprintf(ver, sizeof(ver), "v%s", VERSION);
    int verW = strlen(ver) * 6;
    tft.setCursor(centerX - verW / 2, centerY + 70);
    tft.print(ver);

    // Bottom tagline with typing effect
    tft.setTextColor(colors.secondary, colors.background);
    const char* tagline = "[ WiFi Portal Hunter ]";
    int tagW = strlen(tagline) * 6;
    int tagX = centerX - tagW / 2;
    int tagY = h - 30;

    for (int i = 0; tagline[i]; i++) {
        tft.setCursor(tagX + i * 6, tagY);
        tft.print(tagline[i]);
        delay(25);
    }

    delay(800);

    // Flash effect
    for (int i = 0; i < 3; i++) {
        tft.drawRect(0, 0, w, h, colors.primary);
        delay(50);
        tft.drawRect(0, 0, w, h, colors.background);
        delay(50);
    }
}

// ============================================================
// LARGE ROUND BUTTON DRAWING
// ============================================================

void UI::drawRoundButton(int x, int y, int radius, const char* label, bool selected, uint16_t color) {
    // Draw filled circle for button
    if (selected) {
        // Selected: filled with color, text inverted
        tft.fillCircle(x, y, radius, color);
        tft.drawCircle(x, y, radius + 2, colors.accent);
        tft.drawCircle(x, y, radius + 3, colors.accent);
        tft.setTextColor(colors.background, color);
    } else {
        // Unselected: outline only
        tft.fillCircle(x, y, radius, colors.background);
        tft.drawCircle(x, y, radius, color);
        tft.drawCircle(x, y, radius - 1, color);
        tft.setTextColor(color, colors.background);
    }

    // Center the label
    tft.setTextSize(1);
    int labelW = strlen(label) * 6;
    int labelH = 8;
    tft.setCursor(x - labelW / 2, y - labelH / 2);
    tft.print(label);
}

void UI::drawSquareButton(int x, int y, int w, int h, const char* label, bool selected, uint16_t color) {
    int radius = 8;  // Corner radius

    if (selected) {
        // Filled rounded rect
        tft.fillRoundRect(x, y, w, h, radius, color);
        tft.drawRoundRect(x - 2, y - 2, w + 4, h + 4, radius + 2, colors.accent);
        tft.setTextColor(colors.background, color);
    } else {
        tft.fillRoundRect(x, y, w, h, radius, colors.background);
        tft.drawRoundRect(x, y, w, h, radius, color);
        tft.setTextColor(color, colors.background);
    }

    // Center label
    tft.setTextSize(1);
    int labelW = strlen(label) * 6;
    tft.setCursor(x + (w - labelW) / 2, y + (h - 8) / 2);
    tft.print(label);
}

// Menu button with icon - combines button + icon drawing
static void drawMenuButton(int x, int y, int w, int h, const char* label, bool sel, uint16_t color, int iconType) {
    int radius = 8;

    // Clear selection highlight area first
    UI::getDisplay().fillRect(x - 4, y - 4, w + 8, h + 8, UI::getColors().background);

    if (sel) {
        UI::getDisplay().fillRoundRect(x, y, w, h, radius, color);
        UI::getDisplay().drawRoundRect(x - 2, y - 2, w + 4, h + 4, radius + 2, UI::getColors().accent);
        UI::getDisplay().setTextColor(UI::getColors().background, color);
    } else {
        UI::getDisplay().fillRoundRect(x, y, w, h, radius, UI::getColors().background);
        UI::getDisplay().drawRoundRect(x, y, w, h, radius, color);
        UI::getDisplay().setTextColor(color, UI::getColors().background);
    }

    // Center label
    UI::getDisplay().setTextSize(1);
    int labelW = strlen(label) * 6;
    UI::getDisplay().setCursor(x + (w - labelW) / 2, y + (h - 8) / 2);
    UI::getDisplay().print(label);

    // Icon on left side
    int iconX = x + 20;
    int iconY = y + h / 2;
    uint16_t iconColor = sel ? UI::getColors().background : color;

    switch (iconType) {
        case 0: // Scan - WiFi icon
            UI::getDisplay().drawCircle(iconX, iconY, 8, iconColor);
            UI::getDisplay().drawCircle(iconX, iconY, 5, iconColor);
            UI::getDisplay().fillCircle(iconX, iconY, 2, iconColor);
            break;
        case 1: // Portals - Star
            UI::getDisplay().setCursor(iconX - 4, iconY - 4);
            UI::getDisplay().print("*");
            break;
        case 2: // Enum - List icon
            for (int j = 0; j < 3; j++) {
                UI::getDisplay().drawFastHLine(iconX - 6, iconY - 4 + j * 4, 12, iconColor);
            }
            break;
        case 3: // Settings - Gear
            UI::getDisplay().drawCircle(iconX, iconY, 6, iconColor);
            UI::getDisplay().fillCircle(iconX, iconY, 3, iconColor);
            break;
    }
}

// ============================================================
// MAIN MENU WITH LARGE BUTTONS
// ============================================================

void UI::drawMainMenu() {
    int w = tft.width();
    int h = tft.height();

    // Main button area
    int btnY = 40;
    int btnW = w - 20;
    int btnH = 50;
    int btnSpacing = 8;

    // Menu items
    const char* menuItems[] = {"SCAN NETWORKS", "VIEW PORTALS", "ENUMERATION", "SETTINGS"};
    uint16_t menuColors[] = {colors.primary, colors.success, colors.warning, colors.textDim};
    int menuCount = 4;

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);

        // Header
        tft.setTextSize(1);
        tft.setTextColor(colors.primary, colors.background);
        tft.setCursor(4, 4);
        tft.print("[haKC.ai :: It's Turbo Time!!]");

        // Power/battery indicator
        PowerMode mode = Power::getMode();
        tft.setTextColor(mode == POWER_USB ? colors.success : colors.warning, colors.background);
        tft.setCursor(w - 24, 4);
        tft.print(mode == POWER_USB ? "USB" : "BAT");

        // Stats bar
        auto& networks = Scanner::getNetworks();
        int openCount = 0;
        for (auto& net : networks) if (net.isOpen) openCount++;
        int portalCount = Scanner::getPortalCount();

        tft.setTextColor(colors.text, colors.background);
        tft.setCursor(w / 2 - 60, 4);
        tft.printf("N:%d O:%d P:%d", (int)networks.size(), openCount, portalCount);

        tft.drawFastHLine(0, 16, w, colors.secondary);

        // Draw ALL buttons on full redraw
        for (int i = 0; i < menuCount; i++) {
            int y = btnY + i * (btnH + btnSpacing);
            bool sel = (selectedButton == i);
            drawMenuButton(10, y, btnW, btnH, menuItems[i], sel, menuColors[i], i);
        }

        // Footer hint
        tft.drawFastHLine(0, h - 18, w, colors.secondary);
        tft.setTextColor(colors.textDim, colors.background);
        tft.setCursor(4, h - 12);
        tft.print("[^v] Navigate  [OK] Select");

        prevSelectedButton = selectedButton;
        needsFullRedraw = false;
        return;
    }

    // Incremental update - only redraw buttons that changed selection state
    if (prevSelectedButton != selectedButton) {
        // Redraw old selected (now unselected)
        if (prevSelectedButton >= 0 && prevSelectedButton < menuCount) {
            int y = btnY + prevSelectedButton * (btnH + btnSpacing);
            drawMenuButton(10, y, btnW, btnH, menuItems[prevSelectedButton], false, menuColors[prevSelectedButton], prevSelectedButton);
        }
        // Redraw new selected
        if (selectedButton >= 0 && selectedButton < menuCount) {
            int y = btnY + selectedButton * (btnH + btnSpacing);
            drawMenuButton(10, y, btnW, btnH, menuItems[selectedButton], true, menuColors[selectedButton], selectedButton);
        }
        prevSelectedButton = selectedButton;
    }
}

// ============================================================
// NETWORK LIST SCREEN
// ============================================================

void UI::drawScannerScreen() {
    int w = tft.width();
    int h = tft.height();

    auto& networks = Scanner::getNetworks();
    int currentNetCount = networks.size();
    bool networkCountChanged = (currentNetCount != prevNetworkCount);
    bool selectionChanged = (selectedIndex != prevSelectedIndex);

    // Network list params
    int listY = 44;
    int itemH = 32;
    int maxVisible = (h - listY - 20) / itemH;

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);

        // Header
        tft.setTextSize(1);
        tft.setTextColor(colors.primary, colors.background);
        tft.setCursor(4, 4);
        tft.print("< SCAN NETWORKS");
        tft.drawFastHLine(0, 16, w, colors.secondary);

        // Footer
        tft.drawFastHLine(0, h - 18, w, colors.secondary);
        tft.setTextColor(colors.textDim, colors.background);
        tft.setCursor(4, h - 12);
        tft.print("[<] Back  [OK] Check Portal");

        contentNeedsUpdate = true;
        prevSelectedIndex = -1;  // Force redraw all items
        needsFullRedraw = false;
    }

    // Update stats only when network count changes
    if (networkCountChanged || contentNeedsUpdate) {
        int openCount = 0;
        for (auto& net : networks) if (net.isOpen) openCount++;

        tft.fillRect(0, 20, w, 20, colors.background);
        tft.setTextColor(colors.textDim, colors.background);
        tft.setCursor(4, 24);
        tft.printf("Found: %d networks (%d open)", currentNetCount, openCount);
        prevNetworkCount = currentNetCount;
    }

    // Scroll management
    if (selectedIndex >= scrollOffset + maxVisible) {
        scrollOffset = selectedIndex - maxVisible + 1;
        contentNeedsUpdate = true;  // Need to redraw all items when scrolling
    }
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
        contentNeedsUpdate = true;
    }

    // Only redraw items if content changed or selection changed
    if (contentNeedsUpdate || selectionChanged || networkCountChanged) {
        // If scrolled or content changed, redraw all visible items
        if (contentNeedsUpdate || networkCountChanged) {
            for (int i = 0; i < maxVisible; i++) {
                int y = listY + i * itemH;
                if ((scrollOffset + i) < currentNetCount) {
                    int idx = scrollOffset + i;
                    drawNetworkListItem(2, y, w - 4, itemH - 2, networks[idx], idx == selectedIndex);
                } else {
                    // Clear empty slot
                    tft.fillRect(2, y, w - 4, itemH - 2, colors.background);
                }
            }
        } else if (selectionChanged) {
            // Only redraw affected items (old and new selection)
            for (int i = 0; i < maxVisible && (scrollOffset + i) < currentNetCount; i++) {
                int idx = scrollOffset + i;
                if (idx == selectedIndex || idx == prevSelectedIndex) {
                    int y = listY + i * itemH;
                    drawNetworkListItem(2, y, w - 4, itemH - 2, networks[idx], idx == selectedIndex);
                }
            }
        }
        prevSelectedIndex = selectedIndex;
        contentNeedsUpdate = false;
    }

    if (networks.empty()) {
        printCentered("Scanning...", h / 2, colors.textDim);
    }
}

// Helper to draw a single network list item
static void drawNetworkListItem(int x, int y, int w, int h, NetworkInfo& net, bool sel) {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    if (sel) {
        tft.fillRoundRect(x, y, w, h, 6, colors.secondary);
        tft.setTextColor(colors.background, colors.secondary);
    } else {
        tft.fillRoundRect(x, y, w, h, 6, colors.background);
        tft.drawRoundRect(x, y, w, h, 6, colors.textDim);
        tft.setTextColor(net.hasPortal ? colors.success : colors.text, colors.background);
    }

    // Status icon
    tft.setCursor(x + 8, y + 8);
    if (net.hasPortal) {
        tft.setTextColor(sel ? colors.background : colors.success, sel ? colors.secondary : colors.background);
        tft.print("*");
    } else if (net.isOpen) {
        tft.print("o");
    } else {
        tft.print("#");
    }

    // SSID
    tft.setCursor(x + 22, y + 8);
    String ssid = net.ssid;
    int maxLen = (w - 60) / 6;
    if ((int)ssid.length() > maxLen) ssid = ssid.substring(0, maxLen - 2) + "..";
    tft.print(ssid);

    // Signal bars on right
    int screenW = UI::getWidth();
    UI::drawSignalStrength(screenW - 24, y + 10, net.rssi);
}

// ============================================================
// PORTAL LIST SCREEN
// ============================================================

void UI::drawPortalsScreen() {
    int w = tft.width();
    int h = tft.height();

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);
        tft.setTextSize(1);
        tft.setTextColor(colors.success, colors.background);
        tft.setCursor(4, 4);
        tft.print("< VIEW PORTALS");
        tft.drawFastHLine(0, 16, w, colors.secondary);
        needsFullRedraw = false;
    }

    auto& portals = Scanner::getPortals();

    tft.fillRect(0, 20, w, 20, colors.background);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, 24);
    tft.printf("Captured: %d portal(s)", (int)portals.size());

    int listY = 44;
    int itemH = 40;
    int maxVisible = (h - listY - 20) / itemH;

    if (portals.empty()) {
        printCentered("No portals found", h / 2 - 10, colors.textDim);
        printCentered("Scan networks first", h / 2 + 10, colors.textDim);
    } else {
        for (int i = 0; i < maxVisible && i < (int)portals.size(); i++) {
            NetworkInfo* portal = portals[i];
            int y = listY + i * itemH;
            bool sel = (i == selectedIndex);

            if (sel) {
                tft.fillRoundRect(2, y, w - 4, itemH - 2, 6, colors.success);
                tft.setTextColor(colors.background, colors.success);
            } else {
                tft.fillRoundRect(2, y, w - 4, itemH - 2, 6, colors.background);
                tft.drawRoundRect(2, y, w - 4, itemH - 2, 6, colors.success);
                tft.setTextColor(colors.success, colors.background);
            }

            tft.setCursor(10, y + 8);
            tft.print(portal->ssid);

            tft.setTextColor(sel ? colors.background : colors.textDim, sel ? colors.success : colors.background);
            tft.setCursor(10, y + 22);
            tft.print(portal->analyzed ? "Analyzed" : "Click to analyze");
        }
    }

    tft.fillRect(0, h - 18, w, 18, colors.background);
    tft.drawFastHLine(0, h - 18, w, colors.secondary);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, h - 12);
    tft.print("[<] Back  [OK] Analyze");
}

// ============================================================
// ENUM SCREEN
// ============================================================

void UI::drawEnumScreen() {
    int w = tft.width();
    int h = tft.height();

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);
        tft.setTextSize(1);
        tft.setTextColor(colors.warning, colors.background);
        tft.setCursor(4, 4);
        tft.print("< ENUMERATION");
        tft.drawFastHLine(0, 16, w, colors.secondary);
        needsFullRedraw = false;
    }

    // Info card
    int cardY = 30;
    tft.drawRoundRect(10, cardY, w - 20, 80, 8, colors.warning);

    tft.setTextColor(colors.warning, colors.background);
    tft.setCursor(20, cardY + 10);
    tft.print("Credential Enumeration");

    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(20, cardY + 30);
    tft.print("Test room numbers and");
    tft.setCursor(20, cardY + 44);
    tft.print("common names against");
    tft.setCursor(20, cardY + 58);
    tft.print("hotel captive portals");

    // Button
    drawSquareButton(10, cardY + 100, w - 20, 50, "START ENUMERATION", selectedButton == 0, colors.warning);

    tft.fillRect(0, h - 18, w, 18, colors.background);
    tft.drawFastHLine(0, h - 18, w, colors.secondary);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, h - 12);
    tft.print("[<] Back");
}

// ============================================================
// LLM SCREEN
// ============================================================

void UI::drawLLMScreen() {
    int w = tft.width();
    int h = tft.height();

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);
        tft.setTextSize(1);
        tft.setTextColor(colors.accent, colors.background);
        tft.setCursor(4, 4);
        tft.print("< LLM INSIGHTS");
        tft.drawFastHLine(0, 16, w, colors.secondary);
        needsFullRedraw = false;
    }

    int cardY = 30;
    tft.drawRoundRect(10, cardY, w - 20, 100, 8, colors.accent);

    tft.setTextColor(colors.accent, colors.background);
    tft.setCursor(20, cardY + 10);
    tft.print("AI Portal Analysis");

    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(20, cardY + 30);
    tft.print("On-device LLM analyzes");
    tft.setCursor(20, cardY + 44);
    tft.print("portal HTML to identify");
    tft.setCursor(20, cardY + 58);
    tft.print("venue type and fields");

    #if LLM_ENABLED
    tft.setTextColor(colors.success, colors.background);
    tft.setCursor(20, cardY + 78);
    tft.printf("Model: %s [Ready]", LLM_MODEL);
    #else
    tft.setTextColor(colors.error, colors.background);
    tft.setCursor(20, cardY + 78);
    tft.print("LLM: Disabled");
    #endif

    tft.fillRect(0, h - 18, w, 18, colors.background);
    tft.drawFastHLine(0, h - 18, w, colors.secondary);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, h - 12);
    tft.print("[<] Back");
}

// ============================================================
// SETTINGS SCREEN
// ============================================================

void UI::drawSettingsScreen() {
    int w = tft.width();
    int h = tft.height();

    if (needsFullRedraw) {
        tft.fillScreen(colors.background);
        tft.setTextSize(1);
        tft.setTextColor(colors.textDim, colors.background);
        tft.setCursor(4, 4);
        tft.print("< SETTINGS");
        tft.drawFastHLine(0, 16, w, colors.secondary);
        needsFullRedraw = false;
    }

    printCentered("Settings", h / 2 - 20, colors.text);
    printCentered("Coming soon...", h / 2, colors.textDim);

    tft.fillRect(0, h - 18, w, 18, colors.background);
    tft.drawFastHLine(0, h - 18, w, colors.secondary);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, h - 12);
    tft.print("[<] Back");
}

void UI::drawNetworkDetailScreen() {
    drawPortalsScreen();  // Reuse for now
}

void UI::drawMainScreen() {
    drawMainMenu();
}

// ============================================================
// UPDATE & NAVIGATION
// ============================================================

void UI::update() {
    switch (currentScreen) {
        case SCREEN_BOOT:
            drawBootBanner();
            currentScreen = SCREEN_MAIN;
            needsFullRedraw = true;
            break;
        case SCREEN_MAIN:
            drawMainMenu();
            break;
        case SCREEN_SCANNER:
            drawScannerScreen();
            break;
        case SCREEN_PORTALS:
            drawPortalsScreen();
            break;
        case SCREEN_ENUM:
            drawEnumScreen();
            break;
        case SCREEN_LLM:
            drawLLMScreen();
            break;
        case SCREEN_SETTINGS:
            drawSettingsScreen();
            break;
        default:
            break;
    }
}

void UI::showScreen(Screen screen) {
    currentScreen = screen;
    selectedIndex = 0;
    selectedButton = 0;
    scrollOffset = 0;
    needsFullRedraw = true;
    update();
}

void UI::navigate(NavAction action) {
    Power::resetIdleTimer();
    lastInputTime = millis();

    switch (currentScreen) {
        case SCREEN_MAIN:
            // Main menu navigation
            if (action == NAV_UP && selectedButton > 0) {
                selectedButton--;
                needsFullRedraw = true;
            } else if (action == NAV_DOWN && selectedButton < 3) {
                selectedButton++;
                needsFullRedraw = true;
            } else if (action == NAV_SELECT) {
                switch (selectedButton) {
                    case 0: showScreen(SCREEN_SCANNER); break;
                    case 1: showScreen(SCREEN_PORTALS); break;
                    case 2: showScreen(SCREEN_ENUM); break;
                    case 3: showScreen(SCREEN_SETTINGS); break;
                }
            }
            break;

        case SCREEN_SCANNER:
            if (action == NAV_BACK || action == NAV_LEFT) {
                showScreen(SCREEN_MAIN);
            } else if (action == NAV_UP && selectedIndex > 0) {
                selectedIndex--;
                needsFullRedraw = true;
            } else if (action == NAV_DOWN) {
                auto& networks = Scanner::getNetworks();
                if (selectedIndex < (int)networks.size() - 1) {
                    selectedIndex++;
                    needsFullRedraw = true;
                }
            } else if (action == NAV_SELECT) {
                Scanner::checkForPortal(selectedIndex);
            }
            break;

        case SCREEN_PORTALS:
            if (action == NAV_BACK || action == NAV_LEFT) {
                showScreen(SCREEN_MAIN);
            } else if (action == NAV_UP && selectedIndex > 0) {
                selectedIndex--;
                needsFullRedraw = true;
            } else if (action == NAV_DOWN) {
                auto& portals = Scanner::getPortals();
                if (selectedIndex < (int)portals.size() - 1) {
                    selectedIndex++;
                    needsFullRedraw = true;
                }
            }
            break;

        case SCREEN_ENUM:
        case SCREEN_LLM:
        case SCREEN_SETTINGS:
            if (action == NAV_BACK || action == NAV_LEFT) {
                showScreen(SCREEN_MAIN);
            }
            break;

        default:
            break;
    }

    update();
}

void UI::setTab(TabIndex tab) {
    switch (tab) {
        case TAB_SCANNER: showScreen(SCREEN_SCANNER); break;
        case TAB_PORTALS: showScreen(SCREEN_PORTALS); break;
        case TAB_ENUM: showScreen(SCREEN_ENUM); break;
        case TAB_LLM: showScreen(SCREEN_LLM); break;
        default: break;
    }
    currentTab = tab;
}

TabIndex UI::getCurrentTab() {
    return currentTab;
}

void UI::setBrightness(uint8_t brightness) {
    #ifdef TFT_BL
    if (TFT_BL >= 0) {
        ledcSetup(0, 5000, 8);
        ledcAttachPin(TFT_BL, 0);
        ledcWrite(0, brightness);
    }
    #endif
}

void UI::clear() {
    tft.fillScreen(colors.background);
    needsFullRedraw = true;
}

// ============================================================
// DRAWING UTILITIES
// ============================================================

void UI::drawHeader(const char* title) {
    tft.fillRect(0, 0, tft.width(), 18, colors.background);
    tft.setTextColor(colors.primary, colors.background);
    tft.setTextSize(1);
    tft.setCursor(4, 4);
    tft.print(title);
    tft.drawFastHLine(0, 17, tft.width(), colors.secondary);
}

void UI::drawFooter(const char* left, const char* right) {
    int y = tft.height() - 14;
    tft.drawFastHLine(0, y - 2, tft.width(), colors.secondary);
    tft.fillRect(0, y, tft.width(), 14, colors.background);
    tft.setTextColor(colors.textDim, colors.background);
    tft.setTextSize(1);
    tft.setCursor(4, y + 3);
    tft.print(left);
    int rightW = strlen(right) * 6;
    tft.setCursor(tft.width() - rightW - 4, y + 3);
    tft.print(right);
}

void UI::drawProgressBar(int x, int y, int w, int h, int percent) {
    tft.drawRect(x, y, w, h, colors.primary);
    int fillW = ((w - 2) * percent) / 100;
    tft.fillRect(x + 1, y + 1, fillW, h - 2, colors.primary);
}

void UI::drawSignalStrength(int x, int y, int rssi) {
    int bars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
    for (int i = 0; i < 4; i++) {
        int bh = 3 + i * 2;
        int bx = x + i * 4;
        int by = y + (10 - bh);
        if (i < bars) {
            tft.fillRect(bx, by, 3, bh, colors.primary);
        } else {
            tft.drawRect(bx, by, 3, bh, colors.textDim);
        }
    }
}

void UI::drawBattery(int x, int y, int percent) {
    tft.drawRect(x, y, 20, 10, colors.text);
    tft.fillRect(x + 20, y + 3, 2, 4, colors.text);
    int fillW = (16 * percent) / 100;
    uint16_t fillC = percent < 20 ? colors.error : percent < 50 ? colors.warning : colors.success;
    tft.fillRect(x + 2, y + 2, fillW, 6, fillC);
}

void UI::print(const char* text, int x, int y, uint16_t color) {
    tft.setTextColor(color ? color : colors.text, colors.background);
    tft.setCursor(x, y);
    tft.print(text);
}

void UI::printCentered(const char* text, int y, uint16_t color) {
    int tw = strlen(text) * 6;
    print(text, (tft.width() - tw) / 2, y, color);
}

void UI::printTyped(const char* text, int x, int y, int delayMs) {
    tft.setTextColor(colors.text, colors.background);
    tft.setCursor(x, y);
    for (int i = 0; text[i]; i++) {
        tft.print(text[i]);
        delay(delayMs);
    }
}

void UI::drawCard(int x, int y, int w, int h, const char* title) {
    tft.drawRoundRect(x, y, w, h, 6, colors.primary);
    tft.fillRect(x + 1, y + 1, w - 2, 14, colors.primary);
    tft.setTextColor(colors.background, colors.primary);
    tft.setCursor(x + 4, y + 4);
    tft.print(title);
}

void UI::drawStatsBar(int networks, int open, int portals) {
    // Simplified stats in header
}

void UI::drawTabBar() {
    // Not used in button-based design
}

void UI::drawNetworkItem(int index, int y, bool selected) {
    // Handled in drawScannerScreen
}

void UI::drawStatusIcon(int x, int y, bool connected) {
    if (connected) {
        tft.fillCircle(x, y, 4, colors.success);
    } else {
        tft.drawCircle(x, y, 4, colors.textDim);
    }
}

TFT_eSPI& UI::getDisplay() { return tft; }
ColorPalette& UI::getColors() { return colors; }
int UI::getWidth() { return tft.width(); }
int UI::getHeight() { return tft.height(); }
