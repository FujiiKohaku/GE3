#include "UIAnimationEditorApp.h"
#include "TextureLoader.h"
#include "externals/imgui/imgui.h"
#include <Windows.h>
#include <cmath>
#include <commdlg.h>
#include <filesystem>
#include <shobjidl.h>
#include <vector>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

UIAnimationEditorApp::UIAnimationEditorApp()
{
}

void UIAnimationEditorApp::Initialize(HWND hwnd, UIEditorD3D12* d3d12)
{
    hwnd_ = hwnd;
    d3d12_ = d3d12;
    NewAnimation();
}

void UIAnimationEditorApp::Update(float deltaTime)
{
    if (!isPlaying_) {
        return;
    }

    if (playbackSpeed_ < 0.1f) {
        playbackSpeed_ = 0.1f;
    }
    if (playbackSpeed_ > 4.0f) {
        playbackSpeed_ = 4.0f;
    }

    playbackFrame_ += deltaTime * 60.0f * playbackSpeed_;

    if (playbackFrame_ > static_cast<float>(clip_.GetLength())) {
        if (isLoopEnabled_) {
            playbackFrame_ = 0.0f;
        } else {
            playbackFrame_ = static_cast<float>(clip_.GetLength());
            isPlaying_ = false;
        }
    }

    currentFrame_ = static_cast<int>(playbackFrame_ + 0.5f);
    ClampCurrentFrame();
    EvaluatePreviewFromTimeline();
}

void UIAnimationEditorApp::Draw()
{
    DrawMainMenu();
    DrawTopLayout();
}

void UIAnimationEditorApp::OnDropFile(const std::wstring& filePath)
{
    LoadTextureFile(filePath);
}

void UIAnimationEditorApp::NewAnimation()
{
    clip_ = UIEditorAnimationClip();
    clip_.SetName("TitleOpen");
    clip_.SetCanvasSize(1280, 720);
    clip_.SetLength(60);

    previewState_ = clip_.GetBaseState();
    previewState_.position.x = static_cast<float>(clip_.GetCanvasWidth()) * 0.5f;
    previewState_.position.y = static_cast<float>(clip_.GetCanvasHeight()) * 0.5f;
    clip_.SetBaseState(previewState_);

    currentFrame_ = 0;
    playbackFrame_ = 0.0f;
    selectedFrame_ = -1;
    selectedGroup_ = UIEditorTrackGroup::Position;
    isPlaying_ = false;
    isAutoKeyEnabled_ = true;
    projectFilePath_.clear();
    RefreshNameBuffer();
    statusMessage_ = "New animation. Drop PNG or choose Texture.";
}

void UIAnimationEditorApp::LoadTextureFile(const std::wstring& filePath)
{
    if (d3d12_ == nullptr) {
        return;
    }

    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;

    if (!UIEditorTextureLoader::LoadPngRGBA(filePath, &pixels, &width, &height)) {
        statusMessage_ = "Texture load failed.";
        return;
    }

    UIEditorTextureResource textureResource;

    if (!d3d12_->CreateTextureFromRGBA(pixels.data(), width, height, &textureResource)) {
        statusMessage_ = "Texture upload failed.";
        return;
    }

    texture_ = textureResource;
    clip_.SetTexturePath(UIEditorTextureLoader::WideToUtf8(filePath));
    previewState_.scale.x = static_cast<float>(width);
    previewState_.scale.y = static_cast<float>(height);
    clip_.SetBaseState(previewState_);
    statusMessage_ = "Texture loaded. Drag it in Preview to add keys.";
}

void UIAnimationEditorApp::OpenTextureDialog()
{
    std::wstring filePath = OpenFileDialog(L"PNG Texture\0*.png\0All Files\0*.*\0");

    if (!filePath.empty()) {
        LoadTextureFile(filePath);
    }
}

void UIAnimationEditorApp::OpenAnimationDialog()
{
    std::wstring filePath = OpenFileDialog(L"Animation JSON\0*.json\0All Files\0*.*\0");

    if (filePath.empty()) {
        return;
    }

    std::string path = UIEditorTextureLoader::WideToUtf8(filePath);

    if (!clip_.LoadProjectFile(path)) {
        statusMessage_ = "Open animation failed.";
        return;
    }

    projectFilePath_ = path;
    currentFrame_ = 0;
    playbackFrame_ = 0.0f;
    selectedFrame_ = -1;
    RefreshNameBuffer();
    EvaluatePreviewFromTimeline();

    if (!clip_.GetTexturePath().empty()) {
        LoadTextureFile(UIEditorTextureLoader::Utf8ToWide(clip_.GetTexturePath()));
    }

    statusMessage_ = "Animation opened.";
}

void UIAnimationEditorApp::SaveAnimation()
{
    if (projectFilePath_.empty()) {
        SaveAnimationAs();
        return;
    }

    if (clip_.SaveProjectFile(projectFilePath_)) {
        statusMessage_ = "Saved.";
    } else {
        statusMessage_ = "Save failed.";
    }
}

void UIAnimationEditorApp::SaveAnimationAs()
{
    std::wstring filePath = SaveFileDialog(L"Animation JSON\0*.json\0All Files\0*.*\0", L"json");

    if (filePath.empty()) {
        return;
    }

    projectFilePath_ = UIEditorTextureLoader::WideToUtf8(filePath);
    SaveAnimation();
}

