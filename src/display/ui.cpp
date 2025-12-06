#include "ui.h"
#include "config.h"
#include "core/scanner.h"
#include "core/power.h"

// Static member initialization
TFT_eSPI UI::tft = TFT_eSPI();
Screen UI::currentScreen = SCREEN_BOOT;
int UI::selectedIndex = 0;
int UI::scrollOffset = 0;
ColorPalette UI::colors;

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
            colors.background = tft.color565(20, 10, 40);   // Dark purple
            colors.primary = tft.color565(0, 255, 255);     // Cyan
            colors.secondary = tft.color565(255, 0, 255);   // Magenta
            colors.accent = tft.color565(255, 100, 255);    // Pink
            colors.text = tft.color565(0, 255, 255);
            colors.textDim = tft.color565(100, 100, 150);
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

void UI::update() {
    switch (currentScreen) {
        case SCREEN_SCANNER:
            drawScannerScreen();
            break;
        case SCREEN_PORTALS:
            drawPortalsScreen();
            break;
        case SCREEN_ANALYSIS:
            drawAnalysisScreen();
            break;
        case SCREEN_LOG:
            drawLogScreen();
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
    scrollOffset = 0;
    clear();
    update();
}

void UI::navigate(NavAction action) {
    Power::resetIdleTimer();  // Reset idle on any input

    switch (action) {
        case NAV_UP:
            if (selectedIndex > 0) {
                selectedIndex--;
            }
            break;
        case NAV_DOWN:
            selectedIndex++;
            break;
        case NAV_SELECT:
            // Handle selection based on current screen
            if (currentScreen == SCREEN_SCANNER) {
                // Check selected network for portal
                Scanner::checkForPortal(selectedIndex);
            } else if (currentScreen == SCREEN_PORTALS) {
                // Show analysis for selected portal
                showScreen(SCREEN_ANALYSIS);
            }
            break;
        case NAV_BACK:
            showScreen(SCREEN_SCANNER);
            break;
        default:
            break;
    }

    update();
}

void UI::setBrightness(uint8_t brightness) {
    #ifdef TFT_BL
    if (TFT_BL >= 0) {
        ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit
        ledcAttachPin(TFT_BL, 0);
        ledcWrite(0, brightness);
    }
    #endif
}

void UI::clear() {
    tft.fillScreen(colors.background);
}

void UI::drawHeader(const char* title) {
    // Top bar background
    tft.fillRect(0, 0, tft.width(), 20, colors.background);

    // Title
    tft.setTextColor(colors.primary, colors.background);
    tft.setTextSize(1);
    tft.setCursor(4, 6);
    tft.print(title);

    // Battery indicator (top right)
    int batteryPercent = Power::getBatteryPercent();
    drawBattery(tft.width() - 28, 4, batteryPercent);

    // Divider line
    tft.drawFastHLine(0, 19, tft.width(), colors.secondary);
}

void UI::drawFooter(const char* left, const char* right) {
    int y = tft.height() - 16;

    // Divider line
    tft.drawFastHLine(0, y - 2, tft.width(), colors.secondary);

    // Footer background
    tft.fillRect(0, y, tft.width(), 16, colors.background);

    tft.setTextColor(colors.textDim, colors.background);
    tft.setTextSize(1);

    // Left text
    tft.setCursor(4, y + 4);
    tft.print(left);

    // Right text
    int rightWidth = strlen(right) * 6;
    tft.setCursor(tft.width() - rightWidth - 4, y + 4);
    tft.print(right);
}

void UI::drawProgressBar(int x, int y, int w, int h, int percent) {
    // Border
    tft.drawRect(x, y, w, h, colors.primary);

    // Fill
    int fillWidth = ((w - 2) * percent) / 100;
    tft.fillRect(x + 1, y + 1, fillWidth, h - 2, colors.primary);
}

void UI::drawSignalStrength(int x, int y, int rssi) {
    // Convert RSSI to bars (0-4)
    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;

    // Draw bars
    for (int i = 0; i < 4; i++) {
        int barHeight = 3 + (i * 2);
        int barX = x + (i * 4);
        int barY = y + (10 - barHeight);

        if (i < bars) {
            tft.fillRect(barX, barY, 3, barHeight, colors.primary);
        } else {
            tft.drawRect(barX, barY, 3, barHeight, colors.textDim);
        }
    }
}

void UI::drawBattery(int x, int y, int percent) {
    // Battery outline
    tft.drawRect(x, y, 20, 10, colors.text);
    tft.fillRect(x + 20, y + 3, 2, 4, colors.text);  // Battery tip

    // Fill based on percent
    int fillWidth = (16 * percent) / 100;
    uint16_t fillColor = colors.success;
    if (percent < 20) fillColor = colors.error;
    else if (percent < 50) fillColor = colors.warning;

    tft.fillRect(x + 2, y + 2, fillWidth, 6, fillColor);
}

void UI::print(const char* text, int x, int y, uint16_t color) {
    tft.setTextColor(color ? color : colors.text, colors.background);
    tft.setCursor(x, y);
    tft.print(text);
}

void UI::printCentered(const char* text, int y, uint16_t color) {
    int textWidth = strlen(text) * 6;  // Approximate width
    int x = (tft.width() - textWidth) / 2;
    print(text, x, y, color);
}

void UI::printTyped(const char* text, int x, int y, int delayMs) {
    tft.setTextColor(colors.text, colors.background);
    tft.setCursor(x, y);

    for (int i = 0; text[i] != '\0'; i++) {
        tft.print(text[i]);
        delay(delayMs);
    }
}

// === SCREEN DRAWING ===

void UI::drawScannerScreen() {
    drawHeader("[ SCANNER ]");

    auto& networks = Scanner::getNetworks();
    int portalCount = Scanner::getPortalCount();

    // Stats line
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(4, 24);
    tft.printf("Networks: %d | Portals: %d", networks.size(), portalCount);

    // Network list
    int listY = 38;
    int itemHeight = 16;
    int maxVisible = (tft.height() - listY - 20) / itemHeight;

    // Clamp scroll offset
    if (selectedIndex >= scrollOffset + maxVisible) {
        scrollOffset = selectedIndex - maxVisible + 1;
    }
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    }

    for (int i = 0; i < maxVisible && (scrollOffset + i) < networks.size(); i++) {
        int idx = scrollOffset + i;
        NetworkInfo& net = networks[idx];
        int y = listY + (i * itemHeight);

        // Selection highlight
        if (idx == selectedIndex) {
            tft.fillRect(0, y, tft.width(), itemHeight, colors.secondary);
            tft.setTextColor(colors.background, colors.secondary);
        } else {
            tft.fillRect(0, y, tft.width(), itemHeight, colors.background);
            tft.setTextColor(net.hasPortal ? colors.accent : colors.text, colors.background);
        }

        // Network icon
        if (net.hasPortal) {
            tft.setCursor(4, y + 4);
            tft.print("*");  // Portal indicator
        } else if (net.isOpen) {
            tft.setCursor(4, y + 4);
            tft.print("o");  // Open network
        } else {
            tft.setCursor(4, y + 4);
            tft.print("#");  // Secured
        }

        // SSID (truncated if needed)
        String ssid = net.ssid;
        if (ssid.length() > 18) {
            ssid = ssid.substring(0, 15) + "...";
        }
        tft.setCursor(14, y + 4);
        tft.print(ssid);

        // Signal strength (right side)
        drawSignalStrength(tft.width() - 20, y + 3, net.rssi);
    }

    // Scroll indicator
    if (networks.size() > maxVisible) {
        int scrollBarHeight = tft.height() - listY - 20;
        int thumbHeight = max(10, (scrollBarHeight * maxVisible) / (int)networks.size());
        int thumbY = listY + (scrollBarHeight * scrollOffset) / (int)networks.size();
        tft.fillRect(tft.width() - 3, listY, 2, scrollBarHeight, colors.textDim);
        tft.fillRect(tft.width() - 3, thumbY, 2, thumbHeight, colors.primary);
    }

    drawFooter("[<] Scroll", "[>] Check");
}

