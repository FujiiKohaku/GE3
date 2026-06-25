#include "GeminiClient.h"
#include "externals/json.hpp"
#include <Windows.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace {
std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return L"";
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);

    if (wideLength <= 0) {
        return L"";
    }

    std::wstring wideText;
    wideText.resize(static_cast<std::size_t>(wideLength));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wideText.data(), wideLength);

    return wideText;
}

std::string GetLastWinHttpErrorText(const std::string& prefix)
{
    DWORD errorCode = GetLastError();
    std::ostringstream stream;
    stream << prefix << " WinHTTP error: " << errorCode;
    return stream.str();
}

void CloseWinHttpHandle(HINTERNET handle)
{
    if (handle != nullptr) {
        WinHttpCloseHandle(handle);
    }
}

bool QueryStatusCode(HINTERNET requestHandle, DWORD* statusCode)
{
    if (statusCode == nullptr) {
        return false;
    }

    DWORD statusCodeSize = sizeof(DWORD);
    DWORD statusCodeIndex = WINHTTP_NO_HEADER_INDEX;
    BOOL result = WinHttpQueryHeaders(
        requestHandle,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        statusCode,
        &statusCodeSize,
        &statusCodeIndex);

    if (!result) {
        return false;
    }

    return true;
}

std::string ExtractApiErrorMessage(const std::string& responseBody)
{
    if (responseBody.empty()) {
        return "";
    }

    try {
        nlohmann::json responseJson = nlohmann::json::parse(responseBody);

        if (responseJson.contains("error")) {
            const nlohmann::json& errorJson = responseJson["error"];

            if (errorJson.contains("message")) {
                return errorJson["message"].get<std::string>();
            }
        }
    } catch (const std::exception&) {
    }

    return "";
}

void LogGeminiError(const std::string& message)
{
    std::string line = message + "\n";
    OutputDebugStringA(line.c_str());
}

std::string BuildHttpStatusError(DWORD statusCode, const std::string& responseBody, const std::string& modelName)
{
    std::ostringstream stream;
    stream << "Gemini API request failed.";
    stream << " Model: " << modelName;
    stream << " HTTP status: " << statusCode;

    std::string apiErrorMessage = ExtractApiErrorMessage(responseBody);

    if (!apiErrorMessage.empty()) {
        stream << " Error message: " << apiErrorMessage;
    }

    if (!responseBody.empty()) {
        stream << " Response body: " << responseBody;
    }

    return stream.str();
}

bool FindRetryDelayText(const nlohmann::json& jsonValue, std::string* retryDelayText)
{
    if (retryDelayText == nullptr) {
        return false;
    }

    if (jsonValue.is_object()) {
        for (nlohmann::json::const_iterator iterator = jsonValue.begin(); iterator != jsonValue.end(); ++iterator) {
            if (iterator.key() == "retryDelay" && iterator.value().is_string()) {
                *retryDelayText = iterator.value().get<std::string>();
                return true;
            }

            if (FindRetryDelayText(iterator.value(), retryDelayText)) {
                return true;
            }
        }
    }

    if (jsonValue.is_array()) {
        for (std::size_t index = 0; index < jsonValue.size(); ++index) {
            if (FindRetryDelayText(jsonValue[index], retryDelayText)) {
                return true;
            }
        }
    }

    return false;
}
}

const char* GeminiClient::GetProviderName() const
{
    return "Gemini";
}

AIAnimationResponse GeminiClient::GenerateAnimationCommands(const AIAnimationRequest& request, IAIClientProgressListener* progressListener)
{
    AIAnimationResponse response;

    GeminiSettings settings;
    std::string errorMessage;

    if (!LoadSettings(&settings, &errorMessage)) {
        response.errorMessage = errorMessage;
        return response;
    }

    std::string responseBody;
    std::string prompt = BuildPrompt(request);

    if (!PostGenerateContent(settings, prompt, &responseBody, &errorMessage, progressListener)) {
        response.errorMessage = errorMessage;
        return response;
    }

    if (!ExtractTextFromResponse(responseBody, &response.commandText, &errorMessage)) {
        response.errorMessage = errorMessage;
        return response;
    }

    response.success = true;
    return response;
}