void UIAnimationEditorApp::ExportJson()
{
    if (exportFolderPath_.empty()) {
        PickExportFolder();
    }

    if (exportFolderPath_.empty()) {
        return;
    }

    std::filesystem::path exportPath(exportFolderPath_);
    exportPath /= clip_.GetName() + ".json";

    if (clip_.ExportRuntimeJson(exportPath.string())) {
        statusMessage_ = "Exported: " + exportPath.string();
    } else {
        statusMessage_ = "Export failed.";
    }
}

void UIAnimationEditorApp::PickExportFolder()
{
    std::wstring folderPath = PickFolderDialog();

    if (!folderPath.empty()) {
        exportFolderPath_ = UIEditorTextureLoader::WideToUtf8(folderPath);
    }
}

void UIAnimationEditorApp::DrawMainMenu()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar;

    ImGui::Begin("UIAnimationEditorRoot", nullptr, flags);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button("New Animation")) {
            NewAnimation();
        }

        if (ImGui::Button("Open Animation")) {
            OpenAnimationDialog();
        }

        if (ImGui::Button("Texture")) {
            OpenTextureDialog();
        }

        if (ImGui::Button("Save")) {
            SaveAnimation();
        }

        if (ImGui::Button("Save As")) {
            SaveAnimationAs();
        }

        if (ImGui::Button("Export JSON")) {
            ExportJson();
        }

        if (ImGui::Button("Export Folder")) {
            PickExportFolder();
        }

        ImGui::Text("%s", statusMessage_.c_str());
        ImGui::EndMenuBar();
    }
}

void UIAnimationEditorApp::DrawTopLayout()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float timelineHeight = 210.0f;
    float topHeight = availableSize.y - timelineHeight;

    if (topHeight < 300.0f) {
        topHeight = 300.0f;
    }

    if (ImGui::BeginChild("TopArea", ImVec2(0.0f, topHeight), true)) {
        if (ImGui::BeginTable("TopLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Animations", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 300.0f);

            ImGui::TableNextColumn();
            DrawAnimationListPanel();

            ImGui::TableNextColumn();
            DrawPreviewPanel();

            ImGui::TableNextColumn();
            DrawInspectorPanel();

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();

    if (ImGui::BeginChild("TimelineArea", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        DrawTimelinePanel();
    }

    ImGui::EndChild();
    ImGui::End();
}

void UIAnimationEditorApp::DrawAnimationListPanel()
{
    ImGui::Text("Animations");
    ImGui::Separator();
    ImGui::Selectable(clip_.GetName().c_str(), true);

    ImGui::Spacing();
    ImGui::InputText("Name", animationNameBuffer_, sizeof(animationNameBuffer_));

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        clip_.SetName(animationNameBuffer_);
        RefreshNameBuffer();
    }

    ImGui::Separator();

    if (ImGui::Button("1280 x 720", ImVec2(-1.0f, 0.0f))) {
        clip_.SetCanvasSize(1280, 720);
        EvaluatePreviewFromTimeline();
    }

    if (ImGui::Button("1920 x 1080", ImVec2(-1.0f, 0.0f))) {
        clip_.SetCanvasSize(1920, 1080);
        EvaluatePreviewFromTimeline();
    }

    ImGui::Separator();
    ImGui::Text("Canvas: %d x %d", clip_.GetCanvasWidth(), clip_.GetCanvasHeight());
    ImGui::Text("Length: %d frames", clip_.GetLength());

    if (!exportFolderPath_.empty()) {
        ImGui::TextWrapped("Export: %s", exportFolderPath_.c_str());
    }
}

void UIAnimationEditorApp::DrawPreviewPanel()
{
    ImGui::Text("Preview");
    ImGui::Separator();

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    ImVec2 canvasAvailableSize = availableSize;
    canvasAvailableSize.y -= 28.0f;

    if (canvasAvailableSize.y < 120.0f) {
        canvasAvailableSize.y = 120.0f;
    }

    float scaleX = canvasAvailableSize.x / static_cast<float>(clip_.GetCanvasWidth());
    float scaleY = canvasAvailableSize.y / static_cast<float>(clip_.GetCanvasHeight());
    float canvasScale = scaleX;

    if (scaleY < canvasScale) {
        canvasScale = scaleY;
    }
    if (canvasScale <= 0.0f) {
        canvasScale = 1.0f;
    }

    ImVec2 canvasSize;
    canvasSize.x = static_cast<float>(clip_.GetCanvasWidth()) * canvasScale;
    canvasSize.y = static_cast<float>(clip_.GetCanvasHeight()) * canvasScale;

    ImVec2 cursorPosition = ImGui::GetCursorScreenPos();
    ImVec2 canvasPosition;
    canvasPosition.x = cursorPosition.x + (canvasAvailableSize.x - canvasSize.x) * 0.5f;
    canvasPosition.y = cursorPosition.y + 8.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 canvasColor = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.45f, 0.45f, 0.48f, 1.0f));

    drawList->AddRectFilled(canvasPosition, ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y), canvasColor);
    drawList->AddRect(canvasPosition, ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y), borderColor);

    if (isGridVisible_) {
        DrawPreviewGrid(canvasPosition, canvasSize, canvasScale);
    }

    ImGui::SetCursorScreenPos(canvasPosition);
    ImGui::InvisibleButton("PreviewCanvas", canvasSize);

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGuiIO& io = ImGui::GetIO();

        if (IsMouseInsidePreviewObject(canvasPosition, canvasScale, io.MousePos)) {
            UIEditorVector2 canvasMousePosition = ScreenToCanvasPosition(canvasPosition, canvasScale, io.MousePos);
            isPreviewDragging_ = true;
            previewDragOffset_.x = previewState_.position.x - canvasMousePosition.x;
            previewDragOffset_.y = previewState_.position.y - canvasMousePosition.y;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        isPreviewDragging_ = false;
    }

    if (isPreviewDragging_) {
        ImGuiIO& io = ImGui::GetIO();
        UIEditorVector2 canvasMousePosition = ScreenToCanvasPosition(canvasPosition, canvasScale, io.MousePos);
        previewState_.position.x = canvasMousePosition.x + previewDragOffset_.x;
        previewState_.position.y = canvasMousePosition.y + previewDragOffset_.y;

        if (isAutoKeyEnabled_) {
            AddKeyForProperty(UIEditorProperty::PositionX, currentFrame_, previewState_.position.x);
            AddKeyForProperty(UIEditorProperty::PositionY, currentFrame_, previewState_.position.y);
            selectedGroup_ = UIEditorTrackGroup::Position;
            selectedFrame_ = currentFrame_;
        }
    }

    if (!texture_.isValid) {
        ImVec2 textSize = ImGui::CalcTextSize("Drop PNG here");
        ImVec2 textPosition;
        textPosition.x = canvasPosition.x + canvasSize.x * 0.5f - textSize.x * 0.5f;
        textPosition.y = canvasPosition.y + canvasSize.y * 0.5f - textSize.y * 0.5f;
        drawList->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), "Drop PNG here");
    } else {
        if (isGhostVisible_) {
            int previousFrame = currentFrame_ - ghostFrameOffset_;
            int nextFrame = currentFrame_ + ghostFrameOffset_;

            if (previousFrame >= 0) {
                UIEditorObjectState previousState = clip_.Evaluate(static_cast<float>(previousFrame));
                DrawPreviewObjectState(canvasPosition, canvasScale, previousState, ImGui::GetColorU32(ImVec4(0.40f, 0.70f, 1.0f, 0.60f)), 0.28f);
            }
            if (nextFrame <= clip_.GetLength()) {
                UIEditorObjectState nextState = clip_.Evaluate(static_cast<float>(nextFrame));
                DrawPreviewObjectState(canvasPosition, canvasScale, nextState, ImGui::GetColorU32(ImVec4(1.0f, 0.55f, 0.25f, 0.60f)), 0.28f);
            }
        }

        DrawPreviewObject(canvasPosition, canvasScale);
    }

    ImGui::SetCursorScreenPos(ImVec2(cursorPosition.x, canvasPosition.y + canvasSize.y + 8.0f));
    ImGui::Text("Frame %d / %d", currentFrame_, clip_.GetLength());
}

