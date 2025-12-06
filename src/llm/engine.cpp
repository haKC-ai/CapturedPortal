#include "engine.h"
#include "config.h"
#include <SPIFFS.h>

// Static member initialization
bool LLMEngine::initialized = false;
bool LLMEngine::modelLoaded = false;
String LLMEngine::currentModel = "";

// NOTE: Full LLM integration requires the esp32-llm library
// This implementation provides a pattern-based fallback that works without
// the full LLM model, while maintaining the interface for future integration.
//
// To enable full LLM support:
// 1. Add esp32-llm library to lib_deps in platformio.ini
// 2. Download and flash a quantized model to SPIFFS/SD
// 3. Uncomment the LLM inference code below

void LLMEngine::init() {
    if (initialized) return;

    #if DEBUG_SERIAL
    Serial.println("[LLM] Initializing engine...");
    #endif

    // Check available memory
    size_t freeHeap = ESP.getFreeHeap();
    size_t freePsram = ESP.getFreePsram();

    #if DEBUG_SERIAL
    Serial.printf("[LLM] Free heap: %d bytes\n", freeHeap);
    Serial.printf("[LLM] Free PSRAM: %d bytes\n", freePsram);
    #endif

    // For full LLM support, you would initialize the model here:
    // #if LLM_ENABLED
    //     if (freePsram >= 2 * 1024 * 1024) {  // Need at least 2MB PSRAM
    //         loadModel("/models/tinyllama.bin");
    //     }
    // #endif

    initialized = true;

    #if DEBUG_SERIAL
    Serial.printf("[LLM] Engine initialized (model loaded: %s)\n",
        modelLoaded ? "yes" : "no - using pattern matching");
    #endif
}

bool LLMEngine::isAvailable() {
    return initialized;
}

bool LLMEngine::isReady() {
    return initialized && modelLoaded;
}

bool LLMEngine::loadModel(const String& modelPath) {
    #if DEBUG_SERIAL
    Serial.printf("[LLM] Loading model: %s\n", modelPath.c_str());
    #endif

    // Check if file exists
    if (!SPIFFS.exists(modelPath)) {
        #if DEBUG_SERIAL
        Serial.println("[LLM] Model file not found");
        #endif
        return false;
    }

    // TODO: Implement actual model loading with esp32-llm
    // For now, just mark as loaded if file exists

    currentModel = modelPath;
    modelLoaded = true;

    #if DEBUG_SERIAL
    Serial.println("[LLM] Model loaded successfully");
    #endif

    return true;
}

void LLMEngine::unloadModel() {
    modelLoaded = false;
    currentModel = "";

    #if DEBUG_SERIAL
    Serial.println("[LLM] Model unloaded");
    #endif
}

size_t LLMEngine::getModelSize() {
    if (!modelLoaded) return 0;

    File f = SPIFFS.open(currentModel, "r");
    if (!f) return 0;

    size_t size = f.size();
    f.close();
    return size;
}

size_t LLMEngine::getFreeMemory() {
    return ESP.getFreeHeap() + ESP.getFreePsram();
}

LLMAnalysis LLMEngine::analyzePortalHTML(const String& html) {
    if (!initialized) {
        init();
    }

    // If LLM model is loaded, use it
    if (modelLoaded) {
        String prompt = buildAnalysisPrompt(html);
        String response = infer(prompt, LLM_MAX_TOKENS);

        // Parse LLM response into structured analysis
        LLMAnalysis analysis;
        analysis.rawAnalysis = response;
        analysis.success = response.length() > 0;

        // TODO: Parse structured data from LLM response
        // For now, combine with pattern matching
        LLMAnalysis patternAnalysis = patternBasedAnalysis(html);
        analysis.venueName = patternAnalysis.venueName;
        analysis.venueType = patternAnalysis.venueType;
        analysis.formFields = patternAnalysis.formFields;
        analysis.securityIssues = patternAnalysis.securityIssues;

        return analysis;
    }

    // Fallback to pattern-based analysis
    return patternBasedAnalysis(html);
}

