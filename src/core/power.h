#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

// Power modes
enum PowerMode {
    POWER_BATTERY,  // Battery powered - conserve energy
    POWER_USB       // USB powered - full capabilities
};

class Power {
public:
    static void init(PowerMode mode);
    static PowerMode detectMode();
    static PowerMode getMode();
    static void checkIdle();
    static void resetIdleTimer();

    // Battery monitoring
    static float getBatteryVoltage();
    static int getBatteryPercent();
    static bool isCharging();

    // Sleep management
    static void lightSleep(unsigned long durationMs);
    static void deepSleep(unsigned long durationMs);
    static void preventSleep();
    static void allowSleep();

private:
    static PowerMode currentMode;
    static unsigned long lastActivity;
    static bool sleepPrevented;
    static float batteryVoltage;
};

#endif // POWER_H
