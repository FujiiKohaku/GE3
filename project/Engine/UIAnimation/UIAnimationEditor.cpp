#include "UIAnimationEditor.h"
#include "Engine/2D/Sprite.h"
#include <cstdio>
#include <cstring>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

UIAnimationEditor::UIAnimationEditor()
{
}

void UIAnimationEditor::Initialize()
{
    clips_.clear();
    CreateDefaultClip();
    SelectClip(0);

    CopyTextToBuffer(filePathBuffer_, sizeof(filePathBuffer_), "resources/UIAnimations/TitleOpen.json");
}

void UIAnimationEditor::Update(float deltaTime)
{
    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        return;
    }

    player_.Update(deltaTime);

    if (player_.IsPlaying()) {
        currentFrame_ = player_.GetCurrentFrameInt();
    } else {
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    ClampCurrentFrame(clip);

    if (previewSprite_ != nullptr) {
        player_.ApplyToSprite(previewSprite_);
    }
}

void UIAnimationEditor::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(980.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("UI Animation Editor")) {
        if (ImGui::BeginTable("UIAnimationEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Clips", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 260.0f);

            ImGui::TableNextColumn();
            DrawClipList();

            ImGui::TableNextColumn();
            DrawTimeline();

            ImGui::TableNextColumn();
            DrawSelectedKeyFrame();

            ImGui::EndTable();
        }
    }

    ImGui::End();
#endif
}

void UIAnimationEditor::SetPreviewSprite(Sprite* sprite)
{
    previewSprite_ = sprite;
}

UIAnimationClip* UIAnimationEditor::GetSelectedClip()
{
    if (selectedClipIndex_ < 0) {
        return nullptr;
    }
    if (selectedClipIndex_ >= static_cast<int>(clips_.size())) {
        return nullptr;
    }

    return &clips_[static_cast<std::size_t>(selectedClipIndex_)];
}

const UIAnimationClip* UIAnimationEditor::GetSelectedClip() const
{
    if (selectedClipIndex_ < 0) {
        return nullptr;
    }
    if (selectedClipIndex_ >= static_cast<int>(clips_.size())) {
        return nullptr;
    }

    return &clips_[static_cast<std::size_t>(selectedClipIndex_)];
}

void UIAnimationEditor::CreateDefaultClip()
{
    UIAnimationClip clip;

    clip.SetName("TitleOpen");
    clip.SetLength(60);
    clip.AddKeyFrame(UIAnimationProperty::PositionX, 0, 220.0f);
    clip.AddKeyFrame(UIAnimationProperty::PositionX, 60, 620.0f);
    clip.AddKeyFrame(UIAnimationProperty::PositionY, 0, 240.0f);
    clip.AddKeyFrame(UIAnimationProperty::PositionY, 60, 240.0f);
    clip.AddKeyFrame(UIAnimationProperty::ScaleX, 0, 96.0f);
    clip.AddKeyFrame(UIAnimationProperty::ScaleX, 60, 160.0f);
    clip.AddKeyFrame(UIAnimationProperty::ScaleY, 0, 96.0f);
    clip.AddKeyFrame(UIAnimationProperty::ScaleY, 60, 160.0f);

    clips_.push_back(clip);
}

void UIAnimationEditor::SelectClip(int clipIndex)
{
    if (clipIndex < 0) {
        return;
    }
    if (clipIndex >= static_cast<int>(clips_.size())) {
        return;
    }

    selectedClipIndex_ = clipIndex;
    selectedKeyFrameIndex_ = -1;
    currentFrame_ = 0;
    selectedProperty_ = UIAnimationProperty::PositionX;

    UIAnimationClip* clip = GetSelectedClip();

    if (clip != nullptr) {
        CopyTextToBuffer(clipNameBuffer_, sizeof(clipNameBuffer_), clip->GetName());
    }

    SyncPlayerClip();
}

void UIAnimationEditor::SyncPlayerClip()
{
    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        player_.SetClip(nullptr);
        return;
    }

    player_.SetClip(clip);
    player_.SeekFrame(static_cast<float>(currentFrame_));
}