bool GeminiClient::LoadSettings(GeminiSettings* settings, std::string* errorMessage) const
{
    if (settings == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini settings output is null.";
        }
        return false;
    }

    std::vector<std::string> settingsPaths = GetSettingsPaths();

    for (std::size_t pathIndex = 0; pathIndex < settingsPaths.size(); ++pathIndex) {
        const std::string& path = settingsPaths[pathIndex];

        if (!std::filesystem::exists(path)) {
            continue;
        }

        std::ifstream file(path);

        if (!file.is_open()) {
            if (errorMessage != nullptr) {
                *errorMessage = "AI settings file exists but could not be opened: " + path;
            }
            return false;
        }

        nlohmann::json settingsJson;

        try {
            file >> settingsJson;
        } catch (const std::exception& exception) {
            if (errorMessage != nullptr) {
                *errorMessage = std::string("AI settings JSON parse failed: ") + exception.what();
            }
            return false;
        }

        if (settingsJson.contains("gemini")) {
            const nlohmann::json& geminiJson = settingsJson["gemini"];

            if (geminiJson.contains("apiKey")) {
                settings->apiKey = geminiJson["apiKey"].get<std::string>();
            }

            if (geminiJson.contains("model")) {
                settings->model = geminiJson["model"].get<std::string>();
            }
        }

        if (settingsJson.contains("apiKey") && settings->apiKey.empty()) {
            settings->apiKey = settingsJson["apiKey"].get<std::string>();
        }

        if (settingsJson.contains("model")) {
            settings->model = settingsJson["model"].get<std::string>();
        }

        if (settings->apiKey.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Gemini API key is not set in: " + path;
            }
            return false;
        }

        if (settings->model.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Gemini model is not set in: " + path;
            }
            return false;
        }

        return true;
    }

    if (errorMessage != nullptr) {
        std::string message = "AI settings file was not found. Searched:";

        for (std::size_t pathIndex = 0; pathIndex < settingsPaths.size(); ++pathIndex) {
            message += " ";
            message += settingsPaths[pathIndex];
        }

        *errorMessage = message;
    }

    return false;
}

std::vector<std::string> GeminiClient::GetSettingsPaths() const
{
    std::vector<std::string> paths;
    paths.push_back("ai_settings.json");
    paths.push_back("Tools/UIAnimationEditor/ai_settings.json");
    paths.push_back("project/Tools/UIAnimationEditor/ai_settings.json");
    paths.push_back("../../ai_settings.json");
    paths.push_back("../../../project/Tools/UIAnimationEditor/ai_settings.json");
    return paths;
}

std::string GeminiClient::BuildPrompt(const AIAnimationRequest& request) const
{
    std::ostringstream prompt;
    prompt << "You are an animation command generator for a C++ UI animation editor.\n";
    prompt << "Return only the custom command format. Do not return JSON. Do not use Markdown fences.\n";
    prompt << "The user writes Japanese natural language. Convert it into readable animation commands.\n";
    prompt << "Coordinate rule: Move values are offsets from the current base position. Positive X is right, positive Y is down.\n";
    prompt << "Scale values are multipliers of the current base size. Fade values are alpha 0.0 to 1.0. Rotate values are degrees.\n";
    prompt << "Supported commands: Move, Fade, Scale, Rotate, Color.\n";
    prompt << "Supported properties: At(seconds), Start(...), End(...), Duration(seconds), Ease(Linear|InQuad|OutQuad|InOutQuad), Unit(Degrees|Radians), Key(seconds,...).\n";
    prompt << "Use multiple commands for combined animation. Use Key lines when an effect needs bounces or multiple stops.\n";
    prompt << "Keep the total duration practical for UI animation, usually 0.3 to 3.0 seconds.\n";
    prompt << "Example:\n";
    prompt << "Move\n";
    prompt << "Start(-400,0)\n";
    prompt << "End(0,0)\n";
    prompt << "Duration(1.0)\n";
    prompt << "Ease(OutQuad)\n\n";
    prompt << "Fade\n";
    prompt << "Start(0)\n";
    prompt << "End(1)\n";
    prompt << "Duration(1.0)\n";
    prompt << "Ease(OutQuad)\n\n";
    prompt << "Current animation name: " << request.animationName << "\n";
    prompt << "Canvas: " << request.canvasWidth << " x " << request.canvasHeight << "\n";
    prompt << "Current length frames: " << request.currentLength << "\n";
    prompt << "User instruction:\n";
    prompt << request.prompt << "\n";

    return prompt.str();
}

