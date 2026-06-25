#pragma once

#include "AIAnimationWindow.h"
#include "AnimationData.h"
#include "StandaloneD3D12.h"
#include <string>

struct ImVec2;

class UIAnimationEditorApp {
public:
    UIAnimationEditorApp();

    void Initialize(HWND hwnd, UIEditorD3D12* d3d12);
    void Update(float deltaTime);
    void Draw();
    void OnDropFile(const std::wstring& filePath);

private:
    void NewAnimation();
    void LoadTextureFile(const std::wstring& filePath);
    void OpenTextureDialog();
    void OpenAnimationDialog();
    void SaveAnimation();
    void SaveAnimationAs();
    void ExportJson();
    void PickExportFolder();

    void DrawMainMenu();
    void DrawTopLayout();
    void DrawAnimationListPanel();
    void DrawPreviewPanel();
    void DrawInspectorPanel();
    void DrawTimelinePanel();

    void DrawPreviewObject(const ImVec2& canvasPosition, float canvasScale);
    void DrawPreviewObjectState(const ImVec2& canvasPosition, float canvasScale, const UIEditorObjectState& state, unsigned int outlineColor, float alphaScale);
    void DrawPreviewGrid(const ImVec2& canvasPosition, const ImVec2& canvasSize, float canvasScale);
    UIEditorVector2 ScreenToCanvasPosition(const ImVec2& canvasPosition, float canvasScale, const ImVec2& screenPosition) const;
    bool IsMouseInsidePreviewObject(const ImVec2& canvasPosition, float canvasScale, const ImVec2& mousePosition) const;
    void DrawTimelineGroup(UIEditorTrackGroup group, float startX, float startY, float width, float rowHeight);
    bool DrawInterpolationCombo(const char* label, UIEditorInterpolation* interpolation);

    void AddKeyForProperty(UIEditorProperty property, int frame, float value);
    void AddKeyForGroup(UIEditorTrackGroup group);
    void AddKeyForGroupAtFrame(UIEditorTrackGroup group, int frame, const UIEditorObjectState& state);
    void AddKeyForCurrentTransform();
    void DeleteSelectedKey();
    void DuplicateSelectedKey();
    void MoveGroupKeys(UIEditorTrackGroup group, int oldFrame, int newFrame);
    bool GroupHasKeyAtFrame(UIEditorTrackGroup group, int frame) const;
    void SetInterpolationForGroup(UIEditorTrackGroup group, int frame, UIEditorInterpolation interpolation);
    UIEditorInterpolation GetInterpolationForGroup(UIEditorTrackGroup group, int frame) const;

    void SetCurrentFrame(int frame);
    void ClampCurrentFrame();
    void CenterPreviewObject();
    void ResetPreviewObject();
    void ApplyPreviewStateToBase();
    void EvaluatePreviewFromTimeline();

    std::wstring OpenFileDialog(const wchar_t* filter);
    std::wstring SaveFileDialog(const wchar_t* filter, const wchar_t* defaultExtension);
    std::wstring PickFolderDialog();

    int GetFrameFromTimelineX(float mouseX, float startX, float width) const;
    float GetTimelineXFromFrame(int frame, float startX, float width) const;
    unsigned int GetInterpolationColor(UIEditorInterpolation interpolation, bool isSelected) const;

    void CopyTextToBuffer(char* buffer, int bufferSize, const std::string& text);
    void RefreshNameBuffer();

private:
    HWND hwnd_ = nullptr;
    UIEditorD3D12* d3d12_ = nullptr;

    UIEditorAnimationClip clip_;
    UIEditorObjectState previewState_;
    UIEditorTextureResource texture_;
    AIAnimationWindow aiAnimationWindow_;

    std::string projectFilePath_;
    std::string exportFolderPath_;
    std::string statusMessage_ = "Drop a PNG to start.";

    int currentFrame_ = 0;
    float playbackFrame_ = 0.0f;
    float playbackSpeed_ = 1.0f;
    float timelineZoom_ = 1.0f;
    int duplicateFrameOffset_ = 10;
    int ghostFrameOffset_ = 10;
    bool isPlaying_ = false;
    bool isLoopEnabled_ = true;
    bool isAutoKeyEnabled_ = true;
    bool isGridVisible_ = true;
    bool isGhostVisible_ = true;
    bool isPreviewDragging_ = false;
    UIEditorVector2 previewDragOffset_ = { 0.0f, 0.0f };
    UIEditorTrackGroup selectedGroup_ = UIEditorTrackGroup::Position;
    UIEditorInterpolation defaultInterpolation_ = UIEditorInterpolation::EaseInOut;
    int selectedFrame_ = -1;

    char animationNameBuffer_[128] = {};
};
