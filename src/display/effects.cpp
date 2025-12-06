#include "effects.h"
#include "ui.h"
#include "config.h"

// Static member initialization
EffectType Effects::currentEffect = EFFECT_NONE;
unsigned long Effects::effectStartTime = 0;
int Effects::effectFrame = 0;

int Effects::dropX[MAX_DROPS];
int Effects::dropY[MAX_DROPS];
int Effects::dropSpeed[MAX_DROPS];
char Effects::dropChar[MAX_DROPS];

// Characters for matrix effect
const char matrixChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&*<>[]{}";
const int matrixCharCount = sizeof(matrixChars) - 1;

void Effects::init() {
    // Initialize matrix rain drops
    for (int i = 0; i < MAX_DROPS; i++) {
        dropX[i] = random(0, UI::getWidth());
        dropY[i] = random(-100, 0);
        dropSpeed[i] = random(2, 6);
        dropChar[i] = matrixChars[random(matrixCharCount)];
    }
}

void Effects::update() {
    switch (currentEffect) {
        case EFFECT_MATRIX_RAIN:
            updateMatrixRain();
            break;
        case EFFECT_SCAN_LINE:
            updateScanLine();
            break;
        case EFFECT_WAVE:
            updateWave();
            break;
        default:
            break;
    }
}

void Effects::bootSequence() {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    // Clear screen
    tft.fillScreen(TFT_BLACK);

    // Matrix rain for 2 seconds
    unsigned long start = millis();
    while (millis() - start < 2000) {
        updateMatrixRain();
        delay(MATRIX_RAIN_SPEED);
    }

    // Clear and show boot text
    tft.fillScreen(colors.background);

    // ASCII art logo (scaled down for small screens)
    tft.setTextColor(colors.primary, colors.background);
    tft.setTextSize(1);

    int centerY = UI::getHeight() / 2 - 40;

    // Decrypt effect on title
    decrypt("CAPTURED", UI::getWidth()/2 - 24, centerY, 800);
    decrypt("PORTAL", UI::getWidth()/2 - 18, centerY + 16, 600);

    delay(300);

    // Version
    tft.setTextColor(colors.textDim, colors.background);
    tft.setCursor(UI::getWidth()/2 - 20, centerY + 40);
    tft.print("v");
    tft.print(VERSION);

    delay(500);

    // Loading bar
    int barY = centerY + 60;
    int barWidth = UI::getWidth() - 40;
    int barX = 20;

    tft.drawRect(barX, barY, barWidth, 8, colors.primary);

    for (int i = 0; i <= 100; i += 5) {
        int fillWidth = ((barWidth - 2) * i) / 100;
        tft.fillRect(barX + 1, barY + 1, fillWidth, 6, colors.primary);
        delay(30);
    }

    delay(300);

    // Scan line wipe transition
    for (int y = 0; y < UI::getHeight(); y += 4) {
        tft.fillRect(0, y, UI::getWidth(), 4, colors.background);
        delay(10);
    }
}

void Effects::matrixRain(int duration) {
    startEffect(EFFECT_MATRIX_RAIN);
    if (duration > 0) {
        unsigned long start = millis();
        while (millis() - start < duration) {
            updateMatrixRain();
            delay(MATRIX_RAIN_SPEED);
        }
        stopEffect();
    }
}

void Effects::updateMatrixRain() {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    // Fade effect (darken existing pixels slightly)
    // Note: True fade would require reading pixels, so we just draw new

    for (int i = 0; i < MAX_DROPS; i++) {
        // Erase previous position with dim character
        if (dropY[i] > 0) {
            tft.setTextColor(colors.textDim, colors.background);
            tft.setCursor(dropX[i], dropY[i] - dropSpeed[i]);
            tft.print(dropChar[i]);
        }

        // Draw new position with bright character
        if (dropY[i] >= 0 && dropY[i] < UI::getHeight()) {
            tft.setTextColor(colors.primary, colors.background);
            tft.setCursor(dropX[i], dropY[i]);
            tft.print(dropChar[i]);

            // Trail (slightly dimmer)
            if (dropY[i] > 8) {
                tft.setTextColor(colors.secondary, colors.background);
                tft.setCursor(dropX[i], dropY[i] - 8);
                tft.print(matrixChars[random(matrixCharCount)]);
            }
        }

        // Update position
        dropY[i] += dropSpeed[i];

        // Reset if off screen
        if (dropY[i] > UI::getHeight()) {
            dropX[i] = random(0, UI::getWidth() / 8) * 8;  // Align to char grid
            dropY[i] = random(-50, 0);
            dropSpeed[i] = random(2, 6);
            dropChar[i] = matrixChars[random(matrixCharCount)];
        }
    }

    effectFrame++;
}