bool GeminiClient::PostGenerateContent(const GeminiSettings& settings, const std::string& prompt, std::string* responseBody, std::string* errorMessage, IAIClientProgressListener* progressListener) const
{
    if (responseBody == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response output is null.";
        }
        return false;
    }

    nlohmann::json requestJson;
    requestJson["contents"] = nlohmann::json::array();
    requestJson["contents"][0]["parts"] = nlohmann::json::array();
    requestJson["contents"][0]["parts"][0]["text"] = prompt;
    requestJson["generationConfig"]["temperature"] = 0.45f;
    requestJson["generationConfig"]["maxOutputTokens"] = 1200;

    std::string requestBody = requestJson.dump();
    const int maxRetryCount = 3;

    for (int attemptIndex = 0; attemptIndex <= maxRetryCount; ++attemptIndex) {
        int statusCode = 0;
        std::string attemptErrorMessage;
        responseBody->clear();

        if (!SendGenerateContentRequest(settings, requestBody, responseBody, &statusCode, &attemptErrorMessage)) {
            if (errorMessage != nullptr) {
                *errorMessage = attemptErrorMessage;
            }
            LogGeminiError(attemptErrorMessage);
            return false;
        }

        if (statusCode >= 200 && statusCode < 300) {
            return true;
        }

        std::string httpError = BuildHttpStatusError(static_cast<DWORD>(statusCode), *responseBody, settings.model);

        if (ShouldRetryStatus(statusCode) && attemptIndex < maxRetryCount) {
            int retryAttempt = attemptIndex + 1;
            int waitSeconds = GetRetryWaitSeconds(retryAttempt, *responseBody);
            LogGeminiError(httpError);
            WaitBeforeRetry(retryAttempt, maxRetryCount, waitSeconds, statusCode, settings.model, progressListener);
            continue;
        }

        if (errorMessage != nullptr) {
            *errorMessage = httpError;
        }
        LogGeminiError(httpError);
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Gemini API request failed after retries. Model: " + settings.model;
    }

    return false;
}

