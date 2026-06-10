#include "UIAnimationClip.h"
#include <filesystem>
#include <fstream>

UIAnimationClip::UIAnimationClip()
{
    EnsureAllTracks();
}

const std::string& UIAnimationClip::GetName() const
{
    return name_;
}

void UIAnimationClip::SetName(const std::string& name)
{
    name_ = name;

    if (name_.empty()) {
        name_ = "NewClip";
    }
}

int UIAnimationClip::GetLength() const
{
    return length_;
}

void UIAnimationClip::SetLength(int length)
{
    length_ = length;

    if (length_ < 1) {
        length_ = 1;
    }
}

const std::vector<UIAnimationTrack>& UIAnimationClip::GetTracks() const
{
    return tracks_;
}

std::vector<UIAnimationTrack>& UIAnimationClip::GetTracks()
{
    return tracks_;
}

void UIAnimationClip::Clear()
{
    name_ = "NewClip";
    length_ = 60;
    tracks_.clear();
    EnsureAllTracks();
}

void UIAnimationClip::EnsureAllTracks()
{
    for (int propertyIndex = 0; propertyIndex < UIAnimationPropertyCount(); ++propertyIndex) {
        UIAnimationProperty property = UIAnimationPropertyFromIndex(propertyIndex);

        if (!HasTrack(property)) {
            UIAnimationTrack track(property);
            tracks_.push_back(track);
        }
    }
}

UIAnimationTrack* UIAnimationClip::GetTrack(UIAnimationProperty property)
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return &tracks_[trackIndex];
        }
    }

    return nullptr;
}

const UIAnimationTrack* UIAnimationClip::GetTrack(UIAnimationProperty property) const
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return &tracks_[trackIndex];
        }
    }

    return nullptr;
}

bool UIAnimationClip::AddKeyFrame(UIAnimationProperty property, int frame, float value)
{
    if (frame < 0) {
        frame = 0;
    }
    if (frame > length_) {
        frame = length_;
    }

    UIAnimationTrack* track = GetTrack(property);

    if (track == nullptr) {
        UIAnimationTrack newTrack(property);
        tracks_.push_back(newTrack);
        track = &tracks_[tracks_.size() - 1];
    }

    return track->AddKeyFrame(frame, value);
}

float UIAnimationClip::EvaluateProperty(UIAnimationProperty property, float frame, float defaultValue) const
{
    const UIAnimationTrack* track = GetTrack(property);

    if (track == nullptr) {
        return defaultValue;
    }

    return track->Evaluate(frame, defaultValue);
}

nlohmann::json UIAnimationClip::ToJson() const
{
    nlohmann::json clipJson;

    clipJson["Name"] = name_;
    clipJson["Length"] = length_;
    clipJson["Tracks"] = nlohmann::json::array();

    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].HasKeyFrames()) {
            clipJson["Tracks"].push_back(tracks_[trackIndex].ToJson());
        }
    }

    return clipJson;
}

bool UIAnimationClip::FromJson(const nlohmann::json& clipJson)
{
    if (!clipJson.contains("Name")) {
        return false;
    }
    if (!clipJson.contains("Length")) {
        return false;
    }

    name_ = clipJson["Name"].get<std::string>();
    length_ = clipJson["Length"].get<int>();

    if (length_ < 1) {
        length_ = 1;
    }

    tracks_.clear();

    if (clipJson.contains("Tracks")) {
        for (std::size_t trackIndex = 0; trackIndex < clipJson["Tracks"].size(); ++trackIndex) {
            UIAnimationTrack track;

            if (track.FromJson(clipJson["Tracks"][trackIndex])) {
                tracks_.push_back(track);
            }
        }
    }

    EnsureAllTracks();
    return true;
}

bool UIAnimationClip::SaveToFile(const std::string& filePath) const
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

    file << ToJson().dump(4);
    file.close();

    return true;
}

bool UIAnimationClip::LoadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return false;
    }

    nlohmann::json clipJson;
    file >> clipJson;
    file.close();

    return FromJson(clipJson);
}

bool UIAnimationClip::HasTrack(UIAnimationProperty property) const
{
    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex) {
        if (tracks_[trackIndex].GetProperty() == property) {
            return true;
        }
    }

    return false;
}
