#include "AnimationData.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

const char* UIEditorPropertyToString(UIEditorProperty property)
{
    if (property == UIEditorProperty::PositionX) {
        return "PositionX";
    }
    if (property == UIEditorProperty::PositionY) {
        return "PositionY";
    }
    if (property == UIEditorProperty::ScaleX) {
        return "ScaleX";
    }
    if (property == UIEditorProperty::ScaleY) {
        return "ScaleY";
    }
    if (property == UIEditorProperty::Rotation) {
        return "Rotation";
    }
    if (property == UIEditorProperty::ColorR) {
        return "ColorR";
    }
    if (property == UIEditorProperty::ColorG) {
        return "ColorG";
    }
    if (property == UIEditorProperty::ColorB) {
        return "ColorB";
    }
    if (property == UIEditorProperty::ColorA) {
        return "ColorA";
    }

    return "PositionX";
}

bool UIEditorPropertyFromString(const std::string& text, UIEditorProperty* property)
{
    if (property == nullptr) {
        return false;
    }

    for (int propertyIndex = 0; propertyIndex < UIEditorPropertyCount(); ++propertyIndex) {
        UIEditorProperty currentProperty = UIEditorPropertyFromIndex(propertyIndex);

        if (text == UIEditorPropertyToString(currentProperty)) {
            *property = currentProperty;
            return true;
        }
    }

    return false;
}

int UIEditorPropertyCount()
{
    return 9;
}

UIEditorProperty UIEditorPropertyFromIndex(int index)
{
    if (index == 0) {
        return UIEditorProperty::PositionX;
    }
    if (index == 1) {
        return UIEditorProperty::PositionY;
    }
    if (index == 2) {
        return UIEditorProperty::ScaleX;
    }
    if (index == 3) {
        return UIEditorProperty::ScaleY;
    }
    if (index == 4) {
        return UIEditorProperty::Rotation;
    }
    if (index == 5) {
        return UIEditorProperty::ColorR;
    }
    if (index == 6) {
        return UIEditorProperty::ColorG;
    }
    if (index == 7) {
        return UIEditorProperty::ColorB;
    }
    if (index == 8) {
        return UIEditorProperty::ColorA;
    }

    return UIEditorProperty::PositionX;
}

const char* UIEditorTrackGroupToString(UIEditorTrackGroup group)
{
    if (group == UIEditorTrackGroup::Position) {
        return "Position";
    }
    if (group == UIEditorTrackGroup::Scale) {
        return "Scale";
    }
    if (group == UIEditorTrackGroup::Rotation) {
        return "Rotation";
    }
    if (group == UIEditorTrackGroup::Color) {
        return "Color";
    }
    if (group == UIEditorTrackGroup::Alpha) {
        return "Alpha";
    }

    return "Position";
}

int UIEditorTrackGroupCount()
{
    return 5;
}

UIEditorTrackGroup UIEditorTrackGroupFromIndex(int index)
{
    if (index == 0) {
        return UIEditorTrackGroup::Position;
    }
    if (index == 1) {
        return UIEditorTrackGroup::Scale;
    }
    if (index == 2) {
        return UIEditorTrackGroup::Rotation;
    }
    if (index == 3) {
        return UIEditorTrackGroup::Color;
    }
    if (index == 4) {
        return UIEditorTrackGroup::Alpha;
    }

    return UIEditorTrackGroup::Position;
}

bool UIEditorPropertyBelongsToGroup(UIEditorProperty property, UIEditorTrackGroup group)
{
    if (group == UIEditorTrackGroup::Position) {
        return property == UIEditorProperty::PositionX || property == UIEditorProperty::PositionY;
    }
    if (group == UIEditorTrackGroup::Scale) {
        return property == UIEditorProperty::ScaleX || property == UIEditorProperty::ScaleY;
    }
    if (group == UIEditorTrackGroup::Rotation) {
        return property == UIEditorProperty::Rotation;
    }
    if (group == UIEditorTrackGroup::Color) {
        return property == UIEditorProperty::ColorR || property == UIEditorProperty::ColorG || property == UIEditorProperty::ColorB;
    }
    if (group == UIEditorTrackGroup::Alpha) {
        return property == UIEditorProperty::ColorA;
    }

    return false;
}

