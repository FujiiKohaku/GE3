#include "AnimationSerializer.h"
#include <exception>

bool AnimationSerializer::ValidateProjectJson(const UIEditorAnimationClip& clip, std::string* errorMessage) const
{
    try {
        nlohmann::json clipJson = clip.ToProjectJson();
        std::string jsonText = clipJson.dump(4);

        if (jsonText.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Generated JSON text was empty.";
            }
            return false;
        }
    } catch (const std::exception& exception) {
        if (errorMessage != nullptr) {
            *errorMessage = std::string("Generated JSON validation failed: ") + exception.what();
        }
        return false;
    }

    return true;
}

bool AnimationSerializer::SaveProjectJson(const UIEditorAnimationClip& clip, const std::string& filePath, std::string* errorMessage) const
{
    if (filePath.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "JSON save path is empty.";
        }
        return false;
    }

    if (!clip.SaveProjectFile(filePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = "JSON save failed: " + filePath;
        }
        return false;
    }

    return true;
}
