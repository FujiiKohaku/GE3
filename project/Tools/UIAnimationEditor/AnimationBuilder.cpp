#include "AnimationBuilder.h"
#include <cmath>
#include <sstream>

namespace {
constexpr float kPi = 3.1415926535f;
}

bool AnimationBuilder::Build(const std::vector<AnimationCommand>& commands, const UIEditorAnimationClip& sourceClip, UIEditorAnimationClip* outputClip, std::string* errorMessage) const
{
    if (outputClip == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "AnimationBuilder output clip is null.";
        }
        return false;
    }

    if (commands.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "AnimationBuilder received no commands.";
        }
        return false;
    }

    UIEditorAnimationClip generatedClip = sourceClip;
    generatedClip.ClearKeys();

    int maxFrame = 1;
    UIEditorObjectState baseState = generatedClip.GetBaseState();

    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex) {
        if (!AddCommandKeys(commands[commandIndex], &generatedClip, baseState, &maxFrame, errorMessage)) {
            std::ostringstream message;
            message << "AnimationBuilder failed at command " << commandIndex + 1;

            if (errorMessage != nullptr && !errorMessage->empty()) {
                message << ": " << *errorMessage;
            }

            if (errorMessage != nullptr) {
                *errorMessage = message.str();
            }
            return false;
        }
    }

    if (maxFrame > generatedClip.GetLength()) {
        generatedClip.SetLength(maxFrame);
    }

    *outputClip = generatedClip;
    return true;
}

bool AnimationBuilder::AddCommandKeys(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const
{
    if (!command.keys.empty()) {
        return AddKeyCommand(command, clip, baseState, maxFrame, errorMessage);
    }

    return AddStartEndCommand(command, clip, baseState, maxFrame, errorMessage);
}

bool AnimationBuilder::AddStartEndCommand(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const
{
    if (!command.hasStart) {
        if (errorMessage != nullptr) {
            *errorMessage = "Command is missing Start(...).";
        }
        return false;
    }
    if (!command.hasEnd) {
        if (errorMessage != nullptr) {
            *errorMessage = "Command is missing End(...).";
        }
        return false;
    }

    int startFrame = SecondsToFrame(command.atSeconds);
    int endFrame = SecondsToFrame(command.atSeconds + command.durationSeconds);

    if (endFrame <= startFrame) {
        endFrame = startFrame + 1;
    }

    if (!AddValues(command.type, clip, baseState, startFrame, command.startValues, command.interpolation, command.usesDegrees, errorMessage)) {
        return false;
    }

    if (!AddValues(command.type, clip, baseState, endFrame, command.endValues, UIEditorInterpolation::Linear, command.usesDegrees, errorMessage)) {
        return false;
    }

    if (maxFrame != nullptr && endFrame > *maxFrame) {
        *maxFrame = endFrame;
    }

    return true;
}

bool AnimationBuilder::AddKeyCommand(const AnimationCommand& command, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int* maxFrame, std::string* errorMessage) const
{
    for (std::size_t keyIndex = 0; keyIndex < command.keys.size(); ++keyIndex) {
        const AnimationCommandKey& key = command.keys[keyIndex];
        int frame = SecondsToFrame(command.atSeconds + key.timeSeconds);

        if (!AddValues(command.type, clip, baseState, frame, key.values, key.interpolation, command.usesDegrees, errorMessage)) {
            return false;
        }

        if (maxFrame != nullptr && frame > *maxFrame) {
            *maxFrame = frame;
        }
    }

    return true;
}

bool AnimationBuilder::AddValues(AnimationCommandType type, UIEditorAnimationClip* clip, const UIEditorObjectState& baseState, int frame, const std::vector<float>& values, UIEditorInterpolation interpolation, bool usesDegrees, std::string* errorMessage) const
{
    if (clip == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "target clip is null.";
        }
        return false;
    }

    if (type == AnimationCommandType::Move) {
        if (values.size() < 2) {
            if (errorMessage != nullptr) {
                *errorMessage = "Move requires two values: x, y.";
            }
            return false;
        }

        clip->AddKey(UIEditorProperty::PositionX, frame, baseState.position.x + values[0], interpolation);
        clip->AddKey(UIEditorProperty::PositionY, frame, baseState.position.y + values[1], interpolation);
        return true;
    }

    if (type == AnimationCommandType::Fade) {
        if (values.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Fade requires one alpha value.";
            }
            return false;
        }

        clip->AddKey(UIEditorProperty::ColorA, frame, values[0], interpolation);
        return true;
    }

    if (type == AnimationCommandType::Scale) {
        if (values.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Scale requires one or two values.";
            }
            return false;
        }

        float scaleX = values[0];
        float scaleY = values[0];

        if (values.size() >= 2) {
            scaleY = values[1];
        }

        clip->AddKey(UIEditorProperty::ScaleX, frame, baseState.scale.x * scaleX, interpolation);
        clip->AddKey(UIEditorProperty::ScaleY, frame, baseState.scale.y * scaleY, interpolation);
        return true;
    }

    if (type == AnimationCommandType::Rotate) {
        if (values.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Rotate requires one value.";
            }
            return false;
        }

        float rotationValue = values[0];

        if (usesDegrees) {
            rotationValue = DegreesToRadians(rotationValue);
        }

        clip->AddKey(UIEditorProperty::Rotation, frame, baseState.rotation + rotationValue, interpolation);
        return true;
    }

    if (type == AnimationCommandType::Color) {
        if (values.size() < 3) {
            if (errorMessage != nullptr) {
                *errorMessage = "Color requires at least r, g, b.";
            }
            return false;
        }

        float alpha = baseState.color.a;

        if (values.size() >= 4) {
            alpha = values[3];
        }

        clip->AddKey(UIEditorProperty::ColorR, frame, values[0], interpolation);
        clip->AddKey(UIEditorProperty::ColorG, frame, values[1], interpolation);
        clip->AddKey(UIEditorProperty::ColorB, frame, values[2], interpolation);
        clip->AddKey(UIEditorProperty::ColorA, frame, alpha, interpolation);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unknown animation command type.";
    }

    return false;
}

int AnimationBuilder::SecondsToFrame(float seconds) const
{
    if (seconds < 0.0f) {
        seconds = 0.0f;
    }

    return static_cast<int>(seconds * 60.0f + 0.5f);
}

float AnimationBuilder::DegreesToRadians(float degrees) const
{
    return degrees * kPi / 180.0f;
}