void UIAnimationEditorApp::DrawInspectorPanel()
{
    ImGui::Text("Selected Object");
    ImGui::Separator();

    ImGui::Checkbox("Auto Key", &isAutoKeyEnabled_);
    ImGui::Checkbox("Loop", &isLoopEnabled_);
    ImGui::Checkbox("Grid", &isGridVisible_);
    ImGui::Checkbox("Ghost", &isGhostVisible_);

    if (isGhostVisible_) {
        ImGui::DragInt("Ghost Offset", &ghostFrameOffset_, 1.0f, 1, 120);
    }

    DrawInterpolationCombo("New Key Easing", &defaultInterpolation_);
    ImGui::DragFloat("Playback Speed", &playbackSpeed_, 0.05f, 0.1f, 4.0f, "%.2fx");
    ImGui::DragFloat("Timeline Zoom", &timelineZoom_, 0.05f, 0.5f, 6.0f, "%.2fx");

    int length = clip_.GetLength();

    if (ImGui::DragInt("Length", &length, 1.0f, 1, 600)) {
        clip_.SetLength(length);
        ClampCurrentFrame();
    }

    if (aiAnimationWindow_.Draw(&clip_, &statusMessage_)) {
        currentFrame_ = 0;
        playbackFrame_ = 0.0f;
        selectedFrame_ = -1;
        isPlaying_ = false;
        RefreshNameBuffer();
        EvaluatePreviewFromTimeline();
    }

    ImGui::Separator();

    if (ImGui::DragFloat2("Position", &previewState_.position.x, 1.0f)) {
        if (isAutoKeyEnabled_) {
            AddKeyForProperty(UIEditorProperty::PositionX, currentFrame_, previewState_.position.x);
            AddKeyForProperty(UIEditorProperty::PositionY, currentFrame_, previewState_.position.y);
            selectedGroup_ = UIEditorTrackGroup::Position;
            selectedFrame_ = currentFrame_;
        }
    }

    if (ImGui::DragFloat2("Scale", &previewState_.scale.x, 1.0f, 1.0f, 4096.0f)) {
        if (isAutoKeyEnabled_) {
            AddKeyForProperty(UIEditorProperty::ScaleX, currentFrame_, previewState_.scale.x);
            AddKeyForProperty(UIEditorProperty::ScaleY, currentFrame_, previewState_.scale.y);
            selectedGroup_ = UIEditorTrackGroup::Scale;
            selectedFrame_ = currentFrame_;
        }
    }

    if (ImGui::DragFloat("Rotation", &previewState_.rotation, 0.01f)) {
        if (isAutoKeyEnabled_) {
            AddKeyForProperty(UIEditorProperty::Rotation, currentFrame_, previewState_.rotation);
            selectedGroup_ = UIEditorTrackGroup::Rotation;
            selectedFrame_ = currentFrame_;
        }
    }

    if (ImGui::ColorEdit4("Color", &previewState_.color.r)) {
        if (isAutoKeyEnabled_) {
            AddKeyForProperty(UIEditorProperty::ColorR, currentFrame_, previewState_.color.r);
            AddKeyForProperty(UIEditorProperty::ColorG, currentFrame_, previewState_.color.g);
            AddKeyForProperty(UIEditorProperty::ColorB, currentFrame_, previewState_.color.b);
            AddKeyForProperty(UIEditorProperty::ColorA, currentFrame_, previewState_.color.a);
            selectedGroup_ = UIEditorTrackGroup::Color;
            selectedFrame_ = currentFrame_;
        }
    }

    if (ImGui::Button("Center Object", ImVec2(-1.0f, 0.0f))) {
        CenterPreviewObject();
    }

    if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f))) {
        ResetPreviewObject();
    }

    if (ImGui::Button("Key Transform", ImVec2(-1.0f, 0.0f))) {
        AddKeyForCurrentTransform();
    }

    if (ImGui::Button("Key Color", ImVec2(-1.0f, 0.0f))) {
        AddKeyForGroup(UIEditorTrackGroup::Color);
        AddKeyForGroup(UIEditorTrackGroup::Alpha);
    }

    if (selectedFrame_ >= 0) {
        ImGui::Separator();
        ImGui::Text("Selected Key");
        ImGui::Text("%s / Frame %d", UIEditorTrackGroupToString(selectedGroup_), selectedFrame_);

        int editedFrame = selectedFrame_;

        if (ImGui::DragInt("Key Frame", &editedFrame, 1.0f, 0, clip_.GetLength())) {
            if (editedFrame != selectedFrame_) {
                MoveGroupKeys(selectedGroup_, selectedFrame_, editedFrame);
                selectedFrame_ = editedFrame;
                SetCurrentFrame(editedFrame);
            }
        }

        UIEditorInterpolation selectedInterpolation = GetInterpolationForGroup(selectedGroup_, selectedFrame_);

        if (DrawInterpolationCombo("Key Easing", &selectedInterpolation)) {
            SetInterpolationForGroup(selectedGroup_, selectedFrame_, selectedInterpolation);
        }

        ImGui::DragInt("Duplicate Offset", &duplicateFrameOffset_, 1.0f, 1, 120);

        if (ImGui::Button("Duplicate Selected Key", ImVec2(-1.0f, 0.0f))) {
            DuplicateSelectedKey();
        }

        if (ImGui::Button("Delete Selected Key", ImVec2(-1.0f, 0.0f))) {
            DeleteSelectedKey();
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Clear All Keys", ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("ClearAllKeysPopup");
    }

    if (ImGui::BeginPopupModal("ClearAllKeysPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete every key in this animation?");

        if (ImGui::Button("Clear", ImVec2(120.0f, 0.0f))) {
            clip_.ClearKeys();
            selectedFrame_ = -1;
            isPlaying_ = false;
            EvaluatePreviewFromTimeline();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void UIAnimationEditorApp::DrawTimelinePanel()
{
    if (ImGui::Button("|<")) {
        isPlaying_ = false;
        SetCurrentFrame(0);
    }

    ImGui::SameLine();

    if (isPlaying_) {
        if (ImGui::Button("Stop")) {
            isPlaying_ = false;
        }
    } else {
        if (ImGui::Button("Play")) {
            playbackFrame_ = static_cast<float>(currentFrame_);
            isPlaying_ = true;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(">|")) {
        isPlaying_ = false;
        SetCurrentFrame(clip_.GetLength());
    }

    ImGui::SameLine();
    ImGui::Text("Frame %d", currentFrame_);

    if (ImGui::SliderInt("##TimelineFrame", &currentFrame_, 0, clip_.GetLength())) {
        isPlaying_ = false;
        SetCurrentFrame(currentFrame_);
    }

    ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
    float labelWidth = 110.0f;
    float rowHeight = 26.0f;
    float rulerHeight = 24.0f;
    if (timelineZoom_ < 0.5f) {
        timelineZoom_ = 0.5f;
    }
    if (timelineZoom_ > 6.0f) {
        timelineZoom_ = 6.0f;
    }

    float width = (ImGui::GetContentRegionAvail().x - labelWidth) * timelineZoom_;

    if (width < 260.0f) {
        width = 260.0f;
    }

    float startX = canvasPosition.x + labelWidth;
    float startY = canvasPosition.y + rulerHeight;
    float totalHeight = rulerHeight + rowHeight * static_cast<float>(UIEditorTrackGroupCount());
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPosition, ImVec2(startX + width, canvasPosition.y + totalHeight), ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.13f, 1.0f)));

    int tickCount = 6;

    if (clip_.GetLength() < tickCount) {
        tickCount = clip_.GetLength();
    }
    if (tickCount < 1) {
        tickCount = 1;
    }

    for (int tickIndex = 0; tickIndex <= tickCount; ++tickIndex) {
        float rate = static_cast<float>(tickIndex) / static_cast<float>(tickCount);
        int frame = static_cast<int>(static_cast<float>(clip_.GetLength()) * rate + 0.5f);
        float tickX = startX + width * rate;
        drawList->AddLine(ImVec2(tickX, canvasPosition.y), ImVec2(tickX, canvasPosition.y + totalHeight), ImGui::GetColorU32(ImVec4(0.28f, 0.28f, 0.30f, 1.0f)));

        char text[32] = {};
        std::snprintf(text, sizeof(text), "%d", frame);
        drawList->AddText(ImVec2(tickX + 3.0f, canvasPosition.y + 3.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    }

    ImGui::SetCursorScreenPos(ImVec2(startX, canvasPosition.y));
    ImGui::InvisibleButton("TimelineSeek", ImVec2(width, totalHeight));

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGuiIO& io = ImGui::GetIO();
        SetCurrentFrame(GetFrameFromTimelineX(io.MousePos.x, startX, width));
        isPlaying_ = false;
    }

    for (int groupIndex = 0; groupIndex < UIEditorTrackGroupCount(); ++groupIndex) {
        UIEditorTrackGroup group = UIEditorTrackGroupFromIndex(groupIndex);
        DrawTimelineGroup(group, startX, startY + rowHeight * static_cast<float>(groupIndex), width, rowHeight);
    }

    float playheadX = GetTimelineXFromFrame(currentFrame_, startX, width);
    drawList->AddLine(ImVec2(playheadX, canvasPosition.y), ImVec2(playheadX, canvasPosition.y + totalHeight), ImGui::GetColorU32(ImVec4(1.0f, 0.25f, 0.18f, 1.0f)), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(canvasPosition.x, canvasPosition.y + totalHeight + 4.0f));
}

void UIAnimationEditorApp::DrawPreviewObject(const ImVec2& canvasPosition, float canvasScale)
{
    DrawPreviewObjectState(canvasPosition, canvasScale, previewState_, ImGui::GetColorU32(ImVec4(0.2f, 0.62f, 1.0f, 1.0f)), 1.0f);
}

void UIAnimationEditorApp::DrawPreviewObjectState(const ImVec2& canvasPosition, float canvasScale, const UIEditorObjectState& state, unsigned int outlineColor, float alphaScale)
{
    if (!texture_.isValid) {
        return;
    }

    float centerX = canvasPosition.x + state.position.x * canvasScale;
    float centerY = canvasPosition.y + state.position.y * canvasScale;
    float halfWidth = state.scale.x * canvasScale * 0.5f;
    float halfHeight = state.scale.y * canvasScale * 0.5f;
    float cosValue = std::cos(state.rotation);
    float sinValue = std::sin(state.rotation);

    UIEditorVector2 corners[4];
    corners[0] = { -halfWidth, -halfHeight };
    corners[1] = { halfWidth, -halfHeight };
    corners[2] = { halfWidth, halfHeight };
    corners[3] = { -halfWidth, halfHeight };

    ImVec2 points[4];

    for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
        float rotatedX = corners[cornerIndex].x * cosValue - corners[cornerIndex].y * sinValue;
        float rotatedY = corners[cornerIndex].x * sinValue + corners[cornerIndex].y * cosValue;
        points[cornerIndex] = ImVec2(centerX + rotatedX, centerY + rotatedY);
    }

    ImTextureID textureId = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(texture_.gpuHandle.ptr));
    float alpha = state.color.a * alphaScale;

    if (alpha < 0.0f) {
        alpha = 0.0f;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }

    ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(state.color.r, state.color.g, state.color.b, alpha));
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddImageQuad(
        textureId,
        points[0],
        points[1],
        points[2],
        points[3],
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        ImVec2(0.0f, 1.0f),
        tint);

    drawList->AddPolyline(points, 4, outlineColor, ImDrawFlags_Closed, 2.0f);
}