String LLMEngine::generateEnumStrategy(const String& html, const std::vector<String>& fieldNames) {
    if (modelLoaded) {
        String prompt = buildEnumPrompt(html, fieldNames);
        return infer(prompt, 128);
    }

    // Pattern-based strategy suggestion
    String strategy = "Recommended enumeration strategy:\n";

    bool hasRoom = false;
    bool hasName = false;

    for (const String& field : fieldNames) {
        String lower = field;
        lower.toLowerCase();
        if (lower.indexOf("room") >= 0) hasRoom = true;
        if (lower.indexOf("name") >= 0 || lower.indexOf("last") >= 0) hasName = true;
    }

    if (hasRoom && hasName) {
        strategy += "1. Start with common surnames (Smith, Johnson, etc.)\n";
        strategy += "2. Try room numbers 101-120, 201-220, etc.\n";
        strategy += "3. Look for patterns in successful combinations\n";
    } else if (hasRoom) {
        strategy += "1. Enumerate room numbers systematically\n";
        strategy += "2. Try common patterns (101, 102, 201, 202)\n";
        strategy += "3. Note response differences for valid vs invalid\n";
    } else if (hasName) {
        strategy += "1. Try common surnames from wordlist\n";
        strategy += "2. Note any error messages for clues\n";
        strategy += "3. May indicate number of registered guests\n";
    }

    return strategy;
}

String LLMEngine::interpretResponse(const String& response, const String& context) {
    if (modelLoaded) {
        String prompt = "Given the context: " + context +
                       "\nInterpret this response: " + response +
                       "\nWhat does this tell us?";
        return infer(prompt, 128);
    }

    // Simple interpretation
    String lower = response;
    lower.toLowerCase();

    if (lower.indexOf("success") >= 0 || lower.indexOf("welcome") >= 0) {
        return "SUCCESS: Credentials appear valid";
    }
    if (lower.indexOf("invalid") >= 0 || lower.indexOf("incorrect") >= 0) {
        return "FAILURE: Invalid credentials";
    }
    if (lower.indexOf("locked") >= 0 || lower.indexOf("blocked") >= 0) {
        return "WARNING: Account may be locked or IP blocked";
    }
    if (lower.indexOf("rate") >= 0 || lower.indexOf("limit") >= 0) {
        return "WARNING: Rate limiting detected";
    }

    return "UNKNOWN: Response needs manual review";
}

String LLMEngine::infer(const String& prompt, int maxTokens) {
    if (!modelLoaded) {
        return "";
    }

    #if DEBUG_SERIAL
    Serial.printf("[LLM] Inference: %d chars, max %d tokens\n",
        prompt.length(), maxTokens);
    #endif

    // TODO: Implement actual inference with esp32-llm
    // Example integration:
    //
    // #include "llama.h"
    // llama_context* ctx = llama_get_context();
    // llama_eval(ctx, prompt.c_str(), maxTokens);
    // return llama_get_response(ctx);

    // Placeholder response
    return "[LLM inference not implemented - using pattern matching]";
}

String LLMEngine::buildAnalysisPrompt(const String& html) {
    String prompt = R"(Analyze this captive portal HTML and extract:
1. Venue name and type (hotel, airport, cafe, etc.)
2. Required form fields and their purpose
3. Security vulnerabilities
4. Estimated number of rooms/users if detectable

HTML:
)";
    // Truncate HTML to fit in context
    if (html.length() > 2000) {
        prompt += html.substring(0, 2000) + "...[truncated]";
    } else {
        prompt += html;
    }

    prompt += "\n\nAnalysis:";
    return prompt;
}

String LLMEngine::buildEnumPrompt(const String& html, const std::vector<String>& fields) {
    String prompt = "Given a captive portal with these fields: ";
    for (size_t i = 0; i < fields.size(); i++) {
        if (i > 0) prompt += ", ";
        prompt += fields[i];
    }
    prompt += "\n\nSuggest the best enumeration strategy:";
    return prompt;
}

