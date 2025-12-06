#ifndef ENUMERATOR_H
#define ENUMERATOR_H

#include <Arduino.h>
#include <vector>
#include "scanner.h"

// Form field types detected in portal
enum FieldType {
    FIELD_UNKNOWN,
    FIELD_ROOM_NUMBER,
    FIELD_LAST_NAME,
    FIELD_FIRST_NAME,
    FIELD_EMAIL,
    FIELD_PHONE,
    FIELD_CODE,
    FIELD_CHECKBOX,
    FIELD_BUTTON
};

// Detected form field
struct FormField {
    String name;
    String id;
    String type;
    String placeholder;
    FieldType detectedType;
    bool required;
};

// Enumeration attempt result
struct EnumAttempt {
    String fieldValues;      // JSON object of field:value pairs
    int responseCode;
    String responseSnippet;  // First 200 chars of response
    bool success;            // Did it appear to succeed?
    unsigned long timestamp;
};

// Enumeration result summary
struct EnumResult {
    int totalAttempts;
    int successfulAttempts;
    int failedAttempts;
    std::vector<EnumAttempt> successes;
    std::vector<String> discoveredPatterns;
    String estimatedRoomCount;
    String venueInsights;
};

class Enumerator {
public:
    static void init();

    // Form analysis
    static std::vector<FormField> analyzePortalForm(const String& html);
    static FieldType detectFieldType(const FormField& field);

    // Enumeration
    static EnumResult enumerate(NetworkInfo* portal, int maxAttempts = 100);
    static bool testCredentials(const String& url, const std::vector<FormField>& fields,
                                const String& roomNumber, const String& lastName);

    // Wordlist management
    static std::vector<String> loadRoomNumbers();
    static std::vector<String> loadSurnames();
    static void addCustomRoom(const String& room);
    static void addCustomSurname(const String& surname);

    // Progress callback
    typedef void (*ProgressCallback)(int current, int total, const String& status);
    static void setProgressCallback(ProgressCallback cb);

private:
    static std::vector<String> roomNumbers;
    static std::vector<String> surnames;
    static std::vector<String> customRooms;
    static std::vector<String> customSurnames;
    static ProgressCallback progressCb;

    static String extractFormAction(const String& html);
    static String extractFormMethod(const String& html);
    static std::vector<FormField> extractFormFields(const String& html);
    static String buildPostData(const std::vector<FormField>& fields,
                                const String& roomNumber, const String& lastName);
    static bool isSuccessResponse(int httpCode, const String& response);
};

#endif // ENUMERATOR_H