const char* UIEditorInterpolationToString(UIEditorInterpolation interpolation)
{
    if (interpolation == UIEditorInterpolation::Linear) {
        return "Linear";
    }
    if (interpolation == UIEditorInterpolation::EaseIn) {
        return "EaseIn";
    }
    if (interpolation == UIEditorInterpolation::EaseOut) {
        return "EaseOut";
    }
    if (interpolation == UIEditorInterpolation::EaseInOut) {
        return "EaseInOut";
    }

    return "Linear";
}

bool UIEditorInterpolationFromString(const std::string& text, UIEditorInterpolation* interpolation)
{
    if (interpolation == nullptr) {
        return false;
    }

    if (text == "Linear") {
        *interpolation = UIEditorInterpolation::Linear;
        return true;
    }
    if (text == "EaseIn") {
        *interpolation = UIEditorInterpolation::EaseIn;
        return true;
    }
    if (text == "EaseOut") {
        *interpolation = UIEditorInterpolation::EaseOut;
        return true;
    }
    if (text == "EaseInOut") {
        *interpolation = UIEditorInterpolation::EaseInOut;
        return true;
    }

    return false;
}

int UIEditorInterpolationToIndex(UIEditorInterpolation interpolation)
{
    if (interpolation == UIEditorInterpolation::Linear) {
        return 0;
    }
    if (interpolation == UIEditorInterpolation::EaseIn) {
        return 1;
    }
    if (interpolation == UIEditorInterpolation::EaseOut) {
        return 2;
    }
    if (interpolation == UIEditorInterpolation::EaseInOut) {
        return 3;
    }

    return 0;
}

UIEditorInterpolation UIEditorInterpolationFromIndex(int index)
{
    if (index == 0) {
        return UIEditorInterpolation::Linear;
    }
    if (index == 1) {
        return UIEditorInterpolation::EaseIn;
    }
    if (index == 2) {
        return UIEditorInterpolation::EaseOut;
    }
    if (index == 3) {
        return UIEditorInterpolation::EaseInOut;
    }

    return UIEditorInterpolation::Linear;
}

int UIEditorInterpolationCount()
{
    return 4;
}

UIEditorKeyFrame::UIEditorKeyFrame()
{
}

UIEditorKeyFrame::UIEditorKeyFrame(int frame, float value)
{
    SetFrame(frame);
    value_ = value;
}

int UIEditorKeyFrame::GetFrame() const
{
    return frame_;
}

void UIEditorKeyFrame::SetFrame(int frame)
{
    frame_ = frame;

    if (frame_ < 0) {
        frame_ = 0;
    }
}

float UIEditorKeyFrame::GetValue() const
{
    return value_;
}

void UIEditorKeyFrame::SetValue(float value)
{
    value_ = value;
}

UIEditorInterpolation UIEditorKeyFrame::GetInterpolation() const
{
    return interpolation_;
}

void UIEditorKeyFrame::SetInterpolation(UIEditorInterpolation interpolation)
{
    interpolation_ = interpolation;
}

nlohmann::json UIEditorKeyFrame::ToJson() const
{
    nlohmann::json keyJson;

    keyJson["Frame"] = frame_;
    keyJson["Value"] = value_;

    if (interpolation_ != UIEditorInterpolation::Linear) {
        keyJson["Interpolation"] = UIEditorInterpolationToString(interpolation_);
    }

    return keyJson;
}

bool UIEditorKeyFrame::FromJson(const nlohmann::json& keyJson)
{
    if (!keyJson.contains("Frame")) {
        return false;
    }
    if (!keyJson.contains("Value")) {
        return false;
    }

    frame_ = keyJson["Frame"].get<int>();
    value_ = keyJson["Value"].get<float>();
    interpolation_ = UIEditorInterpolation::Linear;

    if (keyJson.contains("Interpolation")) {
        UIEditorInterpolation parsedInterpolation = UIEditorInterpolation::Linear;
        std::string interpolationText = keyJson["Interpolation"].get<std::string>();

        if (UIEditorInterpolationFromString(interpolationText, &parsedInterpolation)) {
            interpolation_ = parsedInterpolation;
        }
    }

    if (frame_ < 0) {
        frame_ = 0;
    }

    return true;
}

