#pragma once

#include "externals/json.hpp"
#include <string>

// Properties match the JSON names used by the runtime player.
enum class UIAnimationProperty {
    PositionX,
    PositionY,
    ScaleX,
    ScaleY,
    Rotation,
    ColorR,
    ColorG,
    ColorB,
    ColorA
};

// Stored per key so curve types can be extended without changing track data.
enum class UIAnimationInterpolation {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

const char* UIAnimationPropertyToString(UIAnimationProperty property);
bool UIAnimationPropertyFromString(const std::string& text, UIAnimationProperty* property);
int UIAnimationPropertyToIndex(UIAnimationProperty property);
UIAnimationProperty UIAnimationPropertyFromIndex(int index);
int UIAnimationPropertyCount();

const char* UIAnimationInterpolationToString(UIAnimationInterpolation interpolation);
bool UIAnimationInterpolationFromString(const std::string& text, UIAnimationInterpolation* interpolation);
int UIAnimationInterpolationToIndex(UIAnimationInterpolation interpolation);
UIAnimationInterpolation UIAnimationInterpolationFromIndex(int index);
int UIAnimationInterpolationCount();

class UIKeyFrame {
public:
    UIKeyFrame();
    UIKeyFrame(int frame, float value);

    int GetFrame() const;
    void SetFrame(int frame);

    float GetValue() const;
    void SetValue(float value);

    UIAnimationInterpolation GetInterpolation() const;
    void SetInterpolation(UIAnimationInterpolation interpolation);

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& keyFrameJson);

private:
    int frame_ = 0;
    float value_ = 0.0f;
    UIAnimationInterpolation interpolation_ = UIAnimationInterpolation::Linear;
};
