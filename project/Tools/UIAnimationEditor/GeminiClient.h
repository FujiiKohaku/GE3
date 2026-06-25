#pragma once

#include "IAIClient.h"
#include <string>
#include <vector>

struct GeminiSettings {
    std::string apiKey;
    std::string model;
};

class GeminiClient : public IAIClient {
public:
    const char* GetProviderName() const override;
    AIAnimationResponse GenerateAnimationCommands(const AIAnimationRequest& request, IAIClientProgressListener* progressListener) override;

private:
    bool LoadSettings(GeminiSettings* settings, std::string* errorMessage) const;
    std::vector<std::string> GetSettingsPaths() const;
    std::string BuildPrompt(const AIAnimationRequest& request) const;
    bool PostGenerateContent(const GeminiSettings& settings, const std::string& prompt, std::string* responseBody, std::string* errorMessage, IAIClientProgressListener* progressListener) const;
    bool SendGenerateContentRequest(const GeminiSettings& settings, const std::string& requestBody, std::string* responseBody, int* statusCode, std::string* errorMessage) const;
    bool ShouldRetryStatus(int statusCode) const;
    int GetRetryWaitSeconds(int retryAttempt, const std::string& responseBody) const;
    bool TryExtractRetryDelaySeconds(const std::string& responseBody, int* retryDelaySeconds) const;
    bool TryParseRetryDelayText(const std::string& retryDelayText, int* retryDelaySeconds) const;
    void WaitBeforeRetry(int retryAttempt, int maxRetryCount, int waitSeconds, int httpStatus, const std::string& modelName, IAIClientProgressListener* progressListener) const;
    bool ExtractTextFromResponse(const std::string& responseBody, std::string* commandText, std::string* errorMessage) const;
};