void UIAnimationEditorApp::DrawPreviewGrid(const ImVec2& canvasPosition, const ImVec2& canvasSize, float canvasScale)
{
    if (canvasScale <= 0.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.24f, 0.55f));
    ImU32 centerColor = ImGui::GetColorU32(ImVec4(0.35f, 0.45f, 0.55f, 0.75f));
    float gridStep = 100.0f;

    for (float gridX = gridStep; gridX < static_cast<float>(clip_.GetCanvasWidth()); gridX += gridStep) {
        float screenX = canvasPosition.x + gridX * canvasScale;
        drawList->AddLine(ImVec2(screenX, canvasPosition.y), ImVec2(screenX, canvasPosition.y + canvasSize.y), gridColor);
    }

    for (float gridY = gridStep; gridY < static_cast<float>(clip_.GetCanvasHeight()); gridY += gridStep) {
        float screenY = canvasPosition.y + gridY * canvasScale;
        drawList->AddLine(ImVec2(canvasPosition.x, screenY), ImVec2(canvasPosition.x + canvasSize.x, screenY), gridColor);
    }

    float centerX = canvasPosition.x + static_cast<float>(clip_.GetCanvasWidth()) * canvasScale * 0.5f;
    float centerY = canvasPosition.y + static_cast<float>(clip_.GetCanvasHeight()) * canvasScale * 0.5f;
    drawList->AddLine(ImVec2(centerX, canvasPosition.y), ImVec2(centerX, canvasPosition.y + canvasSize.y), centerColor, 2.0f);
    drawList->AddLine(ImVec2(canvasPosition.x, centerY), ImVec2(canvasPosition.x + canvasSize.x, centerY), centerColor, 2.0f);
}