UIEditorTrack::UIEditorTrack()
{
}

UIEditorTrack::UIEditorTrack(UIEditorProperty property)
{
    property_ = property;
}

UIEditorProperty UIEditorTrack::GetProperty() const
{
    return property_;
}

void UIEditorTrack::SetProperty(UIEditorProperty property)
{
    property_ = property;
}

const std::vector<UIEditorKeyFrame>& UIEditorTrack::GetKeys() const
{
    return keys_;
}

std::vector<UIEditorKeyFrame>& UIEditorTrack::GetKeys()
{
    return keys_;
}

bool UIEditorTrack::HasKeys() const
{
    return !keys_.empty();
}

int UIEditorTrack::FindKeyIndex(int frame) const
{
    for (std::size_t keyIndex = 0; keyIndex < keys_.size(); ++keyIndex) {
        if (keys_[keyIndex].GetFrame() == frame) {
            return static_cast<int>(keyIndex);
        }
    }

    return -1;
}

bool UIEditorTrack::AddKey(int frame, float value)
{
    return AddKey(frame, value, UIEditorInterpolation::Linear);
}

bool UIEditorTrack::AddKey(int frame, float value, UIEditorInterpolation interpolation)
{
    int keyIndex = FindKeyIndex(frame);

    if (keyIndex >= 0) {
        keys_[static_cast<std::size_t>(keyIndex)].SetValue(value);
        return true;
    }

    UIEditorKeyFrame keyFrame(frame, value);
    keyFrame.SetInterpolation(interpolation);
    keys_.push_back(keyFrame);
    SortKeys();

    return true;
}

bool UIEditorTrack::SetKeyInterpolation(int frame, UIEditorInterpolation interpolation)
{
    int keyIndex = FindKeyIndex(frame);

    if (keyIndex < 0) {
        return false;
    }

    keys_[static_cast<std::size_t>(keyIndex)].SetInterpolation(interpolation);
    return true;
}

UIEditorInterpolation UIEditorTrack::GetKeyInterpolation(int frame, UIEditorInterpolation fallback) const
{
    int keyIndex = FindKeyIndex(frame);

    if (keyIndex < 0) {
        return fallback;
    }

    return keys_[static_cast<std::size_t>(keyIndex)].GetInterpolation();
}

bool UIEditorTrack::RemoveKeyAtIndex(std::size_t keyIndex)
{
    if (keyIndex >= keys_.size()) {
        return false;
    }

    keys_.erase(keys_.begin() + keyIndex);
    return true;
}

void UIEditorTrack::SortKeys()
{
    std::sort(keys_.begin(), keys_.end(), CompareByFrame);
}

float UIEditorTrack::Evaluate(float frame, float defaultValue) const
{
    if (keys_.empty()) {
        return defaultValue;
    }

    if (keys_.size() == 1) {
        return keys_[0].GetValue();
    }

    if (frame <= static_cast<float>(keys_[0].GetFrame())) {
        return keys_[0].GetValue();
    }

    std::size_t lastIndex = keys_.size() - 1;

    if (frame >= static_cast<float>(keys_[lastIndex].GetFrame())) {
        return keys_[lastIndex].GetValue();
    }

    for (std::size_t keyIndex = 0; keyIndex + 1 < keys_.size(); ++keyIndex) {
        const UIEditorKeyFrame& startKey = keys_[keyIndex];
        const UIEditorKeyFrame& endKey = keys_[keyIndex + 1];
        float startFrame = static_cast<float>(startKey.GetFrame());
        float endFrame = static_cast<float>(endKey.GetFrame());

        if (startFrame <= frame && frame <= endFrame) {
            float range = endFrame - startFrame;

            if (range <= 0.0f) {
                return endKey.GetValue();
            }

            float t = (frame - startFrame) / range;
            t = ApplyInterpolation(t, startKey.GetInterpolation());

            return startKey.GetValue() + (endKey.GetValue() - startKey.GetValue()) * t;
        }
    }

    return defaultValue;
}

