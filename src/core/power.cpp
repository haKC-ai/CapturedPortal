#include "power.h"
#include "config.h"
#include <esp_sleep.h>
#include <driver/adc.h>

// Static member initialization
PowerMode Power::currentMode = POWER_BATTERY;
unsigned long Power::lastActivity = 0;
bool Power::sleepPrevented = false;
float Power::batteryVoltage = 0.0;

void Power::init(PowerMode mode) {
    currentMode = mode;
    lastActivity = millis();

    #ifdef BATTERY_ADC
    if (BATTERY_ADC >= 0) {
        // Configure ADC for battery monitoring
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);
    }
    #endif

    #if DEBUG_SERIAL
    Serial.printf("[POWER] Initialized in %s mode\n",
        mode == POWER_USB ? "USB" : "BATTERY");
    #endif
}

PowerMode Power::detectMode() {
    #ifdef BATTERY_ADC
    if (BATTERY_ADC >= 0) {
        // Read battery/USB voltage
        float voltage = getBatteryVoltage();

        #if DEBUG_SERIAL
        Serial.printf("[POWER] Detected voltage: %.2fV\n", voltage);
        #endif

        // If voltage is above threshold, likely on USB power
        if (voltage >= USB_VOLTAGE_THRESHOLD) {
            return POWER_USB;
        }
    }
    #endif

    // Default to USB mode if no battery ADC or can't detect
    // Better to have full features than accidentally limit them
    return POWER_USB;
}

PowerMode Power::getMode() {
    return currentMode;
}

float Power::getBatteryVoltage() {
    #ifdef BATTERY_ADC
    if (BATTERY_ADC >= 0) {
        // Read ADC and convert to voltage
        // Assumes a voltage divider (typically 2:1 on most boards)
        int raw = analogRead(BATTERY_ADC);
        // 12-bit ADC: 0-4095, reference ~3.3V, divider ratio 2:1
        float voltage = (raw / 4095.0) * 3.3 * 2.0;
        batteryVoltage = voltage;
        return voltage;
    }
    #endif
    return 5.0;  // Assume USB voltage if no ADC
}

int Power::getBatteryPercent() {
    float voltage = batteryVoltage > 0 ? batteryVoltage : getBatteryVoltage();

    // LiPo voltage range: ~3.2V (empty) to ~4.2V (full)
    // Linear approximation (not perfectly accurate but close enough)
    if (voltage >= 4.2) return 100;
    if (voltage <= 3.2) return 0;

    return (int)((voltage - 3.2) / (4.2 - 3.2) * 100);
}

bool Power::isCharging() {
    // Most boards don't expose charging status
    // Could be implemented for specific boards with charging ICs
    return currentMode == POWER_USB && getBatteryPercent() < 100;
}

void Power::checkIdle() {
    if (sleepPrevented) return;

    unsigned long idleTime = millis() - lastActivity;

    if (currentMode == POWER_BATTERY) {
        if (idleTime > DEEP_SLEEP_TIMEOUT) {
            #if DEBUG_SERIAL
            Serial.println("[POWER] Deep sleep due to idle...");
            #endif
            deepSleep(0);  // Wake on button press
        } else if (idleTime > IDLE_SLEEP_TIMEOUT) {
            #if DEBUG_SERIAL
            Serial.println("[POWER] Light sleep due to idle...");
            #endif
            lightSleep(IDLE_SLEEP_TIMEOUT);
        }
    }
}

void Power::resetIdleTimer() {
    lastActivity = millis();
}

void Power::lightSleep(unsigned long durationMs) {
    // Configure timer wakeup
    esp_sleep_enable_timer_wakeup(durationMs * 1000);  // microseconds

    // Configure button wakeup
    #ifdef BTN_LEFT
    if (BTN_LEFT >= 0) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_LEFT, 0);  // Wake on LOW
    }
    #endif

    // Enter light sleep
    esp_light_sleep_start();

    // Reset idle timer after wakeup
    lastActivity = millis();
}

void Power::deepSleep(unsigned long durationMs) {
    // Configure wakeup sources
    #ifdef BTN_LEFT
    if (BTN_LEFT >= 0) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_LEFT, 0);  // Wake on button press
    }
    #endif

    if (durationMs > 0) {
        esp_sleep_enable_timer_wakeup(durationMs * 1000);
    }

    #if DEBUG_SERIAL
    Serial.println("[POWER] Entering deep sleep...");
    Serial.flush();
    #endif

    // Enter deep sleep (will reset on wakeup)
    esp_deep_sleep_start();
}

void Power::preventSleep() {
    sleepPrevented = true;
}

void Power::allowSleep() {
    sleepPrevented = false;
    lastActivity = millis();
}
