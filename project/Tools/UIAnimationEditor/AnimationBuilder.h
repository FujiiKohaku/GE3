#pragma once

#include "AnimationCommandParser.h"
#include "AnimationData.h"
#include <string>
#include <vector>

class AnimationBuilder {
public:
    bool Build(const std::vector<AnimationCommand>& commands, const UIEditorAnimationClip& sourceClip, UIEditorAnimationClip* outputClip, std::string* errorMessage) const;

private:
    bool AddCommandKeys(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const;
    bool AddStartEndCommand(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const;
    bool AddKeyCommand(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const;
    bool AddValues(AnimationCommandType type, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int frame, const std::vector<float>& values, UIEditorInterpolation interpolation, bool usesDegrees, std::string* errorMessage) const;
    int SecondsToFrame(float seconds) const;
    float DegreesToRadians(float degrees) const;
};
