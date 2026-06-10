#pragma once

#include "externals/json.hpp"
#include <string>
#include <vector>

struct UIEditorVector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct UIEditorColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct UIEditorObjectState {
    UIEditorVector2 position = { 960.0f, 540.0f };
    UIEditorVector2 scale = { 256.0f, 256.0f };
    float rotation = 0.0f;
    UIEditorColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

enum class UIEditorProperty {
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

enum class UIEditorTrackGroup {
    Position,
    Scale,
    Rotation,
    Color,
    Alpha
};

enum class UIEditorInterpolation {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

const char* UIEditorPropertyToString(UIEditorProperty property);
bool UIEditorPropertyFromString(const std::string& text, UIEditorProperty* property);
int UIEditorPropertyCount();
UIEditorProperty UIEditorPropertyFromIndex(int index);

const char* UIEditorTrackGroupToString(UIEditorTrackGroup group);
int UIEditorTrackGroupCount();
UIEditorTrackGroup UIEditorTrackGroupFromIndex(int index);
bool UIEditorPropertyBelongsToGroup(UIEditorProperty property, UIEditorTrackGroup group);

const char* UIEditorInterpolationToString(UIEditorInterpolation interpolation);
bool UIEditorInterpolationFromString(const std::string& text, UIEditorInterpolation* interpolation);
int UIEditorInterpolationToIndex(UIEditorInterpolation interpolation);
UIEditorInterpolation UIEditorInterpolationFromIndex(int index);
int UIEditorInterpolationCount();

class UIEditorKeyFrame {
public:
    UIEditorKeyFrame();
    UIEditorKeyFrame(int frame, float value);

    int GetFrame() const;
    void SetFrame(int frame);

    float GetValue() const;
    void SetValue(float value);

    UIEditorInterpolation GetInterpolation() const;
    void SetInterpolation(UIEditorInterpolation interpolation);

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& keyJson);

private:
    int frame_ = 0;
    float value_ = 0.0f;
    UIEditorInterpolation interpolation_ = UIEditorInterpolation::Linear;
};

class UIEditorTrack {
public:
    UIEditorTrack();
    explicit UIEditorTrack(UIEditorProperty property);

    UIEditorProperty GetProperty() const;
    void SetProperty(UIEditorProperty property);

    const std::vector<UIEditorKeyFrame>& GetKeys() const;
    std::vector<UIEditorKeyFrame>& GetKeys();

    bool HasKeys() const;
    int FindKeyIndex(int frame) const;
    bool AddKey(int frame, float value);
    bool AddKey(int frame, float value, UIEditorInterpolation interpolation);
    bool SetKeyInterpolation(int frame, UIEditorInterpolation interpolation);
    UIEditorInterpolation GetKeyInterpolation(int frame, UIEditorInterpolation fallback) const;
    bool RemoveKeyAtIndex(std::size_t keyIndex);
    void SortKeys();
    float Evaluate(float frame, float defaultValue) const;

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& trackJson);

private:
    static bool CompareByFrame(const UIEditorKeyFrame& left, const UIEditorKeyFrame& right);
    float ApplyInterpolation(float t, UIEditorInterpolation interpolation) const;

private:
    UIEditorProperty property_ = UIEditorProperty::PositionX;
    std::vector<UIEditorKeyFrame> keys_;
};

class UIEditorAnimationClip {
public:
    UIEditorAnimationClip();

    const std::string& GetName() const;
    void SetName(const std::string& name);

    int GetLength() const;
    void SetLength(int length);

    int GetCanvasWidth() const;
    int GetCanvasHeight() const;
    void SetCanvasSize(int width, int height);

    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& texturePath);

    UIEditorObjectState GetBaseState() const;
    void SetBaseState(const UIEditorObjectState& state);

    UIEditorTrack* GetTrack(UIEditorProperty property);
    const UIEditorTrack* GetTrack(UIEditorProperty property) const;
    std::vector<UIEditorTrack>& GetTracks();
    const std::vector<UIEditorTrack>& GetTracks() const;

    void ClearKeys();
    void EnsureTracks();
    bool AddKey(UIEditorProperty property, int frame, float value);
    bool AddKey(UIEditorProperty property, int frame, float value, UIEditorInterpolation interpolation);
    bool SetKeyInterpolation(UIEditorProperty property, int frame, UIEditorInterpolation interpolation);
    UIEditorInterpolation GetKeyInterpolation(UIEditorProperty property, int frame, UIEditorInterpolation fallback) const;
    UIEditorObjectState Evaluate(float frame) const;

    nlohmann::json ToRuntimeJson() const;
    nlohmann::json ToProjectJson() const;
    bool FromProjectJson(const nlohmann::json& clipJson);

    bool SaveProjectFile(const std::string& filePath) const;
    bool LoadProjectFile(const std::string& filePath);
    bool ExportRuntimeJson(const std::string& filePath) const;

private:
    bool HasTrack(UIEditorProperty property) const;

private:
    std::string name_ = "TitleOpen";
    int length_ = 60;
    int canvasWidth_ = 1280;
    int canvasHeight_ = 720;
    std::string texturePath_;
    UIEditorObjectState baseState_;
    std::vector<UIEditorTrack> tracks_;
};
