#include "AnimationEventLoader.h"

#include "Engine/Logger/Logger.h"
#include "externals/json.hpp"

#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <cstdint>
#include <exception>
#include <fstream>

namespace {
bool CompareAnimationEventTime(
    const AnimationEvent& left,
    const AnimationEvent& right)
{
    return left.time < right.time;
}
}

void AnimationEventLoader::EnsureEventFilesForModel(
    const std::filesystem::path& modelPath)
{
    const std::string extension = modelPath.extension().string();
    if (extension != ".gltf" && extension != ".glb") {
        return;
    }
    if (!std::filesystem::is_regular_file(modelPath)) {
        return;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelPath.string(), 0);
    if (!scene || scene->mNumAnimations == 0) {
        return;
    }

    const std::filesystem::path eventDirectory =
        modelPath.parent_path() / "AnimationEvents";
    std::error_code directoryError;
    std::filesystem::create_directories(eventDirectory, directoryError);
    if (directoryError) {
        Logger::Warning(
            "Animation Event directory creation failed. Path:" +
            eventDirectory.generic_string());
        return;
    }

    for (uint32_t animationIndex = 0;
        animationIndex < scene->mNumAnimations;
        ++animationIndex) {
        const aiAnimation* animation = scene->mAnimations[animationIndex];
        std::string animationName = animation->mName.C_Str();
        if (animationName.empty()) {
            animationName = "Animation_" + std::to_string(animationIndex);
        }

        const std::filesystem::path eventFilePath =
            BuildEventFilePath(modelPath, animationName);
        if (std::filesystem::is_regular_file(eventFilePath)) {
            continue;
        }

        double ticksPerSecond = animation->mTicksPerSecond;
        if (ticksPerSecond <= 0.0) {
            ticksPerSecond = 25.0;
        }
        const float duration =
            static_cast<float>(animation->mDuration / ticksPerSecond);

        nlohmann::json eventJson;
        eventJson["Animation"] = animationName;
        eventJson["Duration"] = duration;
        eventJson["Events"] = nlohmann::json::array();

        std::ofstream outputFile(eventFilePath);
        if (!outputFile.is_open()) {
            Logger::Warning(
                "Animation Event file creation failed. Path:" +
                eventFilePath.generic_string());
            continue;
        }
        outputFile << eventJson.dump(2) << '\n';
        Logger::Log(
            "Animation Event template created. Path:" +
            eventFilePath.generic_string());
    }
}

std::vector<AnimationEvent> AnimationEventLoader::LoadEvents(
    const std::filesystem::path& modelPath,
    const std::string& animationName,
    float animationDuration)
{
    std::vector<AnimationEvent> events;
    const std::filesystem::path eventFilePath =
        BuildEventFilePath(modelPath, animationName);
    if (!std::filesystem::is_regular_file(eventFilePath)) {
        return events;
    }

    std::ifstream inputFile(eventFilePath);
    if (!inputFile.is_open()) {
        return events;
    }

    try {
        nlohmann::json eventJson;
        inputFile >> eventJson;
        if (!eventJson.contains("Events") ||
            !eventJson.at("Events").is_array()) {
            return events;
        }

        const nlohmann::json& eventArray = eventJson.at("Events");
        for (size_t eventIndex = 0;
            eventIndex < eventArray.size();
            ++eventIndex) {
            const nlohmann::json& eventValue = eventArray.at(eventIndex);
            if (!eventValue.is_object() || !eventValue.contains("Name")) {
                continue;
            }

            AnimationEvent event;
            event.time = eventValue.value("Time", 0.0f);
            event.name = eventValue.at("Name").get<std::string>();
            event.value = eventValue.value("Value", std::string());
            event.bone = eventValue.value("Bone", std::string());

            if (event.name.empty() || event.time < 0.0f) {
                continue;
            }
            if (animationDuration > 0.0f &&
                event.time > animationDuration) {
                event.time = animationDuration;
            }
            events.push_back(event);
        }
    } catch (const std::exception& exception) {
        Logger::Warning(
            "Animation Event JSON load failed. Path:" +
            eventFilePath.generic_string() +
            " Reason:" + exception.what());
        events.clear();
    }

    std::stable_sort(
        events.begin(),
        events.end(),
        CompareAnimationEventTime);
    return events;
}

std::filesystem::path AnimationEventLoader::BuildEventFilePath(
    const std::filesystem::path& modelPath,
    const std::string& animationName)
{
    return modelPath.parent_path() /
        "AnimationEvents" /
        (MakeSafeFileName(animationName) + ".json");
}

std::string AnimationEventLoader::MakeSafeFileName(
    const std::string& animationName)
{
    std::string safeName = animationName;
    const std::string invalidCharacters = "<>:\"/\\|?*";
    for (char& character : safeName) {
        if (invalidCharacters.find(character) != std::string::npos) {
            character = '_';
        }
    }
    if (safeName.empty()) {
        safeName = "Animation";
    }
    return safeName;
}
