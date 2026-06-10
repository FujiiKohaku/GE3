#pragma once

#include "UIAnimationPlayer.h"
#include <string>
#include <vector>

class Sprite;

// ImGui tool for editing clips and previewing them on a Sprite.
class UIAnimationEditor {
public:
    UIAnimationEditor();

    void Initialize();
    void Update(float deltaTime);
    void DrawImGui();

    void SetPreviewSprite(Sprite* sprite);

    UIAnimationClip* GetSelectedClip();
    const UIAnimationClip* GetSelectedClip() const;

private:
    void CreateDefaultClip();
    void SelectClip(int clipIndex);
    void SyncPlayerClip();
    void CopyTextToBuffer(char* buffer, int bufferSize, const std::string& text);

    void DrawClipList();
    void DrawTimeline();
    void DrawSelectedKeyFrame();

    void DrawTransport(UIAnimationClip* clip);
    void DrawLivePropertyControls(UIAnimationClip* clip);
    void DrawTimelineCanvas(UIAnimationClip* clip);
    void DrawKeyFrameInspector(UIAnimationClip* clip);

    void SaveSelectedClip();
    void LoadSelectedClip();
    void UpdateNameFromBuffer(UIAnimationClip* clip);
    void SelectKeyFrame(UIAnimationProperty property, int keyFrameIndex);
    void AddKeyFromPreview(UIAnimationClip* clip, UIAnimationProperty property);
    void AddTransformKeysFromPreview(UIAnimationClip* clip);
    float GetPreviewValue(UIAnimationProperty property) const;
    void SetPreviewValue(UIAnimationProperty property, float value);
    bool CanMoveKeyFrameToFrame(const UIAnimationTrack& track, int keyFrameIndex, int frame) const;
    int GetFrameFromTimelinePosition(float mousePositionX, float timelineStartX, float timelineWidth, int length) const;
    void ClampCurrentFrame(UIAnimationClip* clip);

private:
    std::vector<UIAnimationClip> clips_;
    int selectedClipIndex_ = -1;
    UIAnimationProperty selectedProperty_ = UIAnimationProperty::PositionX;
    int selectedKeyFrameIndex_ = -1;
    int currentFrame_ = 0;
    bool isAutoKeyEnabled_ = true;

    UIAnimationPlayer player_;
    Sprite* previewSprite_ = nullptr;

    char clipNameBuffer_[128] = {};
    char filePathBuffer_[260] = {};
};