UIEditorVector2 UIAnimationEditorApp::ScreenToCanvasPosition(const ImVec2& canvasPosition, float canvasScale, const ImVec2& screenPosition) const
{
    UIEditorVector2 canvasPositionResult = { 0.0f, 0.0f };

    if (canvasScale <= 0.0f) {
        return canvasPositionResult;
    }

    canvasPositionResult.x = (screenPosition.x - canvasPosition.x) / canvasScale;
    canvasPositionResult.y = (screenPosition.y - canvasPosition.y) / canvasScale;

    return canvasPositionResult;
}

bool UIAnimationEditorApp::IsMouseInsidePreviewObject(const ImVec2& canvasPosition, float canvasScale, const ImVec2& mousePosition) const
{
    if (!texture_.isValid) {
        return false;
    }
    if (canvasScale <= 0.0f) {
        return false;
    }

    UIEditorVector2 canvasMousePosition = ScreenToCanvasPosition(canvasPosition, canvasScale, mousePosition);
    float deltaX = canvasMousePosition.x - previewState_.position.x;
    float deltaY = canvasMousePosition.y - previewState_.position.y;

    // Convert the mouse point into object local space so rotated images hit-test correctly.
    float cosValue = std::cos(-previewState_.rotation);
    float sinValue = std::sin(-previewState_.rotation);
    float localX = deltaX * cosValue - deltaY * sinValue;
    float localY = deltaX * sinValue + deltaY * cosValue;
    float halfWidth = previewState_.scale.x * 0.5f;
    float halfHeight = previewState_.scale.y * 0.5f;

    if (halfWidth < 0.0f) {
        halfWidth = -halfWidth;
    }
    if (halfHeight < 0.0f) {
        halfHeight = -halfHeight;
    }

    if (localX < -halfWidth) {
        return false;
    }
    if (localX > halfWidth) {
        return false;
    }
    if (localY < -halfHeight) {
        return false;
    }
    if (localY > halfHeight) {
        return false;
    }

    return true;
}