void UIAnimationEditor::CopyTextToBuffer(char* buffer, int bufferSize, const std::string& text)
{
    if (buffer == nullptr) {
        return;
    }
    if (bufferSize <= 0) {
        return;
    }

    errno_t copyResult = strncpy_s(
        buffer,
        static_cast<std::size_t>(bufferSize),
        text.c_str(),
        _TRUNCATE);

    if (copyResult != 0) {
        buffer[0] = '\0';
    }
}

void UIAnimationEditor::DrawClipList()
{
#ifdef USE_IMGUI
    ImGui::Text("Animations");
    ImGui::Separator();

    for (std::size_t clipIndex = 0; clipIndex < clips_.size(); ++clipIndex) {
        bool isSelected = false;

        if (static_cast<int>(clipIndex) == selectedClipIndex_) {
            isSelected = true;
        }

        if (ImGui::Selectable(clips_[clipIndex].GetName().c_str(), isSelected)) {
            SelectClip(static_cast<int>(clipIndex));
        }
    }

    ImGui::Spacing();

    if (ImGui::Button("New", ImVec2(-1.0f, 0.0f))) {
        UIAnimationClip clip;
        clip.SetName("NewClip");
        clips_.push_back(clip);
        SelectClip(static_cast<int>(clips_.size() - 1));
    }

    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        return;
    }

    ImGui::Separator();

    if (ImGui::InputText("Name", clipNameBuffer_, sizeof(clipNameBuffer_))) {
        UpdateNameFromBuffer(clip);
    }

    ImGui::InputText("File", filePathBuffer_, sizeof(filePathBuffer_));

    if (ImGui::Button("Save", ImVec2(-1.0f, 0.0f))) {
        SaveSelectedClip();
    }

    if (ImGui::Button("Load", ImVec2(-1.0f, 0.0f))) {
        LoadSelectedClip();
    }
#endif
}

void UIAnimationEditor::DrawTimeline()
{
#ifdef USE_IMGUI
    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        ImGui::Text("No Animation");
        return;
    }

    DrawTransport(clip);
    ImGui::Separator();
    DrawLivePropertyControls(clip);
    ImGui::Separator();
    DrawTimelineCanvas(clip);
#endif
}

void UIAnimationEditor::DrawSelectedKeyFrame()
{
#ifdef USE_IMGUI
    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        ImGui::Text("No Animation");
        return;
    }

    DrawKeyFrameInspector(clip);
#endif
}

