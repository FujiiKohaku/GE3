#include "UIAnimationTrack.h"
#include <algorithm>

UIAnimationTrack::UIAnimationTrack()
{
}

UIAnimationTrack::UIAnimationTrack(UIAnimationProperty property)
{
    property_ = property;
}

UIAnimationProperty UIAnimationTrack::GetProperty() const
{
    return property_;
}

void UIAnimationTrack::SetProperty(UIAnimationProperty property)
{
    property_ = property;
}

const std::vector<UIKeyFrame>& UIAnimationTrack::GetKeyFrames() const
{
    return keyFrames_;
}

std::vector<UIKeyFrame>& UIAnimationTrack::GetKeyFrames()
{
    return keyFrames_;
}

bool UIAnimationTrack::HasKeyFrames() const
{
    return !keyFrames_.empty();
}

void UIAnimationTrack::ClearKeyFrames()
{
    keyFrames_.clear();
}

int UIAnimationTrack::FindKeyFrameIndex(int frame) const
{
    for (std::size_t keyFrameIndex = 0; keyFrameIndex < keyFrames_.size(); ++keyFrameIndex) {
        if (keyFrames_[keyFrameIndex].GetFrame() == frame) {
            return static_cast<int>(keyFrameIndex);
        }
    }

    return -1;
}

bool UIAnimationTrack::AddKeyFrame(int frame, float value)
{
    if (frame < 0) {
        frame = 0;
    }

    int keyFrameIndex = FindKeyFrameIndex(frame);

    if (keyFrameIndex >= 0) {
        keyFrames_[static_cast<std::size_t>(keyFrameIndex)].SetValue(value);
        return true;
    }

    UIKeyFrame keyFrame(frame, value);
    keyFrames_.push_back(keyFrame);
    SortKeyFrames();

    return true;
}

bool UIAnimationTrack::RemoveKeyFrame(int frame)
{
    int keyFrameIndex = FindKeyFrameIndex(frame);

    if (keyFrameIndex < 0) {
        return false;
    }

    return RemoveKeyFrameAtIndex(static_cast<std::size_t>(keyFrameIndex));
}

bool UIAnimationTrack::RemoveKeyFrameAtIndex(std::size_t keyFrameIndex)
{
    if (keyFrameIndex >= keyFrames_.size()) {
        return false;
    }

    keyFrames_.erase(keyFrames_.begin() + keyFrameIndex);
    return true;
}

void UIAnimationTrack::SortKeyFrames()
{
    std::sort(keyFrames_.begin(), keyFrames_.end(), CompareKeyFrameByFrame);
}

float UIAnimationTrack::Evaluate(float frame, float defaultValue) const
{
    if (keyFrames_.empty()) {
        return defaultValue;
    }

    if (keyFrames_.size() == 1) {
        return keyFrames_[0].GetValue();
    }

    if (frame <= static_cast<float>(keyFrames_[0].GetFrame())) {
        return keyFrames_[0].GetValue();
    }

    std::size_t lastIndex = keyFrames_.size() - 1;

    if (frame >= static_cast<float>(keyFrames_[lastIndex].GetFrame())) {
        return keyFrames_[lastIndex].GetValue();
    }

    for (std::size_t keyFrameIndex = 0; keyFrameIndex + 1 < keyFrames_.size(); ++keyFrameIndex) {
        const UIKeyFrame& startKeyFrame = keyFrames_[keyFrameIndex];
        const UIKeyFrame& endKeyFrame = keyFrames_[keyFrameIndex + 1];
        float startFrame = static_cast<float>(startKeyFrame.GetFrame());
        float endFrame = static_cast<float>(endKeyFrame.GetFrame());

        if (startFrame <= frame && frame <= endFrame) {
            float frameRange = endFrame - startFrame;

            if (frameRange <= 0.0f) {
                return endKeyFrame.GetValue();
            }

            float t = (frame - startFrame) / frameRange;
            t = ApplyInterpolation(t, startKeyFrame.GetInterpolation());

            return startKeyFrame.GetValue() + (endKeyFrame.GetValue() - startKeyFrame.GetValue()) * t;
        }
    }

    return defaultValue;
}

nlohmann::json UIAnimationTrack::ToJson() const
{
    nlohmann::json trackJson;

    trackJson["Property"] = UIAnimationPropertyToString(property_);
    trackJson["Keys"] = nlohmann::json::array();

    for (std::size_t keyFrameIndex = 0; keyFrameIndex < keyFrames_.size(); ++keyFrameIndex) {
        trackJson["Keys"].push_back(keyFrames_[keyFrameIndex].ToJson());
    }

    return trackJson;
}

bool UIAnimationTrack::FromJson(const nlohmann::json& trackJson)
{
    if (!trackJson.contains("Property")) {
        return false;
    }

    std::string propertyText = trackJson["Property"].get<std::string>();
    UIAnimationProperty parsedProperty = UIAnimationProperty::PositionX;

    if (!UIAnimationPropertyFromString(propertyText, &parsedProperty)) {
        return false;
    }

    property_ = parsedProperty;
    keyFrames_.clear();

    if (trackJson.contains("Keys")) {
        for (std::size_t keyFrameIndex = 0; keyFrameIndex < trackJson["Keys"].size(); ++keyFrameIndex) {
            UIKeyFrame keyFrame;

            if (keyFrame.FromJson(trackJson["Keys"][keyFrameIndex])) {
                keyFrames_.push_back(keyFrame);
            }
        }
    }

    SortKeyFrames();
    return true;
}

bool UIAnimationTrack::CompareKeyFrameByFrame(const UIKeyFrame& left, const UIKeyFrame& right)
{
    return left.GetFrame() < right.GetFrame();
}

float UIAnimationTrack::ApplyInterpolation(float t, UIAnimationInterpolation interpolation) const
{
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }

    if (interpolation == UIAnimationInterpolation::EaseIn) {
        return t * t;
    }
    if (interpolation == UIAnimationInterpolation::EaseOut) {
        float inverse = 1.0f - t;
        return 1.0f - inverse * inverse;
    }
    if (interpolation == UIAnimationInterpolation::EaseInOut) {
        if (t < 0.5f) {
            return 2.0f * t * t;
        }

        float inverse = -2.0f * t + 2.0f;
        return 1.0f - inverse * inverse * 0.5f;
    }

    return t;
}
