#include "UIKeyFrame.h"

const char* UIAnimationPropertyToString(UIAnimationProperty property)
{
    if (property == UIAnimationProperty::PositionX) {
        return "PositionX";
    }
    if (property == UIAnimationProperty::PositionY) {
        return "PositionY";
    }
    if (property == UIAnimationProperty::ScaleX) {
        return "ScaleX";
    }
    if (property == UIAnimationProperty::ScaleY) {
        return "ScaleY";
    }
    if (property == UIAnimationProperty::Rotation) {
        return "Rotation";
    }
    if (property == UIAnimationProperty::ColorR) {
        return "ColorR";
    }
    if (property == UIAnimationProperty::ColorG) {
        return "ColorG";
    }
    if (property == UIAnimationProperty::ColorB) {
        return "ColorB";
    }
    if (property == UIAnimationProperty::ColorA) {
        return "ColorA";
    }

    return "PositionX";
}

bool UIAnimationPropertyFromString(const std::string& text, UIAnimationProperty* property)
{
    if (property == nullptr) {
        return false;
    }

    if (text == "PositionX") {
        *property = UIAnimationProperty::PositionX;
        return true;
    }
    if (text == "PositionY") {
        *property = UIAnimationProperty::PositionY;
        return true;
    }
    if (text == "ScaleX") {
        *property = UIAnimationProperty::ScaleX;
        return true;
    }
    if (text == "ScaleY") {
        *property = UIAnimationProperty::ScaleY;
        return true;
    }
    if (text == "Rotation") {
        *property = UIAnimationProperty::Rotation;
        return true;
    }
    if (text == "ColorR") {
        *property = UIAnimationProperty::ColorR;
        return true;
    }
    if (text == "ColorG") {
        *property = UIAnimationProperty::ColorG;
        return true;
    }
    if (text == "ColorB") {
        *property = UIAnimationProperty::ColorB;
        return true;
    }
    if (text == "ColorA") {
        *property = UIAnimationProperty::ColorA;
        return true;
    }

    return false;
}

int UIAnimationPropertyToIndex(UIAnimationProperty property)
{
    if (property == UIAnimationProperty::PositionX) {
        return 0;
    }
    if (property == UIAnimationProperty::PositionY) {
        return 1;
    }
    if (property == UIAnimationProperty::ScaleX) {
        return 2;
    }
    if (property == UIAnimationProperty::ScaleY) {
        return 3;
    }
    if (property == UIAnimationProperty::Rotation) {
        return 4;
    }
    if (property == UIAnimationProperty::ColorR) {
        return 5;
    }
    if (property == UIAnimationProperty::ColorG) {
        return 6;
    }
    if (property == UIAnimationProperty::ColorB) {
        return 7;
    }
    if (property == UIAnimationProperty::ColorA) {
        return 8;
    }

    return 0;
}

UIAnimationProperty UIAnimationPropertyFromIndex(int index)
{
    if (index == 0) {
        return UIAnimationProperty::PositionX;
    }
    if (index == 1) {
        return UIAnimationProperty::PositionY;
    }
    if (index == 2) {
        return UIAnimationProperty::ScaleX;
    }
    if (index == 3) {
        return UIAnimationProperty::ScaleY;
    }
    if (index == 4) {
        return UIAnimationProperty::Rotation;
    }
    if (index == 5) {
        return UIAnimationProperty::ColorR;
    }
    if (index == 6) {
        return UIAnimationProperty::ColorG;
    }
    if (index == 7) {
        return UIAnimationProperty::ColorB;
    }
    if (index == 8) {
        return UIAnimationProperty::ColorA;
    }

    return UIAnimationProperty::PositionX;
}

int UIAnimationPropertyCount()
{
    return 9;
}

const char* UIAnimationInterpolationToString(UIAnimationInterpolation interpolation)
{
    if (interpolation == UIAnimationInterpolation::Linear) {
        return "Linear";
    }
    if (interpolation == UIAnimationInterpolation::EaseIn) {
        return "EaseIn";
    }
    if (interpolation == UIAnimationInterpolation::EaseOut) {
        return "EaseOut";
    }
    if (interpolation == UIAnimationInterpolation::EaseInOut) {
        return "EaseInOut";
    }

    return "Linear";
}