nlohmann::json UIEditorTrack::ToJson() const
{
    nlohmann::json trackJson;

    trackJson["Property"] = UIEditorPropertyToString(property_);
    trackJson["Keys"] = nlohmann::json::array();

    for (std::size_t keyIndex = 0; keyIndex < keys_.size(); ++keyIndex) {
        trackJson["Keys"].push_back(keys_[keyIndex].ToJson());
    }

    return trackJson;
}

bool UIEditorTrack::FromJson(const nlohmann::json& trackJson)
{
    if (!trackJson.contains("Property")) {
        return false;
    }

    UIEditorProperty parsedProperty = UIEditorProperty::PositionX;
    std::string propertyText = trackJson["Property"].get<std::string>();

    if (!UIEditorPropertyFromString(propertyText, &parsedProperty)) {
        return false;
    }

    property_ = parsedProperty;
    keys_.clear();

    if (trackJson.contains("Keys")) {
        for (std::size_t keyIndex = 0; keyIndex < trackJson["Keys"].size(); ++keyIndex) {
            UIEditorKeyFrame keyFrame;

            if (keyFrame.FromJson(trackJson["Keys"][keyIndex])) {
                keys_.push_back(keyFrame);
            }
        }
    }

    SortKeys();
    return true;
}

bool UIEditorTrack::CompareByFrame(const UIEditorKeyFrame& left, const UIEditorKeyFrame& right)
{
    return left.GetFrame() < right.GetFrame();
}

float UIEditorTrack::ApplyInterpolation(float t, UIEditorInterpolation interpolation) const
{
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }

    if (interpolation == UIEditorInterpolation::EaseIn) {
        return t * t;
    }
    if (interpolation == UIEditorInterpolation::EaseOut) {
        float inverse = 1.0f - t;
        return 1.0f - inverse * inverse;
    }
    if (interpolation == UIEditorInterpolation::EaseInOut) {
        if (t < 0.5f) {
            return 2.0f * t * t;
        }

        float inverse = -2.0f * t + 2.0f;
        return 1.0f - inverse * inverse * 0.5f;
    }

    return t;
}

UIEditorAnimationClip::UIEditorAnimationClip()
{
    EnsureTracks();
}

const std::string& UIEditorAnimationClip::GetName() const
{
    return name_;
}

void UIEditorAnimationClip::SetName(const std::string& name)
{
    name_ = name;

    if (name_.empty()) {
        name_ = "NewAnimation";
    }
}

int UIEditorAnimationClip::GetLength() const
{
    return length_;
}

void UIEditorAnimationClip::SetLength(int length)
{
    length_ = length;

    if (length_ < 1) {
        length_ = 1;
    }
}

int UIEditorAnimationClip::GetCanvasWidth() const
{
    return canvasWidth_;
}

int UIEditorAnimationClip::GetCanvasHeight() const
{
    return canvasHeight_;
}

void UIEditorAnimationClip::SetCanvasSize(int width, int height)
{
    canvasWidth_ = width;
    canvasHeight_ = height;

    if (canvasWidth_ < 1) {
        canvasWidth_ = 1280;
    }
    if (canvasHeight_ < 1) {
        canvasHeight_ = 720;
    }
}

const std::string& UIEditorAnimationClip::GetTexturePath() const
{
    return texturePath_;
}

void UIEditorAnimationClip::SetTexturePath(const std::string& texturePath)
{
    texturePath_ = texturePath;
}

UIEditorObjectState UIEditorAnimationClip::GetBaseState() const
{
    return baseState_;
}

void UIEditorAnimationClip::SetBaseState(const UIEditorObjectState& state)
{
    baseState_ = state;
}

UIEditorTrack* UIEditorAnimationClip::GetTrack(UIEditorProperty property)
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return &tracks_[trackIndex];
        }
    }

    return nullptr;
}

const UIEditorTrack* UIEditorAnimationClip::GetTrack(UIEditorProperty property) const
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return &tracks_[trackIndex];
        }
    }

    return nullptr;
}

std::vector<UIEditorTrack>& UIEditorAnimationClip::GetTracks()
{
    return tracks_;
}

const std::vector<UIEditorTrack>& UIEditorAnimationClip::GetTracks() const
{
    return tracks_;
}

void UIEditorAnimationClip::ClearKeys()
{
    tracks_.clear();
    EnsureTracks();
}

