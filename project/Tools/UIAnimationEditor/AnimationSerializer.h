#pragma once

#include "AnimationData.h"
#include <string>

class AnimationSerializer {
public:
    bool ValidateProjectJson(const UIEditorAnimationClip& clip, std::string* errorMessage) const;
    bool SaveProjectJson(const UIEditorAnimationClip& clip, const std::string& filePath, std::string* errorMessage) const;
};
