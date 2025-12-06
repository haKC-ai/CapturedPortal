#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// Captured Portal Configuration
// ==========================================

// Version
#define VERSION "0.1.0"
#define DEVICE_NAME "CapturedPortal"

// ==========================================
// Display Settings
// ==========================================

// Rotation: 0=Portrait, 1=Landscape, 2=Portrait Inverted, 3=Landscape Inverted
#define DISPLAY_ROTATION 1

// Color schemes
#define COLOR_MATRIX    0  // Classic green on black
#define COLOR_SYNTHWAVE 1  // Cyan/magenta/purple
#define COLOR_CYBERPUNK 2  // Hot pink/yellow
#define COLOR_DRACULA   3  // Purple theme

#define COLOR_SCHEME COLOR_SYNTHWAVE

// Animation speeds (ms)
#define ANIMATION_FRAME_DELAY 50
#define MATRIX_RAIN_SPEED 80
#define DECRYPT_SPEED 30
#define SCROLL_SPEED 100

// ==========================================
// Power Management
// ==========================================

// Auto-detect USB vs Battery
#define USB_VOLTAGE_THRESHOLD 4.5  // Volts

// Scan intervals (ms)
#define BATTERY_SCAN_INTERVAL 10000  // 10s on battery (save power)
#define USB_SCAN_INTERVAL 3000       // 3s when plugged in

// Sleep settings for battery mode
#define IDLE_SLEEP_TIMEOUT 60000     // 1 min idle -> light sleep
#define DEEP_SLEEP_TIMEOUT 300000    // 5 min idle -> deep sleep

// Display brightness (0-255)
#define BRIGHTNESS_BATTERY 128       // Dimmer on battery
#define BRIGHTNESS_USB 255           // Full brightness on USB

// ==========================================
// WiFi Scanner Settings
// ==========================================

// Maximum networks to track
#define MAX_NETWORKS 50

// Portal detection endpoints (tries in order)
#define PORTAL_CHECK_URLS { \
    "http://connectivitycheck.gstatic.com/generate_204", \
    "http://www.msftconnecttest.com/connecttest.txt", \
    "http://captive.apple.com/hotspot-detect.html" \
}

// Connection timeout for portal check (ms)
#define PORTAL_CHECK_TIMEOUT 5000

// Max portal HTML capture size (bytes)
#define MAX_PORTAL_CAPTURE_SIZE 32768

// ==========================================
// Web Server Settings
// ==========================================

#define WEB_SERVER_ENABLED true
#define WEB_SERVER_ON_BATTERY true  // Set true to enable web server on battery (uses more power)
#define AP_SSID_PREFIX "haKC"
#define AP_HIDDEN false  // Set true to hide SSID (won't broadcast network name)
#define AP_PASSWORD "haKCer23"  // Open by default, set for security
#define WEB_SERVER_PORT 80

// API rate limiting
#define API_RATE_LIMIT 60  // requests per minute

// ==========================================
// LLM Settings
// ==========================================

#define LLM_ENABLED true
#define LLM_ON_BATTERY true  // Set true to enable LLM on battery (uses more power)

// Model selection
// Options: "tinyllama", "phi2-tiny", "custom"
#define LLM_MODEL "tinyllama"

// Inference settings
#define LLM_MAX_TOKENS 256
#define LLM_TEMPERATURE 0.7
#define LLM_CONTEXT_SIZE 512

// ==========================================
// Logging & Storage
// ==========================================

// Use SD card if available, otherwise SPIFFS
#define USE_SD_CARD_IF_AVAILABLE true

// Log file settings
#define LOG_FILE_PATH "/logs/portals.json"
#define MAX_LOG_ENTRIES 1000
#define LOG_ROTATION_SIZE 1048576  // 1MB

// ==========================================
// Hardware Pin Definitions
// ==========================================
// Note: Most pins are defined per-board in platformio.ini
// These are common definitions

// Button pins (LILYGO T-Display S3)
#ifdef LILYGO_T_DISPLAY_S3
    #define BTN_LEFT 0
    #define BTN_RIGHT 14
    #define LED_PIN 38
    #define BATTERY_ADC 4
#endif

// Waveshare LCD 1.47 (USB Dongle)
#ifdef WAVESHARE_LCD_147
    #define BTN_LEFT 0
    #define BTN_RIGHT -1  // No second button
    #define LED_PIN 48    // RGB LED
    #define SD_CS 10
#endif

// Round LCD 1.28"
#ifdef ESP32S3_ROUND_128
    #define BTN_LEFT 0
    #define BTN_RIGHT -1
    #define LED_PIN -1
    #define BATTERY_ADC 1
#endif

// Waveshare Touch 1.69"
#ifdef WAVESHARE_TOUCH_169
    #define BTN_LEFT 0
    #define BTN_RIGHT -1  // Use touch instead
    #define LED_PIN -1
    #define BATTERY_ADC 1
#endif

// LILYGO T-Deck (Keyboard + Trackball)
#ifdef LILYGO_T_DECK
    #define BTN_LEFT -1           // Use trackball instead
    #define BTN_RIGHT -1
    #define LED_PIN -1
    #define BATTERY_ADC 4
    #define HAS_KEYBOARD true
    #define HAS_TRACKBALL true
    #define HAS_SPEAKER true
    #define HAS_MICROPHONE true
    #define PERI_POWERON 10       // Peripheral power enable
    #define KEYBOARD_I2C_ADDR 0x55
    #define KEYBOARD_INT_PIN 46
    #define TRACKBALL_UP_PIN 3
    #define TRACKBALL_DOWN_PIN 15
    #define TRACKBALL_LEFT_PIN 1
    #define TRACKBALL_RIGHT_PIN 2
    #define TRACKBALL_CLICK_PIN 0
    #define SD_CS 39
    #define SPEAKER_PIN 2
    #define MIC_PIN 14
#endif

// ==========================================
// Debug Settings
// ==========================================

#define DEBUG_SERIAL true
#define DEBUG_BAUD 115200

// Verbose logging categories
#define DEBUG_WIFI true
#define DEBUG_PORTAL true
#define DEBUG_LLM false
#define DEBUG_DISPLAY false

#endif // CONFIG_H