void UIEditorAnimationClip::EnsureTracks()
{
    for (int propertyIndex = 0; propertyIndex < UIEditorPropertyCount(); ++propertyIndex) {
        UIEditorProperty property = UIEditorPropertyFromIndex(propertyIndex);

        if (!HasTrack(property)) {
            UIEditorTrack track(property);
            tracks_.push_back(track);
        }
    }
}

bool UIEditorAnimationClip::AddKey(UIEditorProperty property, int frame, float value)
{
    return AddKey(property, frame, value, UIEditorInterpolation::Linear);
}

bool UIEditorAnimationClip::AddKey(UIEditorProperty property, int frame, float value, UIEditorInterpolation interpolation)
{
    if (frame < 0) {
        frame = 0;
    }
    if (frame > length_) {
        frame = length_;
    }

    UIEditorTrack* track = GetTrack(property);

    if (track == nullptr) {
        UIEditorTrack newTrack(property);
        tracks_.push_back(newTrack);
        track = &tracks_[tracks_.size() - 1];
    }

    return track->AddKey(frame, value, interpolation);
}

bool UIEditorAnimationClip::SetKeyInterpolation(UIEditorProperty property, int frame, UIEditorInterpolation interpolation)
{
    UIEditorTrack* track = GetTrack(property);

    if (track == nullptr) {
        return false;
    }

    return track->SetKeyInterpolation(frame, interpolation);
}

UIEditorInterpolation UIEditorAnimationClip::GetKeyInterpolation(UIEditorProperty property, int frame, UIEditorInterpolation fallback) const
{
    const UIEditorTrack* track = GetTrack(property);

    if (track == nullptr) {
        return fallback;
    }

    return track->GetKeyInterpolation(frame, fallback);
}

UIEditorObjectState UIEditorAnimationClip::Evaluate(float frame) const
{
    UIEditorObjectState state = baseState_;

    const UIEditorTrack* positionXTrack = GetTrack(UIEditorProperty::PositionX);
    const UIEditorTrack* positionYTrack = GetTrack(UIEditorProperty::PositionY);
    const UIEditorTrack* scaleXTrack = GetTrack(UIEditorProperty::ScaleX);
    const UIEditorTrack* scaleYTrack = GetTrack(UIEditorProperty::ScaleY);
    const UIEditorTrack* rotationTrack = GetTrack(UIEditorProperty::Rotation);
    const UIEditorTrack* colorRTrack = GetTrack(UIEditorProperty::ColorR);
    const UIEditorTrack* colorGTrack = GetTrack(UIEditorProperty::ColorG);
    const UIEditorTrack* colorBTrack = GetTrack(UIEditorProperty::ColorB);
    const UIEditorTrack* colorATrack = GetTrack(UIEditorProperty::ColorA);

    if (positionXTrack != nullptr) {
        state.position.x = positionXTrack->Evaluate(frame, state.position.x);
    }
    if (positionYTrack != nullptr) {
        state.position.y = positionYTrack->Evaluate(frame, state.position.y);
    }
    if (scaleXTrack != nullptr) {
        state.scale.x = scaleXTrack->Evaluate(frame, state.scale.x);
    }
    if (scaleYTrack != nullptr) {
        state.scale.y = scaleYTrack->Evaluate(frame, state.scale.y);
    }
    if (rotationTrack != nullptr) {
        state.rotation = rotationTrack->Evaluate(frame, state.rotation);
    }
    if (colorRTrack != nullptr) {
        state.color.r = colorRTrack->Evaluate(frame, state.color.r);
    }
    if (colorGTrack != nullptr) {
        state.color.g = colorGTrack->Evaluate(frame, state.color.g);
    }
    if (colorBTrack != nullptr) {
        state.color.b = colorBTrack->Evaluate(frame, state.color.b);
    }
    if (colorATrack != nullptr) {
        state.color.a = colorATrack->Evaluate(frame, state.color.a);
    }

    return state;
}

