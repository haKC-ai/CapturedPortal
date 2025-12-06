#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>

// Effect types
enum EffectType {
    EFFECT_NONE,
    EFFECT_MATRIX_RAIN,
    EFFECT_DECRYPT,
    EFFECT_SCAN_LINE,
    EFFECT_GLITCH,
    EFFECT_TYPING,
    EFFECT_WAVE
};

class Effects {
public:
    static void init();
    static void update();

    // Boot sequence
    static void bootSequence();

    // Individual effects
    static void matrixRain(int duration = 0);
    static void decrypt(const char* text, int x, int y, int duration = 1000);
    static void scanLine();
    static void glitch(int intensity = 5);
    static void typeText(const char* text, int x, int y, int speed = 30);
    static void wave(int amplitude = 5);

    // Effect control
    static void startEffect(EffectType effect);
    static void stopEffect();
    static bool isEffectRunning();

private:
    static EffectType currentEffect;
    static unsigned long effectStartTime;
    static int effectFrame;

    // Matrix rain state
    static const int MAX_DROPS = 20;
    static int dropX[MAX_DROPS];
    static int dropY[MAX_DROPS];
    static int dropSpeed[MAX_DROPS];
    static char dropChar[MAX_DROPS];

    static void updateMatrixRain();
    static void updateScanLine();
    static void updateWave();
};

#endif // EFFECTS_H
