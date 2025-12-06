#ifndef LLM_ENGINE_H
#define LLM_ENGINE_H

#include <Arduino.h>
#include <vector>

// LLM Analysis result
struct LLMAnalysis {
    String venueName;
    String venueType;
    String location;
    int estimatedRooms;
    std::vector<String> formFields;
    std::vector<String> securityIssues;
    std::vector<String> recommendations;
    String rawAnalysis;
    bool success;
};

class LLMEngine {
public:
    static void init();
    static bool isAvailable();
    static bool isReady();

    // Analysis functions
    static LLMAnalysis analyzePortalHTML(const String& html);
    static String generateEnumStrategy(const String& html, const std::vector<String>& fieldNames);
    static String interpretResponse(const String& response, const String& context);

    // Direct inference
    static String infer(const String& prompt, int maxTokens = 256);

    // Model management
    static bool loadModel(const String& modelPath);
    static void unloadModel();
    static size_t getModelSize();
    static size_t getFreeMemory();

private:
    static bool initialized;
    static bool modelLoaded;
    static String currentModel;

    // Prompt templates
    static String buildAnalysisPrompt(const String& html);
    static String buildEnumPrompt(const String& html, const std::vector<String>& fields);

    // Simple pattern-based fallback when LLM unavailable
    static LLMAnalysis patternBasedAnalysis(const String& html);
};

#endif // LLM_ENGINE_H