bool UIAnimationInterpolationFromString(const std::string& text, UIAnimationInterpolation* interpolation)
{
    if (interpolation == nullptr) {
        return false;
    }

    if (text == "Linear") {
        *interpolation = UIAnimationInterpolation::Linear;
        return true;
    }
    if (text == "EaseIn") {
        *interpolation = UIAnimationInterpolation::EaseIn;
        return true;
    }
    if (text == "EaseOut") {
        *interpolation = UIAnimationInterpolation::EaseOut;
        return true;
    }
    if (text == "EaseInOut") {
        *interpolation = UIAnimationInterpolation::EaseInOut;
        return true;
    }

    return false;
}

int UIAnimationInterpolationToIndex(UIAnimationInterpolation interpolation)
{
    if (interpolation == UIAnimationInterpolation::Linear) {
        return 0;
    }
    if (interpolation == UIAnimationInterpolation::EaseIn) {
        return 1;
    }
    if (interpolation == UIAnimationInterpolation::EaseOut) {
        return 2;
    }
    if (interpolation == UIAnimationInterpolation::EaseInOut) {
        return 3;
    }

    return 0;
}

UIAnimationInterpolation UIAnimationInterpolationFromIndex(int index)
{
    if (index == 0) {
        return UIAnimationInterpolation::Linear;
    }
    if (index == 1) {
        return UIAnimationInterpolation::EaseIn;
    }
    if (index == 2) {
        return UIAnimationInterpolation::EaseOut;
    }
    if (index == 3) {
        return UIAnimationInterpolation::EaseInOut;
    }

    return UIAnimationInterpolation::Linear;
}

int UIAnimationInterpolationCount()
{
    return 4;
}

UIKeyFrame::UIKeyFrame()
{
}

UIKeyFrame::UIKeyFrame(int frame, float value)
{
    frame_ = frame;
    value_ = value;
}

int UIKeyFrame::GetFrame() const
{
    return frame_;
}

void UIKeyFrame::SetFrame(int frame)
{
    frame_ = frame;

    if (frame_ < 0) {
        frame_ = 0;
    }
}

float UIKeyFrame::GetValue() const
{
    return value_;
}

void UIKeyFrame::SetValue(float value)
{
    value_ = value;
}

UIAnimationInterpolation UIKeyFrame::GetInterpolation() const
{
    return interpolation_;
}

void UIKeyFrame::SetInterpolation(UIAnimationInterpolation interpolation)
{
    interpolation_ = interpolation;
}

nlohmann::json UIKeyFrame::ToJson() const
{
    nlohmann::json keyFrameJson;

    keyFrameJson["Frame"] = frame_;
    keyFrameJson["Value"] = value_;

    if (interpolation_ != UIAnimationInterpolation::Linear) {
        keyFrameJson["Interpolation"] = UIAnimationInterpolationToString(interpolation_);
    }

    return keyFrameJson;
}

bool UIKeyFrame::FromJson(const nlohmann::json& keyFrameJson)
{
    if (!keyFrameJson.contains("Frame")) {
        return false;
    }
    if (!keyFrameJson.contains("Value")) {
        return false;
    }

    frame_ = keyFrameJson["Frame"].get<int>();
    value_ = keyFrameJson["Value"].get<float>();
    interpolation_ = UIAnimationInterpolation::Linear;

    if (keyFrameJson.contains("Interpolation")) {
        std::string interpolationText = keyFrameJson["Interpolation"].get<std::string>();
        UIAnimationInterpolation parsedInterpolation = UIAnimationInterpolation::Linear;

        if (UIAnimationInterpolationFromString(interpolationText, &parsedInterpolation)) {
            interpolation_ = parsedInterpolation;
        }
    }

    if (frame_ < 0) {
        frame_ = 0;
    }

    return true;
}