bool GeminiClient::SendGenerateContentRequest(const GeminiSettings& settings, const std::string& requestBody, std::string* responseBody, int* statusCode, std::string* errorMessage) const
{
    if (responseBody == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response output is null.";
        }
        return false;
    }

    if (statusCode != nullptr) {
        *statusCode = 0;
    }

    std::wstring requestPath = L"/v1beta/models/";
    requestPath += Utf8ToWide(settings.model);
    requestPath += L":generateContent?key=";
    requestPath += Utf8ToWide(settings.apiKey);

    HINTERNET sessionHandle = WinHttpOpen(
        L"KohakuEngine UIAnimationEditor/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (sessionHandle == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpOpen failed.") + " Model: " + settings.model;
        }
        return false;
    }

    HINTERNET connectHandle = WinHttpConnect(sessionHandle, L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (connectHandle == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpConnect failed.") + " Model: " + settings.model;
        }
        CloseWinHttpHandle(sessionHandle);
        return false;
    }

    HINTERNET requestHandle = WinHttpOpenRequest(
        connectHandle,
        L"POST",
        requestPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (requestHandle == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpOpenRequest failed.") + " Model: " + settings.model;
        }
        CloseWinHttpHandle(connectHandle);
        CloseWinHttpHandle(sessionHandle);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";
    BOOL sendResult = WinHttpSendRequest(
        requestHandle,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        const_cast<char*>(requestBody.data()),
        static_cast<DWORD>(requestBody.size()),
        static_cast<DWORD>(requestBody.size()),
        0);

    if (!sendResult) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpSendRequest failed.") + " Model: " + settings.model;
        }
        CloseWinHttpHandle(requestHandle);
        CloseWinHttpHandle(connectHandle);
        CloseWinHttpHandle(sessionHandle);
        return false;
    }

    BOOL receiveResult = WinHttpReceiveResponse(requestHandle, nullptr);

    if (!receiveResult) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpReceiveResponse failed.") + " Model: " + settings.model;
        }
        CloseWinHttpHandle(requestHandle);
        CloseWinHttpHandle(connectHandle);
        CloseWinHttpHandle(sessionHandle);
        return false;
    }

    responseBody->clear();
    DWORD availableSize = 0;

    do {
        availableSize = 0;

        if (!WinHttpQueryDataAvailable(requestHandle, &availableSize)) {
            if (errorMessage != nullptr) {
                *errorMessage = GetLastWinHttpErrorText("WinHttpQueryDataAvailable failed.") + " Model: " + settings.model;
            }
            CloseWinHttpHandle(requestHandle);
            CloseWinHttpHandle(connectHandle);
            CloseWinHttpHandle(sessionHandle);
            return false;
        }

        if (availableSize == 0) {
            break;
        }

        std::string buffer;
        buffer.resize(static_cast<std::size_t>(availableSize));
        DWORD readSize = 0;

        if (!WinHttpReadData(requestHandle, buffer.data(), availableSize, &readSize)) {
            if (errorMessage != nullptr) {
                *errorMessage = GetLastWinHttpErrorText("WinHttpReadData failed.") + " Model: " + settings.model;
            }
            CloseWinHttpHandle(requestHandle);
            CloseWinHttpHandle(connectHandle);
            CloseWinHttpHandle(sessionHandle);
            return false;
        }

        buffer.resize(static_cast<std::size_t>(readSize));
        *responseBody += buffer;
    } while (availableSize > 0);

    DWORD statusCodeValue = 0;

    if (!QueryStatusCode(requestHandle, &statusCodeValue)) {
        if (errorMessage != nullptr) {
            *errorMessage = GetLastWinHttpErrorText("WinHttpQueryHeaders failed.") + " Model: " + settings.model;
        }
        CloseWinHttpHandle(requestHandle);
        CloseWinHttpHandle(connectHandle);
        CloseWinHttpHandle(sessionHandle);
        return false;
    }

    if (statusCode != nullptr) {
        *statusCode = static_cast<int>(statusCodeValue);
    }

    CloseWinHttpHandle(requestHandle);
    CloseWinHttpHandle(connectHandle);
    CloseWinHttpHandle(sessionHandle);

    return true;
}

bool GeminiClient::ShouldRetryStatus(int statusCode) const
{
    if (statusCode == 429) {
        return true;
    }

    if (statusCode == 503) {
        return true;
    }

    return false;
}

int GeminiClient::GetRetryWaitSeconds(int retryAttempt, const std::string& responseBody) const
{
    int retryDelaySeconds = 0;

    if (TryExtractRetryDelaySeconds(responseBody, &retryDelaySeconds)) {
        if (retryDelaySeconds > 0) {
            return retryDelaySeconds;
        }
    }

    if (retryAttempt <= 1) {
        return 2;
    }

    if (retryAttempt == 2) {
        return 4;
    }

    return 8;
}

