#pragma once

#include "UIAnimationTrack.h"
#include <string>
#include <vector>

// A clip owns multiple property tracks and handles JSON file IO.
class UIAnimationClip {
public:
    UIAnimationClip();

    const std::string& GetName() const;
    void SetName(const std::string& name);

    int GetLength() const;
    void SetLength(int length);

    const std::vector<UIAnimationTrack>& GetTracks() const;
    std::vector<UIAnimationTrack>& GetTracks();

    void Clear();
    void EnsureAllTracks();
    UIAnimationTrack* GetTrack(UIAnimationProperty property);
    const UIAnimationTrack* GetTrack(UIAnimationProperty property) const;

    bool AddKeyFrame(UIAnimationProperty property, int frame, float value);
    float EvaluateProperty(UIAnimationProperty property, float frame, float defaultValue) const;

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& clipJson);
    bool SaveToFile(const std::string& filePath) const;
    bool LoadFromFile(const std::string& filePath);

private:
    bool HasTrack(UIAnimationProperty property) const;

private:
    std::string name_ = "TitleOpen";
    int length_ = 60;
    std::vector<UIAnimationTrack> tracks_;
};