void UI::drawPortalsScreen() {
    drawHeader("[ PORTALS ]");

    auto& portals = Scanner::getPortals();

    if (portals.empty()) {
        printCentered("No portals detected", tft.height() / 2 - 8, colors.textDim);
        printCentered("Scan networks first", tft.height() / 2 + 8, colors.textDim);
    } else {
        int listY = 28;
        int itemHeight = 24;

        for (int i = 0; i < portals.size() && i < 6; i++) {
            NetworkInfo* portal = portals[i];
            int y = listY + (i * itemHeight);

            // Selection
            if (i == selectedIndex) {
                tft.fillRect(0, y, tft.width(), itemHeight - 2, colors.secondary);
                tft.setTextColor(colors.background, colors.secondary);
            } else {
                tft.setTextColor(colors.primary, colors.background);
            }

            tft.setCursor(4, y + 4);
            tft.print(portal->ssid);

            tft.setTextColor(colors.textDim, i == selectedIndex ? colors.secondary : colors.background);
            tft.setCursor(4, y + 14);
            tft.print(portal->analyzed ? "Analyzed" : "Pending");
        }
    }

    drawFooter("[<] Back", "[>] Analyze");
}

void UI::drawAnalysisScreen() {
    drawHeader("[ ANALYSIS ]");

    auto& portals = Scanner::getPortals();
    if (selectedIndex >= portals.size()) {
        printCentered("No portal selected", tft.height() / 2, colors.textDim);
        return;
    }

    NetworkInfo* portal = portals[selectedIndex];

    tft.setTextColor(colors.primary, colors.background);
    tft.setCursor(4, 28);
    tft.print("SSID: ");
    tft.print(portal->ssid);

    tft.setCursor(4, 44);
    tft.print("URL: ");
    String url = portal->portalUrl.length() > 20
        ? portal->portalUrl.substring(0, 17) + "..."
        : portal->portalUrl;
    tft.print(url);

    tft.setCursor(4, 60);
    tft.printf("Captured: %d bytes", portal->portalHtml.length());

    if (portal->analyzed) {
        tft.setCursor(4, 80);
        tft.setTextColor(colors.success, colors.background);
        tft.print("Analysis complete");
        // Would show LLM results here
    } else {
        tft.setCursor(4, 80);
        tft.setTextColor(colors.warning, colors.background);
        tft.print("Awaiting analysis...");
    }

    drawFooter("[<] Back", "[>] Run LLM");
}

void UI::drawLogScreen() {
    drawHeader("[ LOG ]");
    printCentered("Coming soon...", tft.height() / 2, colors.textDim);
    drawFooter("[<] Back", "");
}

void UI::drawSettingsScreen() {
    drawHeader("[ SETTINGS ]");
    printCentered("Coming soon...", tft.height() / 2, colors.textDim);
    drawFooter("[<] Back", "");
}

TFT_eSPI& UI::getDisplay() {
    return tft;
}

ColorPalette& UI::getColors() {
    return colors;
}

int UI::getWidth() {
    return tft.width();
}

int UI::getHeight() {
    return tft.height();
}