void UIAnimationEditorApp::DrawTimelineGroup(UIEditorTrackGroup group, float startX, float startY, float width, float rowHeight)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float labelX = startX - 104.0f;
    drawList->AddText(ImVec2(labelX, startY + 5.0f), ImGui::GetColorU32(ImGuiCol_Text), UIEditorTrackGroupToString(group));
    drawList->AddLine(ImVec2(startX, startY + rowHeight), ImVec2(startX + width, startY + rowHeight), ImGui::GetColorU32(ImVec4(0.22f, 0.22f, 0.24f, 1.0f)));

    ImGui::SetCursorScreenPos(ImVec2(startX - 42.0f, startY + 3.0f));
    ImGui::PushID(static_cast<int>(group));

    if (ImGui::SmallButton("+")) {
        AddKeyForGroup(group);
    }

    ImGui::PopID();

    std::vector<int> frames;
    const std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        if (!UIEditorPropertyBelongsToGroup(tracks[trackIndex].GetProperty(), group)) {
            continue;
        }

        const std::vector<UIEditorKeyFrame>& keys = tracks[trackIndex].GetKeys();

        for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            int frame = keys[keyIndex].GetFrame();
            bool exists = false;

            for (std::size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
                if (frames[frameIndex] == frame) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                frames.push_back(frame);
            }
        }
    }

    for (std::size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
        int frame = frames[frameIndex];
        float keyX = GetTimelineXFromFrame(frame, startX, width);
        float keyY = startY + rowHeight * 0.5f;
        bool isSelected = false;

        if (selectedGroup_ == group && selectedFrame_ == frame) {
            isSelected = true;
        }

        UIEditorInterpolation keyInterpolation = GetInterpolationForGroup(group, frame);
        ImU32 keyColor = GetInterpolationColor(keyInterpolation, isSelected);

        ImVec2 points[4];
        points[0] = ImVec2(keyX, keyY - 7.0f);
        points[1] = ImVec2(keyX + 7.0f, keyY);
        points[2] = ImVec2(keyX, keyY + 7.0f);
        points[3] = ImVec2(keyX - 7.0f, keyY);
        drawList->AddConvexPolyFilled(points, 4, keyColor);

        ImGui::SetCursorScreenPos(ImVec2(keyX - 10.0f, keyY - 10.0f));
        ImGui::PushID(static_cast<int>(group) * 10000 + frame);
        ImGui::InvisibleButton("Key", ImVec2(20.0f, 20.0f));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedGroup_ = group;
            selectedFrame_ = frame;
            SetCurrentFrame(frame);
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            ImGuiIO& io = ImGui::GetIO();
            int newFrame = GetFrameFromTimelineX(io.MousePos.x, startX, width);

            if (newFrame != selectedFrame_) {
                MoveGroupKeys(group, selectedFrame_, newFrame);
                selectedFrame_ = newFrame;
                SetCurrentFrame(newFrame);
            }
        }

        ImGui::PopID();
    }
}

bool UIAnimationEditorApp::DrawInterpolationCombo(const char* label, UIEditorInterpolation* interpolation)
{
    if (interpolation == nullptr) {
        return false;
    }

    const char* interpolationNames[] = {
        "Linear",
        "EaseIn",
        "EaseOut",
        "EaseInOut"
    };

    int interpolationIndex = UIEditorInterpolationToIndex(*interpolation);

    if (ImGui::Combo(label, &interpolationIndex, interpolationNames, UIEditorInterpolationCount())) {
        *interpolation = UIEditorInterpolationFromIndex(interpolationIndex);
        return true;
    }

    return false;
}

void UIAnimationEditorApp::AddKeyForProperty(UIEditorProperty property, int frame, float value)
{
    clip_.AddKey(property, frame, value, defaultInterpolation_);
}

void UIAnimationEditorApp::AddKeyForGroup(UIEditorTrackGroup group)
{
    AddKeyForGroupAtFrame(group, currentFrame_, previewState_);
}

void UIAnimationEditorApp::AddKeyForGroupAtFrame(UIEditorTrackGroup group, int frame, const UIEditorObjectState& state)
{
    if (group == UIEditorTrackGroup::Position) {
        AddKeyForProperty(UIEditorProperty::PositionX, frame, state.position.x);
        AddKeyForProperty(UIEditorProperty::PositionY, frame, state.position.y);
    }
    if (group == UIEditorTrackGroup::Scale) {
        AddKeyForProperty(UIEditorProperty::ScaleX, frame, state.scale.x);
        AddKeyForProperty(UIEditorProperty::ScaleY, frame, state.scale.y);
    }
    if (group == UIEditorTrackGroup::Rotation) {
        AddKeyForProperty(UIEditorProperty::Rotation, frame, state.rotation);
    }
    if (group == UIEditorTrackGroup::Color) {
        AddKeyForProperty(UIEditorProperty::ColorR, frame, state.color.r);
        AddKeyForProperty(UIEditorProperty::ColorG, frame, state.color.g);
        AddKeyForProperty(UIEditorProperty::ColorB, frame, state.color.b);
    }
    if (group == UIEditorTrackGroup::Alpha) {
        AddKeyForProperty(UIEditorProperty::ColorA, frame, state.color.a);
    }

    selectedGroup_ = group;
    selectedFrame_ = frame;
}