void Effects::decrypt(const char* text, int x, int y, int duration) {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    int len = strlen(text);
    char display[64];
    bool revealed[64] = {false};

    // Initialize with random characters
    for (int i = 0; i < len; i++) {
        display[i] = matrixChars[random(matrixCharCount)];
    }
    display[len] = '\0';

    int revealedCount = 0;
    unsigned long start = millis();
    unsigned long revealInterval = duration / len;

    while (revealedCount < len) {
        // Randomly cycle non-revealed characters
        for (int i = 0; i < len; i++) {
            if (!revealed[i]) {
                display[i] = matrixChars[random(matrixCharCount)];
            }
        }

        // Draw current state
        tft.setTextColor(colors.primary, colors.background);
        tft.setCursor(x, y);
        tft.print(display);

        delay(DECRYPT_SPEED);

        // Reveal next character if time
        if (millis() - start > revealInterval * (revealedCount + 1)) {
            // Find a random unrevealed character to reveal
            int attempts = 0;
            while (attempts < 20) {
                int idx = random(len);
                if (!revealed[idx]) {
                    revealed[idx] = true;
                    display[idx] = text[idx];
                    revealedCount++;
                    break;
                }
                attempts++;
            }

            // Fallback: reveal sequentially
            if (attempts >= 20) {
                for (int i = 0; i < len; i++) {
                    if (!revealed[i]) {
                        revealed[i] = true;
                        display[i] = text[i];
                        revealedCount++;
                        break;
                    }
                }
            }
        }
    }

    // Final reveal
    tft.setTextColor(colors.primary, colors.background);
    tft.setCursor(x, y);
    tft.print(text);
}

void Effects::scanLine() {
    startEffect(EFFECT_SCAN_LINE);
}

void Effects::updateScanLine() {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    static int scanY = 0;

    // Draw scan line
    tft.drawFastHLine(0, scanY, UI::getWidth(), colors.accent);

    // Erase previous line (2 pixels behind)
    if (scanY > 2) {
        tft.drawFastHLine(0, scanY - 2, UI::getWidth(), colors.background);
    }

    scanY++;
    if (scanY >= UI::getHeight()) {
        scanY = 0;
    }

    effectFrame++;
}

void Effects::glitch(int intensity) {
    TFT_eSPI& tft = UI::getDisplay();

    for (int i = 0; i < intensity; i++) {
        // Random horizontal displacement
        int y = random(UI::getHeight());
        int h = random(1, 10);
        int offset = random(-10, 10);

        // This would need to read/write pixels, simplified version:
        // Just draw some random colored bars
        uint16_t glitchColor = random(0xFFFF);
        tft.fillRect(offset < 0 ? 0 : offset, y, UI::getWidth(), h, glitchColor);
    }
}

void Effects::typeText(const char* text, int x, int y, int speed) {
    TFT_eSPI& tft = UI::getDisplay();
    ColorPalette& colors = UI::getColors();

    tft.setTextColor(colors.text, colors.background);
    tft.setCursor(x, y);

    for (int i = 0; text[i] != '\0'; i++) {
        tft.print(text[i]);

        // Cursor blink effect
        int cursorX = tft.getCursorX();
        tft.print("_");
        delay(speed);
        tft.setCursor(cursorX, y);
        tft.print(" ");
        tft.setCursor(cursorX, y);
    }
}

void Effects::wave(int amplitude) {
    startEffect(EFFECT_WAVE);
}

void Effects::updateWave() {
    // Would implement wavy text effect
    effectFrame++;
}

void Effects::startEffect(EffectType effect) {
    currentEffect = effect;
    effectStartTime = millis();
    effectFrame = 0;
}

void Effects::stopEffect() {
    currentEffect = EFFECT_NONE;
}

bool Effects::isEffectRunning() {
    return currentEffect != EFFECT_NONE;
}
