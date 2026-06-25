#pragma once

#include <string>

struct AIAnimationRequest {
    std::string prompt;
    std::string animationName;
    int canvasWidth = 1280;
    int canvasHeight = 720;
    int currentLength = 60;
};

struct AIAnimationResponse {
    bool success = false;
    std::string commandText;
    std::string errorMessage;
};

struct AIRetryStatus {
    int retryAttempt = 0;
    int maxRetryCount = 0;
    int waitSeconds = 0;
    int httpStatus = 0;
    std::string modelName;
    std::string message;
};

class IAIClientProgressListener {
public:
    virtual ~IAIClientProgressListener() = default;

    virtual void OnAIRetryWait(const AIRetryStatus& status) = 0;
};

class IAIClient {
public:
    virtual ~IAIClient() = default;

    virtual const char* GetProviderName() const = 0;
    virtual AIAnimationResponse GenerateAnimationCommands(const AIAnimationRequest& request, IAIClientProgressListener* progressListener) = 0;
};