void UIAnimationEditorApp::AddKeyForCurrentTransform()
{
    AddKeyForGroup(UIEditorTrackGroup::Position);
    AddKeyForGroup(UIEditorTrackGroup::Scale);
    AddKeyForGroup(UIEditorTrackGroup::Rotation);
    selectedGroup_ = UIEditorTrackGroup::Position;
    selectedFrame_ = currentFrame_;
}

void UIAnimationEditorApp::DeleteSelectedKey()
{
    if (selectedFrame_ < 0) {
        return;
    }

    std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        if (UIEditorPropertyBelongsToGroup(tracks[trackIndex].GetProperty(), selectedGroup_)) {
            int keyIndex = tracks[trackIndex].FindKeyIndex(selectedFrame_);

            if (keyIndex >= 0) {
                tracks[trackIndex].RemoveKeyAtIndex(static_cast<std::size_t>(keyIndex));
            }
        }
    }

    selectedFrame_ = -1;
    EvaluatePreviewFromTimeline();
}

void UIAnimationEditorApp::DuplicateSelectedKey()
{
    if (selectedFrame_ < 0) {
        return;
    }

    int targetFrame = selectedFrame_ + duplicateFrameOffset_;

    if (targetFrame > clip_.GetLength()) {
        targetFrame = clip_.GetLength();
    }
    if (targetFrame < 0) {
        targetFrame = 0;
    }
    if (targetFrame == selectedFrame_) {
        return;
    }

    UIEditorObjectState sourceState = clip_.Evaluate(static_cast<float>(selectedFrame_));
    UIEditorInterpolation sourceInterpolation = GetInterpolationForGroup(selectedGroup_, selectedFrame_);
    UIEditorInterpolation previousDefaultInterpolation = defaultInterpolation_;
    defaultInterpolation_ = sourceInterpolation;
    AddKeyForGroupAtFrame(selectedGroup_, targetFrame, sourceState);
    SetInterpolationForGroup(selectedGroup_, targetFrame, sourceInterpolation);
    defaultInterpolation_ = previousDefaultInterpolation;
    SetCurrentFrame(targetFrame);
}

void UIAnimationEditorApp::MoveGroupKeys(UIEditorTrackGroup group, int oldFrame, int newFrame)
{
    if (newFrame < 0) {
        newFrame = 0;
    }
    if (newFrame > clip_.GetLength()) {
        newFrame = clip_.GetLength();
    }

    std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        if (!UIEditorPropertyBelongsToGroup(tracks[trackIndex].GetProperty(), group)) {
            continue;
        }

        std::vector<UIEditorKeyFrame>& keys = tracks[trackIndex].GetKeys();

        for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            if (keys[keyIndex].GetFrame() == oldFrame) {
                keys[keyIndex].SetFrame(newFrame);
                break;
            }
        }

        tracks[trackIndex].SortKeys();
    }
}

bool UIAnimationEditorApp::GroupHasKeyAtFrame(UIEditorTrackGroup group, int frame) const
{
    const std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        if (!UIEditorPropertyBelongsToGroup(tracks[trackIndex].GetProperty(), group)) {
            continue;
        }

        if (tracks[trackIndex].FindKeyIndex(frame) >= 0) {
            return true;
        }
    }

    return false;
}

void UIAnimationEditorApp::SetInterpolationForGroup(UIEditorTrackGroup group, int frame, UIEditorInterpolation interpolation)
{
    std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        UIEditorProperty property = tracks[trackIndex].GetProperty();

        if (UIEditorPropertyBelongsToGroup(property, group)) {
            clip_.SetKeyInterpolation(property, frame, interpolation);
        }
    }
}

UIEditorInterpolation UIAnimationEditorApp::GetInterpolationForGroup(UIEditorTrackGroup group, int frame) const
{
    const std::vector<UIEditorTrack>& tracks = clip_.GetTracks();

    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        UIEditorProperty property = tracks[trackIndex].GetProperty();

        if (UIEditorPropertyBelongsToGroup(property, group)) {
            if (tracks[trackIndex].FindKeyIndex(frame) >= 0) {
                return clip_.GetKeyInterpolation(property, frame, defaultInterpolation_);
            }
        }
    }

    return defaultInterpolation_;
}

void UIAnimationEditorApp::SetCurrentFrame(int frame)
{
    currentFrame_ = frame;
    ClampCurrentFrame();
    playbackFrame_ = static_cast<float>(currentFrame_);
    EvaluatePreviewFromTimeline();
}

void UIAnimationEditorApp::ClampCurrentFrame()
{
    if (currentFrame_ < 0) {
        currentFrame_ = 0;
    }
    if (currentFrame_ > clip_.GetLength()) {
        currentFrame_ = clip_.GetLength();
    }
    if (playbackFrame_ < 0.0f) {
        playbackFrame_ = 0.0f;
    }
    if (playbackFrame_ > static_cast<float>(clip_.GetLength())) {
        playbackFrame_ = static_cast<float>(clip_.GetLength());
    }
}