void UIAnimationEditor::DrawTransport(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    ImGui::Text("%s", clip->GetName().c_str());

    if (ImGui::Button("|<")) {
        player_.Stop();
        currentFrame_ = 0;
        player_.SeekFrame(0.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("<")) {
        player_.Stop();
        currentFrame_ -= 1;
        ClampCurrentFrame(clip);
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    ImGui::SameLine();

    if (player_.IsPlaying()) {
        if (ImGui::Button("Stop")) {
            player_.Stop();
        }
    } else {
        if (ImGui::Button("Play")) {
            player_.SeekFrame(static_cast<float>(currentFrame_));
            player_.Play();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(">")) {
        player_.Stop();
        currentFrame_ += 1;
        ClampCurrentFrame(clip);
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    ImGui::SameLine();

    if (ImGui::Button(">|")) {
        player_.Stop();
        currentFrame_ = clip->GetLength();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    ImGui::SameLine();

    bool isLoop = player_.IsLoop();

    if (ImGui::Checkbox("Loop", &isLoop)) {
        player_.SetLoop(isLoop);
    }

    ImGui::SameLine();

    ImGui::Checkbox("Auto Key", &isAutoKeyEnabled_);

    int length = clip->GetLength();

    ImGui::SetNextItemWidth(90.0f);

    if (ImGui::DragInt("Length", &length, 1.0f, 1, 600)) {
        clip->SetLength(length);
        ClampCurrentFrame(clip);
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    ImGui::Text("Frame %d / %d", currentFrame_, clip->GetLength());

    if (ImGui::SliderInt("##FrameSlider", &currentFrame_, 0, clip->GetLength())) {
        player_.Stop();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }
#endif
}

void UIAnimationEditor::DrawLivePropertyControls(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    ImGui::Text("Current Object");

    Vector2 position;
    position.x = GetPreviewValue(UIAnimationProperty::PositionX);
    position.y = GetPreviewValue(UIAnimationProperty::PositionY);

    Vector2 scale;
    scale.x = GetPreviewValue(UIAnimationProperty::ScaleX);
    scale.y = GetPreviewValue(UIAnimationProperty::ScaleY);

    float rotation = GetPreviewValue(UIAnimationProperty::Rotation);

    Vector4 color;
    color.x = GetPreviewValue(UIAnimationProperty::ColorR);
    color.y = GetPreviewValue(UIAnimationProperty::ColorG);
    color.z = GetPreviewValue(UIAnimationProperty::ColorB);
    color.w = GetPreviewValue(UIAnimationProperty::ColorA);

    if (ImGui::DragFloat2("Position", &position.x, 1.0f)) {
        SetPreviewValue(UIAnimationProperty::PositionX, position.x);
        SetPreviewValue(UIAnimationProperty::PositionY, position.y);

        if (isAutoKeyEnabled_) {
            clip->AddKeyFrame(UIAnimationProperty::PositionX, currentFrame_, position.x);
            clip->AddKeyFrame(UIAnimationProperty::PositionY, currentFrame_, position.y);
        }
    }

    if (ImGui::DragFloat2("Scale", &scale.x, 1.0f, 1.0f, 2048.0f)) {
        SetPreviewValue(UIAnimationProperty::ScaleX, scale.x);
        SetPreviewValue(UIAnimationProperty::ScaleY, scale.y);

        if (isAutoKeyEnabled_) {
            clip->AddKeyFrame(UIAnimationProperty::ScaleX, currentFrame_, scale.x);
            clip->AddKeyFrame(UIAnimationProperty::ScaleY, currentFrame_, scale.y);
        }
    }

    if (ImGui::DragFloat("Rotation", &rotation, 0.01f)) {
        SetPreviewValue(UIAnimationProperty::Rotation, rotation);

        if (isAutoKeyEnabled_) {
            clip->AddKeyFrame(UIAnimationProperty::Rotation, currentFrame_, rotation);
        }
    }

    if (ImGui::ColorEdit4("Color", &color.x)) {
        SetPreviewValue(UIAnimationProperty::ColorR, color.x);
        SetPreviewValue(UIAnimationProperty::ColorG, color.y);
        SetPreviewValue(UIAnimationProperty::ColorB, color.z);
        SetPreviewValue(UIAnimationProperty::ColorA, color.w);

        if (isAutoKeyEnabled_) {
            clip->AddKeyFrame(UIAnimationProperty::ColorR, currentFrame_, color.x);
            clip->AddKeyFrame(UIAnimationProperty::ColorG, currentFrame_, color.y);
            clip->AddKeyFrame(UIAnimationProperty::ColorB, currentFrame_, color.z);
            clip->AddKeyFrame(UIAnimationProperty::ColorA, currentFrame_, color.w);
        }
    }

    if (ImGui::Button("Key Transform")) {
        AddTransformKeysFromPreview(clip);
    }

    ImGui::SameLine();

    if (ImGui::Button("Key Color")) {
        AddKeyFromPreview(clip, UIAnimationProperty::ColorR);
        AddKeyFromPreview(clip, UIAnimationProperty::ColorG);
        AddKeyFromPreview(clip, UIAnimationProperty::ColorB);
        AddKeyFromPreview(clip, UIAnimationProperty::ColorA);
    }
#endif
}

void UIAnimationEditor::DrawTimelineCanvas(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    const float labelWidth = 116.0f;
    const float rulerHeight = 24.0f;
    const float rowHeight = 30.0f;
    const float keySize = 14.0f;
    const int propertyCount = UIAnimationPropertyCount();
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float timelineWidth = availableWidth - labelWidth - 12.0f;

    if (timelineWidth < 240.0f) {
        timelineWidth = 240.0f;
    }

    float totalHeight = rulerHeight + rowHeight * static_cast<float>(propertyCount);
    ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
    float timelineStartX = canvasPosition.x + labelWidth;
    float timelineEndX = timelineStartX + timelineWidth;
    float canvasEndY = canvasPosition.y + totalHeight;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 backgroundColor = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.14f, 1.0f));
    ImU32 rowColor = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    ImU32 alternateRowColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
    ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.32f, 0.32f, 0.34f, 1.0f));
    ImU32 playheadColor = ImGui::GetColorU32(ImVec4(1.0f, 0.32f, 0.20f, 1.0f));
    ImU32 keyColor = ImGui::GetColorU32(ImVec4(1.0f, 0.76f, 0.24f, 1.0f));
    ImU32 selectedKeyColor = ImGui::GetColorU32(ImVec4(0.20f, 0.62f, 1.0f, 1.0f));

    drawList->AddRectFilled(
        canvasPosition,
        ImVec2(timelineEndX, canvasEndY),
        backgroundColor);

    ImGui::SetCursorScreenPos(ImVec2(timelineStartX, canvasPosition.y));
    ImGui::InvisibleButton("TimelineSeekArea", ImVec2(timelineWidth, totalHeight));

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGuiIO& io = ImGui::GetIO();
        currentFrame_ = GetFrameFromTimelinePosition(io.MousePos.x, timelineStartX, timelineWidth, clip->GetLength());
        player_.Stop();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    int tickCount = 6;

    if (clip->GetLength() < tickCount) {
        tickCount = clip->GetLength();
    }
    if (tickCount < 1) {
        tickCount = 1;
    }

    for (int tickIndex = 0; tickIndex <= tickCount; ++tickIndex) {
        float tickRate = static_cast<float>(tickIndex) / static_cast<float>(tickCount);
        int tickFrame = static_cast<int>(static_cast<float>(clip->GetLength()) * tickRate + 0.5f);
        float tickX = timelineStartX + timelineWidth * tickRate;

        drawList->AddLine(
            ImVec2(tickX, canvasPosition.y),
            ImVec2(tickX, canvasEndY),
            gridColor);

        char tickText[32] = {};
        std::snprintf(tickText, sizeof(tickText), "%d", tickFrame);
        drawList->AddText(ImVec2(tickX + 3.0f, canvasPosition.y + 3.0f), gridColor, tickText);
    }

    for (int propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex) {
        UIAnimationProperty property = UIAnimationPropertyFromIndex(propertyIndex);
        float rowTop = canvasPosition.y + rulerHeight + rowHeight * static_cast<float>(propertyIndex);
        float rowBottom = rowTop + rowHeight;
        ImU32 currentRowColor = rowColor;

        if ((propertyIndex % 2) == 1) {
            currentRowColor = alternateRowColor;
        }

        drawList->AddRectFilled(
            ImVec2(canvasPosition.x, rowTop),
            ImVec2(timelineEndX, rowBottom),
            currentRowColor);

        drawList->AddText(
            ImVec2(canvasPosition.x + 6.0f, rowTop + 7.0f),
            ImGui::GetColorU32(ImGuiCol_Text),
            UIAnimationPropertyToString(property));

        ImGui::SetCursorScreenPos(ImVec2(canvasPosition.x + labelWidth - 38.0f, rowTop + 5.0f));
        ImGui::PushID(propertyIndex);

        if (ImGui::SmallButton("+Key")) {
            AddKeyFromPreview(clip, property);
            UIAnimationTrack* createdTrack = clip->GetTrack(property);

            if (createdTrack != nullptr) {
                selectedProperty_ = property;
                selectedKeyFrameIndex_ = createdTrack->FindKeyFrameIndex(currentFrame_);
            }
        }

        ImGui::PopID();

        UIAnimationTrack* track = clip->GetTrack(property);

        if (track == nullptr) {
            continue;
        }

        std::vector<UIKeyFrame>& keyFrames = track->GetKeyFrames();

        for (std::size_t keyFrameIndex = 0; keyFrameIndex < keyFrames.size(); ++keyFrameIndex) {
            int keyFrame = keyFrames[keyFrameIndex].GetFrame();
            float keyRate = 0.0f;

            if (clip->GetLength() > 0) {
                keyRate = static_cast<float>(keyFrame) / static_cast<float>(clip->GetLength());
            }

            float keyX = timelineStartX + timelineWidth * keyRate;
            float keyY = rowTop + rowHeight * 0.5f;
            bool isSelected = false;

            if (selectedProperty_ == property && selectedKeyFrameIndex_ == static_cast<int>(keyFrameIndex)) {
                isSelected = true;
            }

            ImU32 currentKeyColor = keyColor;

            if (isSelected) {
                currentKeyColor = selectedKeyColor;
            }

            ImVec2 points[4];
            points[0] = ImVec2(keyX, keyY - keySize * 0.5f);
            points[1] = ImVec2(keyX + keySize * 0.5f, keyY);
            points[2] = ImVec2(keyX, keyY + keySize * 0.5f);
            points[3] = ImVec2(keyX - keySize * 0.5f, keyY);
            drawList->AddConvexPolyFilled(points, 4, currentKeyColor);

            ImGui::SetCursorScreenPos(ImVec2(keyX - keySize, keyY - keySize));
            ImGui::PushID(propertyIndex * 1000 + static_cast<int>(keyFrameIndex));
            ImGui::InvisibleButton("Key", ImVec2(keySize * 2.0f, keySize * 2.0f));

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                SelectKeyFrame(property, static_cast<int>(keyFrameIndex));
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                ImGuiIO& io = ImGui::GetIO();
                int movedFrame = GetFrameFromTimelinePosition(io.MousePos.x, timelineStartX, timelineWidth, clip->GetLength());

                if (CanMoveKeyFrameToFrame(*track, static_cast<int>(keyFrameIndex), movedFrame)) {
                    keyFrames[keyFrameIndex].SetFrame(movedFrame);
                    track->SortKeyFrames();
                    selectedProperty_ = property;
                    selectedKeyFrameIndex_ = track->FindKeyFrameIndex(movedFrame);
                    currentFrame_ = movedFrame;
                    player_.Stop();
                    player_.SeekFrame(static_cast<float>(currentFrame_));
                }
            }

            ImGui::PopID();
        }
    }

    float playheadRate = 0.0f;

    if (clip->GetLength() > 0) {
        playheadRate = static_cast<float>(currentFrame_) / static_cast<float>(clip->GetLength());
    }

    float playheadX = timelineStartX + timelineWidth * playheadRate;

    drawList->AddLine(
        ImVec2(playheadX, canvasPosition.y),
        ImVec2(playheadX, canvasEndY),
        playheadColor,
        2.0f);

    drawList->AddTriangleFilled(
        ImVec2(playheadX - 5.0f, canvasPosition.y),
        ImVec2(playheadX + 5.0f, canvasPosition.y),
        ImVec2(playheadX, canvasPosition.y + 9.0f),
        playheadColor);

    ImGui::SetCursorScreenPos(ImVec2(canvasPosition.x, canvasEndY + 8.0f));
#endif
}

void UIAnimationEditor::DrawKeyFrameInspector(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    ImGui::Text("Selection");
    ImGui::Separator();

    UIAnimationTrack* track = clip->GetTrack(selectedProperty_);

    if (track == nullptr) {
        ImGui::Text("No Track");
        return;
    }

    std::vector<UIKeyFrame>& keyFrames = track->GetKeyFrames();

    if (selectedKeyFrameIndex_ < 0 || selectedKeyFrameIndex_ >= static_cast<int>(keyFrames.size())) {
        ImGui::Text("%s", UIAnimationPropertyToString(selectedProperty_));

        float currentValue = GetPreviewValue(selectedProperty_);
        ImGui::DragFloat("Current", &currentValue, 1.0f);

        if (ImGui::Button("Key Current", ImVec2(-1.0f, 0.0f))) {
            AddKeyFromPreview(clip, selectedProperty_);
            selectedKeyFrameIndex_ = track->FindKeyFrameIndex(currentFrame_);
        }

        return;
    }

    UIKeyFrame& keyFrame = keyFrames[static_cast<std::size_t>(selectedKeyFrameIndex_)];
    int frame = keyFrame.GetFrame();
    float value = keyFrame.GetValue();
    int interpolationIndex = UIAnimationInterpolationToIndex(keyFrame.GetInterpolation());

    ImGui::Text("%s", UIAnimationPropertyToString(selectedProperty_));

    if (ImGui::Button("Go To Key", ImVec2(-1.0f, 0.0f))) {
        player_.Stop();
        currentFrame_ = keyFrame.GetFrame();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    if (ImGui::DragInt("Frame", &frame, 1.0f, 0, clip->GetLength())) {
        if (CanMoveKeyFrameToFrame(*track, selectedKeyFrameIndex_, frame)) {
            keyFrame.SetFrame(frame);
            track->SortKeyFrames();
            selectedKeyFrameIndex_ = track->FindKeyFrameIndex(frame);
            currentFrame_ = frame;
            player_.SeekFrame(static_cast<float>(currentFrame_));
        }
    }

    if (ImGui::DragFloat("Value", &value, 1.0f)) {
        keyFrame.SetValue(value);
        SetPreviewValue(selectedProperty_, value);
    }

    const char* interpolationNames[] = {
        "Linear",
        "EaseIn",
        "EaseOut",
        "EaseInOut"
    };

    if (ImGui::Combo("Interpolation", &interpolationIndex, interpolationNames, UIAnimationInterpolationCount())) {
        keyFrame.SetInterpolation(UIAnimationInterpolationFromIndex(interpolationIndex));
    }

    ImGui::Separator();

    if (ImGui::Button("Delete Key", ImVec2(-1.0f, 0.0f))) {
        track->RemoveKeyFrameAtIndex(static_cast<std::size_t>(selectedKeyFrameIndex_));
        selectedKeyFrameIndex_ = -1;
    }
#endif
}

void UIAnimationEditor::SaveSelectedClip()
{
    UIAnimationClip* clip = GetSelectedClip();

    if (clip == nullptr) {
        return;
    }

    std::string filePath = filePathBuffer_;
    clip->SaveToFile(filePath);
}

void UIAnimationEditor::LoadSelectedClip()
{
    UIAnimationClip clip;
    std::string filePath = filePathBuffer_;

    if (!clip.LoadFromFile(filePath)) {
        return;
    }

    if (selectedClipIndex_ >= 0 && selectedClipIndex_ < static_cast<int>(clips_.size())) {
        clips_[static_cast<std::size_t>(selectedClipIndex_)] = clip;
    } else {
        clips_.push_back(clip);
        selectedClipIndex_ = static_cast<int>(clips_.size() - 1);
    }

    UIAnimationClip* selectedClip = GetSelectedClip();

    if (selectedClip != nullptr) {
        CopyTextToBuffer(clipNameBuffer_, sizeof(clipNameBuffer_), selectedClip->GetName());
    }

    currentFrame_ = 0;
    selectedKeyFrameIndex_ = -1;
    SyncPlayerClip();
}

void UIAnimationEditor::UpdateNameFromBuffer(UIAnimationClip* clip)
{
    if (clip == nullptr) {
        return;
    }

    std::string name = clipNameBuffer_;
    clip->SetName(name);
}

void UIAnimationEditor::SelectKeyFrame(UIAnimationProperty property, int keyFrameIndex)
{
    selectedProperty_ = property;
    selectedKeyFrameIndex_ = keyFrameIndex;
}

void UIAnimationEditor::AddKeyFromPreview(UIAnimationClip* clip, UIAnimationProperty property)
{
    if (clip == nullptr) {
        return;
    }

    clip->AddKeyFrame(property, currentFrame_, GetPreviewValue(property));
}

void UIAnimationEditor::AddTransformKeysFromPreview(UIAnimationClip* clip)
{
    AddKeyFromPreview(clip, UIAnimationProperty::PositionX);
    AddKeyFromPreview(clip, UIAnimationProperty::PositionY);
    AddKeyFromPreview(clip, UIAnimationProperty::ScaleX);
    AddKeyFromPreview(clip, UIAnimationProperty::ScaleY);
    AddKeyFromPreview(clip, UIAnimationProperty::Rotation);
}

float UIAnimationEditor::GetPreviewValue(UIAnimationProperty property) const
{
    if (previewSprite_ == nullptr) {
        if (property == UIAnimationProperty::ScaleX || property == UIAnimationProperty::ScaleY) {
            return 1.0f;
        }
        if (property == UIAnimationProperty::ColorR || property == UIAnimationProperty::ColorG ||
            property == UIAnimationProperty::ColorB || property == UIAnimationProperty::ColorA) {
            return 1.0f;
        }

        return 0.0f;
    }

    if (property == UIAnimationProperty::PositionX) {
        return previewSprite_->GetPosition().x;
    }
    if (property == UIAnimationProperty::PositionY) {
        return previewSprite_->GetPosition().y;
    }
    if (property == UIAnimationProperty::ScaleX) {
        return previewSprite_->GetSize().x;
    }
    if (property == UIAnimationProperty::ScaleY) {
        return previewSprite_->GetSize().y;
    }
    if (property == UIAnimationProperty::Rotation) {
        return previewSprite_->GetRotation();
    }
    if (property == UIAnimationProperty::ColorR) {
        return previewSprite_->GetColor().x;
    }
    if (property == UIAnimationProperty::ColorG) {
        return previewSprite_->GetColor().y;
    }
    if (property == UIAnimationProperty::ColorB) {
        return previewSprite_->GetColor().z;
    }
    if (property == UIAnimationProperty::ColorA) {
        return previewSprite_->GetColor().w;
    }

    return 0.0f;
}

void UIAnimationEditor::SetPreviewValue(UIAnimationProperty property, float value)
{
    if (previewSprite_ == nullptr) {
        return;
    }

    if (property == UIAnimationProperty::PositionX || property == UIAnimationProperty::PositionY) {
        Vector2 position = previewSprite_->GetPosition();

        if (property == UIAnimationProperty::PositionX) {
            position.x = value;
        }
        if (property == UIAnimationProperty::PositionY) {
            position.y = value;
        }

        previewSprite_->SetPosition(position);
        return;
    }

    if (property == UIAnimationProperty::ScaleX || property == UIAnimationProperty::ScaleY) {
        Vector2 scale = previewSprite_->GetSize();

        if (property == UIAnimationProperty::ScaleX) {
            scale.x = value;
        }
        if (property == UIAnimationProperty::ScaleY) {
            scale.y = value;
        }

        previewSprite_->SetSize(scale);
        return;
    }

    if (property == UIAnimationProperty::Rotation) {
        previewSprite_->SetRotation(value);
        return;
    }

    Vector4 color = previewSprite_->GetColor();

    if (property == UIAnimationProperty::ColorR) {
        color.x = value;
    }
    if (property == UIAnimationProperty::ColorG) {
        color.y = value;
    }
    if (property == UIAnimationProperty::ColorB) {
        color.z = value;
    }
    if (property == UIAnimationProperty::ColorA) {
        color.w = value;
    }

    previewSprite_->SetColor(color);
}

bool UIAnimationEditor::CanMoveKeyFrameToFrame(const UIAnimationTrack& track, int keyFrameIndex, int frame) const
{
    if (frame < 0) {
        return false;
    }

    const std::vector<UIKeyFrame>& keyFrames = track.GetKeyFrames();

    for (std::size_t index = 0; index < keyFrames.size(); ++index) {
        if (static_cast<int>(index) == keyFrameIndex) {
            continue;
        }
        if (keyFrames[index].GetFrame() == frame) {
            return false;
        }
    }

    return true;
}

int UIAnimationEditor::GetFrameFromTimelinePosition(float mousePositionX, float timelineStartX, float timelineWidth, int length) const
{
    if (timelineWidth <= 0.0f) {
        return 0;
    }

    float rate = (mousePositionX - timelineStartX) / timelineWidth;

    if (rate < 0.0f) {
        rate = 0.0f;
    }
    if (rate > 1.0f) {
        rate = 1.0f;
    }

    return static_cast<int>(static_cast<float>(length) * rate + 0.5f);
}

void UIAnimationEditor::ClampCurrentFrame(UIAnimationClip* clip)
{
    if (currentFrame_ < 0) {
        currentFrame_ = 0;
    }

    if (clip == nullptr) {
        return;
    }

    if (currentFrame_ > clip->GetLength()) {
        currentFrame_ = clip->GetLength();
    }
}
