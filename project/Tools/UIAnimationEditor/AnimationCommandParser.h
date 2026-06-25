#pragma once

#include "AnimationData.h"
#include <string>
#include <vector>

enum class AnimationCommandType {
    Move,
    Fade,
    Scale,
    Rotate,
    Color
};

struct AnimationCommandKey {
    float timeSeconds = 0.0f;
    std::vector<float> values;
    UIEditorInterpolation interpolation = UIEditorInterpolation::Linear;
};

struct AnimationCommand {
    AnimationCommandType type = AnimationCommandType::Move;
    bool hasStart = false;
    bool hasEnd = false;
    bool usesDegrees = true;
    float atSeconds = 0.0f;
    float durationSeconds = 1.0f;
    UIEditorInterpolation interpolation = UIEditorInterpolation::EaseOut;
    std::vector<float> startValues;
    std::vector<float> endValues;
    std::vector<AnimationCommandKey> keys;
};

class AnimationCommandParser {
public:
    bool Parse(const std::string& commandText, std::vector<AnimationCommand>* commands, std::string* errorMessage) const;

private:
    bool IsCommandName(const std::string& line, AnimationCommandType* type) const;
    bool ParsePropertyLine(const std::string& line, AnimationCommand* command, std::string* errorMessage) const;
    bool ParseFloatList(const std::string& text, std::vector<float>* values) const;
    bool ParseInterpolation(const std::string& text, UIEditorInterpolation* interpolation) const;
    std::string Trim(const std::string& text) const;
    std::string ToLower(const std::string& text) const;
    std::string RemoveCommandNoise(const std::string& text) const;
};