void UIAnimationEditorApp::CenterPreviewObject()
{
    previewState_.position.x = static_cast<float>(clip_.GetCanvasWidth()) * 0.5f;
    previewState_.position.y = static_cast<float>(clip_.GetCanvasHeight()) * 0.5f;

    if (isAutoKeyEnabled_) {
        AddKeyForGroup(UIEditorTrackGroup::Position);
    }
}

void UIAnimationEditorApp::ResetPreviewObject()
{
    previewState_.position.x = static_cast<float>(clip_.GetCanvasWidth()) * 0.5f;
    previewState_.position.y = static_cast<float>(clip_.GetCanvasHeight()) * 0.5f;

    if (texture_.isValid) {
        previewState_.scale.x = static_cast<float>(texture_.width);
        previewState_.scale.y = static_cast<float>(texture_.height);
    } else {
        previewState_.scale.x = 256.0f;
        previewState_.scale.y = 256.0f;
    }

    previewState_.rotation = 0.0f;

    if (isAutoKeyEnabled_) {
        AddKeyForCurrentTransform();
    }
}

void UIAnimationEditorApp::ApplyPreviewStateToBase()
{
    clip_.SetBaseState(previewState_);
}

void UIAnimationEditorApp::EvaluatePreviewFromTimeline()
{
    previewState_ = clip_.Evaluate(static_cast<float>(currentFrame_));
}

std::wstring UIAnimationEditorApp::OpenFileDialog(const wchar_t* filter)
{
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = hwnd_;
    openFileName.lpstrFile = fileName;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.lpstrFilter = filter;
    openFileName.nFilterIndex = 1;
    openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&openFileName)) {
        return fileName;
    }

    return L"";
}

std::wstring UIAnimationEditorApp::SaveFileDialog(const wchar_t* filter, const wchar_t* defaultExtension)
{
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW saveFileName = {};
    saveFileName.lStructSize = sizeof(saveFileName);
    saveFileName.hwndOwner = hwnd_;
    saveFileName.lpstrFile = fileName;
    saveFileName.nMaxFile = MAX_PATH;
    saveFileName.lpstrFilter = filter;
    saveFileName.nFilterIndex = 1;
    saveFileName.lpstrDefExt = defaultExtension;
    saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&saveFileName)) {
        return fileName;
    }

    return L"";
}

std::wstring UIAnimationEditorApp::PickFolderDialog()
{
    Microsoft::WRL::ComPtr<IFileDialog> fileDialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fileDialog));

    if (FAILED(result)) {
        return L"";
    }

    DWORD options = 0;
    fileDialog->GetOptions(&options);
    fileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    result = fileDialog->Show(hwnd_);

    if (FAILED(result)) {
        return L"";
    }

    Microsoft::WRL::ComPtr<IShellItem> shellItem;
    result = fileDialog->GetResult(&shellItem);

    if (FAILED(result)) {
        return L"";
    }

    PWSTR folderPath = nullptr;
    result = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPath);

    if (FAILED(result)) {
        return L"";
    }

    std::wstring resultPath = folderPath;
    CoTaskMemFree(folderPath);

    return resultPath;
}

int UIAnimationEditorApp::GetFrameFromTimelineX(float mouseX, float startX, float width) const
{
    if (width <= 0.0f) {
        return 0;
    }

    float rate = (mouseX - startX) / width;

    if (rate < 0.0f) {
        rate = 0.0f;
    }
    if (rate > 1.0f) {
        rate = 1.0f;
    }

    return static_cast<int>(static_cast<float>(clip_.GetLength()) * rate + 0.5f);
}

float UIAnimationEditorApp::GetTimelineXFromFrame(int frame, float startX, float width) const
{
    float rate = 0.0f;

    if (clip_.GetLength() > 0) {
        rate = static_cast<float>(frame) / static_cast<float>(clip_.GetLength());
    }

    if (rate < 0.0f) {
        rate = 0.0f;
    }
    if (rate > 1.0f) {
        rate = 1.0f;
    }

    return startX + width * rate;
}

unsigned int UIAnimationEditorApp::GetInterpolationColor(UIEditorInterpolation interpolation, bool isSelected) const
{
    if (isSelected) {
        return ImGui::GetColorU32(ImVec4(0.2f, 0.62f, 1.0f, 1.0f));
    }
    if (interpolation == UIEditorInterpolation::EaseIn) {
        return ImGui::GetColorU32(ImVec4(0.48f, 0.85f, 0.46f, 1.0f));
    }
    if (interpolation == UIEditorInterpolation::EaseOut) {
        return ImGui::GetColorU32(ImVec4(1.0f, 0.62f, 0.30f, 1.0f));
    }
    if (interpolation == UIEditorInterpolation::EaseInOut) {
        return ImGui::GetColorU32(ImVec4(0.83f, 0.56f, 1.0f, 1.0f));
    }

    return ImGui::GetColorU32(ImVec4(1.0f, 0.76f, 0.22f, 1.0f));
}

void UIAnimationEditorApp::CopyTextToBuffer(char* buffer, int bufferSize, const std::string& text)
{
    if (buffer == nullptr) {
        return;
    }
    if (bufferSize <= 0) {
        return;
    }

    errno_t result = strncpy_s(buffer, static_cast<std::size_t>(bufferSize), text.c_str(), _TRUNCATE);

    if (result != 0) {
        buffer[0] = '\0';
    }
}

void UIAnimationEditorApp::RefreshNameBuffer()
{
    CopyTextToBuffer(animationNameBuffer_, sizeof(animationNameBuffer_), clip_.GetName());
}
