#pragma once

#include "UIKeyFrame.h"
#include <vector>

// A track stores key frames for one UI property.
class UIAnimationTrack {
public:
    UIAnimationTrack();
    explicit UIAnimationTrack(UIAnimationProperty property);

    UIAnimationProperty GetProperty() const;
    void SetProperty(UIAnimationProperty property);

    const std::vector<UIKeyFrame>& GetKeyFrames() const;
    std::vector<UIKeyFrame>& GetKeyFrames();

    bool HasKeyFrames() const;
    void ClearKeyFrames();

    int FindKeyFrameIndex(int frame) const;
    bool AddKeyFrame(int frame, float value);
    bool RemoveKeyFrame(int frame);
    bool RemoveKeyFrameAtIndex(std::size_t keyFrameIndex);
    void SortKeyFrames();

    float Evaluate(float frame, float defaultValue) const;

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& trackJson);

private:
    static bool CompareKeyFrameByFrame(const UIKeyFrame& left, const UIKeyFrame& right);
    float ApplyInterpolation(float t, UIAnimationInterpolation interpolation) const;

private:
    UIAnimationProperty property_ = UIAnimationProperty::PositionX;
    std::vector<UIKeyFrame> keyFrames_;
};
