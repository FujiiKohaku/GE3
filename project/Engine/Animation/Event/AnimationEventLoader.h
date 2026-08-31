#pragma once

#include "AnimationEvent.h"

#include <filesystem>
#include <string>
#include <vector>

class AnimationEventLoader {
public:
    static void EnsureEventFilesForModel(const std::filesystem::path& modelPath);
    static std::vector<AnimationEvent> LoadEvents(
        const std::filesystem::path& modelPath,
        const std::string& animationName,
        float animationDuration);

private:
    static std::filesystem::path BuildEventFilePath(
        const std::filesystem::path& modelPath,
        const std::string& animationName);
    static std::string MakeSafeFileName(const std::string& animationName);
};