bool GeminiClient::TryExtractRetryDelaySeconds(const std::string& responseBody, int* retryDelaySeconds) const
{
    if (retryDelaySeconds == nullptr) {
        return false;
    }

    *retryDelaySeconds = 0;

    if (responseBody.empty()) {
        return false;
    }

    nlohmann::json responseJson;

    try {
        responseJson = nlohmann::json::parse(responseBody);
    } catch (const std::exception&) {
        return false;
    }

    std::string retryDelayText;

    if (!FindRetryDelayText(responseJson, &retryDelayText)) {
        return false;
    }

    return TryParseRetryDelayText(retryDelayText, retryDelaySeconds);
}

bool GeminiClient::TryParseRetryDelayText(const std::string& retryDelayText, int* retryDelaySeconds) const
{
    if (retryDelaySeconds == nullptr) {
        return false;
    }

    *retryDelaySeconds = 0;

    if (retryDelayText.empty()) {
        return false;
    }

    std::string secondsText = retryDelayText;

    while (!secondsText.empty() && secondsText[secondsText.size() - 1] == ' ') {
        secondsText.pop_back();
    }

    if (!secondsText.empty()) {
        char lastCharacter = secondsText[secondsText.size() - 1];

        if (lastCharacter == 's' || lastCharacter == 'S') {
            secondsText.pop_back();
        }
    }

    char* parseEnd = nullptr;
    double secondsValue = std::strtod(secondsText.c_str(), &parseEnd);

    if (parseEnd == secondsText.c_str()) {
        return false;
    }

    if (secondsValue <= 0.0) {
        return false;
    }

    int roundedSeconds = static_cast<int>(secondsValue);

    if (static_cast<double>(roundedSeconds) < secondsValue) {
        roundedSeconds += 1;
    }

    if (roundedSeconds < 1) {
        roundedSeconds = 1;
    }

    *retryDelaySeconds = roundedSeconds;
    return true;
}

void GeminiClient::WaitBeforeRetry(int retryAttempt, int maxRetryCount, int waitSeconds, int httpStatus, const std::string& modelName, IAIClientProgressListener* progressListener) const
{
    if (waitSeconds < 1) {
        waitSeconds = 1;
    }

    for (int remainingSeconds = waitSeconds; remainingSeconds > 0; --remainingSeconds) {
        if (progressListener != nullptr) {
            AIRetryStatus status;
            status.retryAttempt = retryAttempt;
            status.maxRetryCount = maxRetryCount;
            status.waitSeconds = remainingSeconds;
            status.httpStatus = httpStatus;
            status.modelName = modelName;
            status.message = "Geminiサーバーが混雑しています。\n" + std::to_string(remainingSeconds) + "秒後に再試行しています…";
            progressListener->OnAIRetryWait(status);
        }

        Sleep(1000);
    }
}

bool GeminiClient::ExtractTextFromResponse(const std::string& responseBody, std::string* commandText, std::string* errorMessage) const
{
    if (commandText == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini command text output is null.";
        }
        return false;
    }

    nlohmann::json responseJson;

    try {
        responseJson = nlohmann::json::parse(responseBody);
    } catch (const std::exception& exception) {
        if (errorMessage != nullptr) {
            *errorMessage = std::string("Gemini response JSON parse failed: ") + exception.what();
        }
        return false;
    }

    if (!responseJson.contains("candidates")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response did not contain candidates.";
        }
        return false;
    }

    if (responseJson["candidates"].empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response candidates were empty.";
        }
        return false;
    }

    const nlohmann::json& candidate = responseJson["candidates"][0];

    if (!candidate.contains("content")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response candidate did not contain content.";
        }
        return false;
    }

    const nlohmann::json& content = candidate["content"];

    if (!content.contains("parts")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response content did not contain parts.";
        }
        return false;
    }

    if (content["parts"].empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response parts were empty.";
        }
        return false;
    }

    const nlohmann::json& part = content["parts"][0];

    if (!part.contains("text")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response part did not contain text.";
        }
        return false;
    }

    *commandText = part["text"].get<std::string>();

    if (commandText->empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Gemini response text was empty.";
        }
        return false;
    }

    return true;
}