LLMAnalysis LLMEngine::patternBasedAnalysis(const String& html) {
    LLMAnalysis analysis;
    analysis.success = true;

    String lower = html;
    lower.toLowerCase();

    // Detect venue type
    if (lower.indexOf("hotel") >= 0 || lower.indexOf("resort") >= 0 ||
        lower.indexOf("inn") >= 0 || lower.indexOf("suites") >= 0) {
        analysis.venueType = "Hotel/Resort";
    } else if (lower.indexOf("airport") >= 0 || lower.indexOf("terminal") >= 0 ||
               lower.indexOf("airline") >= 0) {
        analysis.venueType = "Airport";
    } else if (lower.indexOf("hospital") >= 0 || lower.indexOf("medical") >= 0 ||
               lower.indexOf("clinic") >= 0) {
        analysis.venueType = "Healthcare Facility";
    } else if (lower.indexOf("cafe") >= 0 || lower.indexOf("coffee") >= 0 ||
               lower.indexOf("restaurant") >= 0) {
        analysis.venueType = "Cafe/Restaurant";
    } else if (lower.indexOf("conference") >= 0 || lower.indexOf("convention") >= 0 ||
               lower.indexOf("event") >= 0) {
        analysis.venueType = "Conference Center";
    } else if (lower.indexOf("university") >= 0 || lower.indexOf("college") >= 0 ||
               lower.indexOf("school") >= 0) {
        analysis.venueType = "Educational Institution";
    } else if (lower.indexOf("library") >= 0) {
        analysis.venueType = "Library";
    } else {
        analysis.venueType = "Unknown Venue Type";
    }

    // Extract venue name from title tag
    int titleStart = html.indexOf("<title>");
    int titleEnd = html.indexOf("</title>");
    if (titleStart >= 0 && titleEnd > titleStart) {
        analysis.venueName = html.substring(titleStart + 7, titleEnd);
        analysis.venueName.trim();
        // Clean up common suffixes
        analysis.venueName.replace(" - WiFi", "");
        analysis.venueName.replace(" Guest WiFi", "");
        analysis.venueName.replace(" - Guest Access", "");
    } else {
        analysis.venueName = "Unknown";
    }

    // Detect form fields
    if (lower.indexOf("room") >= 0) {
        analysis.formFields.push_back("Room Number");
    }
    if (lower.indexOf("last") >= 0 && lower.indexOf("name") >= 0) {
        analysis.formFields.push_back("Last Name");
    }
    if (lower.indexOf("first") >= 0 && lower.indexOf("name") >= 0) {
        analysis.formFields.push_back("First Name");
    }
    if (lower.indexOf("email") >= 0) {
        analysis.formFields.push_back("Email Address");
    }
    if (lower.indexOf("phone") >= 0 || lower.indexOf("mobile") >= 0) {
        analysis.formFields.push_back("Phone Number");
    }
    if (lower.indexOf("code") >= 0 || lower.indexOf("access") >= 0) {
        analysis.formFields.push_back("Access Code");
    }

    // Detect security issues
    if (lower.indexOf("http://") >= 0 && lower.indexOf("https://") < 0) {
        analysis.securityIssues.push_back("Form submits over HTTP (unencrypted)");
    }
    if (lower.indexOf("password") >= 0 && lower.indexOf("type=\"password\"") < 0) {
        analysis.securityIssues.push_back("Password field may not be masked");
    }
    if (analysis.formFields.size() == 1) {
        analysis.securityIssues.push_back("Single-factor authentication (weak)");
    }
    if (lower.indexOf("remember") >= 0 || lower.indexOf("stay logged") >= 0) {
        analysis.securityIssues.push_back("Session persistence may leak credentials");
    }

    // Generate recommendations
    if (analysis.venueType == "Hotel/Resort") {
        analysis.recommendations.push_back("Try room number enumeration (101-999)");
        analysis.recommendations.push_back("Common surnames likely to yield results");
    }
    if (analysis.formFields.size() <= 2) {
        analysis.recommendations.push_back("Low complexity - automated enumeration recommended");
    }

    // Build raw analysis text
    analysis.rawAnalysis = "Venue: " + analysis.venueName + " (" + analysis.venueType + ")\n";
    analysis.rawAnalysis += "Fields: ";
    for (size_t i = 0; i < analysis.formFields.size(); i++) {
        if (i > 0) analysis.rawAnalysis += ", ";
        analysis.rawAnalysis += analysis.formFields[i];
    }
    analysis.rawAnalysis += "\nSecurity Issues: " + String(analysis.securityIssues.size());
    for (const auto& issue : analysis.securityIssues) {
        analysis.rawAnalysis += "\n - " + issue;
    }

    return analysis;
}
