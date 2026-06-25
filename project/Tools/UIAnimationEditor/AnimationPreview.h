#pragma once

#include "AnimationData.h"
#include <string>

class AnimationPreview {
public:
    bool Apply(const UIEditorAnimationClip& generatedClip, UIEditorAnimationClip* targetClip, std::string* errorMessage) const;
};