nlohmann::json UIEditorAnimationClip::ToRuntimeJson() const
{
    nlohmann::json clipJson;

    clipJson["Name"] = name_;
    clipJson["Length"] = length_;
    clipJson["Texture"] = texturePath_;
    clipJson["CanvasWidth"] = canvasWidth_;
    clipJson["CanvasHeight"] = canvasHeight_;
    clipJson["Tracks"] = nlohmann::json::array();

    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].HasKeys()) {
            clipJson["Tracks"].push_back(tracks_[trackIndex].ToJson());
        }
    }

    return clipJson;
}

nlohmann::json UIEditorAnimationClip::ToProjectJson() const
{
    nlohmann::json clipJson = ToRuntimeJson();

    clipJson["BaseState"]["Position"] = { baseState_.position.x, baseState_.position.y };
    clipJson["BaseState"]["Scale"] = { baseState_.scale.x, baseState_.scale.y };
    clipJson["BaseState"]["Rotation"] = baseState_.rotation;
    clipJson["BaseState"]["Color"] = { baseState_.color.r, baseState_.color.g, baseState_.color.b, baseState_.color.a };

    return clipJson;
}

bool UIEditorAnimationClip::FromProjectJson(const nlohmann::json& clipJson)
{
    if (!clipJson.contains("Name")) {
        return false;
    }
    if (!clipJson.contains("Length")) {
        return false;
    }

    name_ = clipJson["Name"].get<std::string>();
    length_ = clipJson["Length"].get<int>();

    if (clipJson.contains("Texture")) {
        texturePath_ = clipJson["Texture"].get<std::string>();
    }
    if (clipJson.contains("CanvasWidth")) {
        canvasWidth_ = clipJson["CanvasWidth"].get<int>();
    }
    if (clipJson.contains("CanvasHeight")) {
        canvasHeight_ = clipJson["CanvasHeight"].get<int>();
    }
    if (clipJson.contains("BaseState")) {
        const nlohmann::json& stateJson = clipJson["BaseState"];

        if (stateJson.contains("Position")) {
            baseState_.position.x = stateJson["Position"][0].get<float>();
            baseState_.position.y = stateJson["Position"][1].get<float>();
        }
        if (stateJson.contains("Scale")) {
            baseState_.scale.x = stateJson["Scale"][0].get<float>();
            baseState_.scale.y = stateJson["Scale"][1].get<float>();
        }
        if (stateJson.contains("Rotation")) {
            baseState_.rotation = stateJson["Rotation"].get<float>();
        }
        if (stateJson.contains("Color")) {
            baseState_.color.r = stateJson["Color"][0].get<float>();
            baseState_.color.g = stateJson["Color"][1].get<float>();
            baseState_.color.b = stateJson["Color"][2].get<float>();
            baseState_.color.a = stateJson["Color"][3].get<float>();
        }
    }

    tracks_.clear();

    if (clipJson.contains("Tracks")) {
        for (std::size_t trackIndex = 0; trackIndex < clipJson["Tracks"].size(); ++trackIndex) {
            UIEditorTrack track;

            if (track.FromJson(clipJson["Tracks"][trackIndex])) {
                tracks_.push_back(track);
            }
        }
    }

    SetLength(length_);
    SetCanvasSize(canvasWidth_, canvasHeight_);
    EnsureTracks();

    return true;
}

bool UIEditorAnimationClip::SaveProjectFile(const std::string& filePath) const
{
    std::filesystem::path outputPath(filePath);
    std::filesystem::path parentPath = outputPath.parent_path();

    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    std::ofstream file(filePath);

    if (!file.is_open()) {
        return false;
    }

    file << ToProjectJson().dump(4);
    file.close();

    return true;
}

bool UIEditorAnimationClip::LoadProjectFile(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return false;
    }

    nlohmann::json clipJson;
    file >> clipJson;
    file.close();

    return FromProjectJson(clipJson);
}

bool UIEditorAnimationClip::ExportRuntimeJson(const std::string& filePath) const
{
    std::filesystem::path outputPath(filePath);
    std::filesystem::path parentPath = outputPath.parent_path();

    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    std::ofstream file(filePath);

    if (!file.is_open()) {
        return false;
    }

    file << ToRuntimeJson().dump(4);
    file.close();

    return true;
}

bool UIEditorAnimationClip::HasTrack(UIEditorProperty property) const
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return true;
        }
    }

    return false;
}
