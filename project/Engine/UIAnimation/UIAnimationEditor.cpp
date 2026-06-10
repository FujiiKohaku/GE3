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

    if (previewSprite_ != nullptr) {
        player_.ApplyToSprite(previewSprite_);
    }
}

void UIAnimationEditor::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("UI Animation Editor");

    ImGui::Columns(3, "UIAnimationEditorColumns", true);

    DrawClipList();

    ImGui::NextColumn();
    DrawTimeline();

    ImGui::NextColumn();
    DrawSelectedKeyFrame();

    ImGui::Columns(1);
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
    newKeyFrame_ = 0;
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

    ImGui::Separator();

    if (ImGui::Button("New Clip")) {
        UIAnimationClip clip;
        clip.SetName("NewClip");
        clips_.push_back(clip);
        SelectClip(static_cast<int>(clips_.size() - 1));
    }

    UIAnimationClip* clip = GetSelectedClip();

    if (clip != nullptr) {
        if (ImGui::InputText("Name", clipNameBuffer_, sizeof(clipNameBuffer_))) {
            UpdateNameFromBuffer(clip);
        }

        ImGui::InputText("File", filePathBuffer_, sizeof(filePathBuffer_));

        if (ImGui::Button("Save JSON")) {
            SaveSelectedClip();
        }

        if (ImGui::Button("Load JSON")) {
            LoadSelectedClip();
        }
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
    DrawKeyFrameCreator(clip);
    ImGui::Separator();
    DrawTrackTable(clip);
#endif
}

void UIAnimationEditor::DrawSelectedKeyFrame()
{
#ifdef USE_IMGUI
    UIAnimationClip* clip = GetSelectedClip();

    ImGui::Text("Selected Key");
    ImGui::Separator();

    if (clip == nullptr) {
        ImGui::Text("No Animation");
        return;
    }

    UIAnimationTrack* track = clip->GetTrack(selectedProperty_);

    if (track == nullptr) {
        ImGui::Text("No Track");
        return;
    }

    std::vector<UIKeyFrame>& keyFrames = track->GetKeyFrames();

    if (selectedKeyFrameIndex_ < 0) {
        ImGui::Text("No Selection");
        return;
    }
    if (selectedKeyFrameIndex_ >= static_cast<int>(keyFrames.size())) {
        ImGui::Text("No Selection");
        return;
    }

    UIKeyFrame& keyFrame = keyFrames[static_cast<std::size_t>(selectedKeyFrameIndex_)];
    int frame = keyFrame.GetFrame();
    float value = keyFrame.GetValue();
    int interpolationIndex = UIAnimationInterpolationToIndex(keyFrame.GetInterpolation());

    ImGui::Text("Property: %s", UIAnimationPropertyToString(selectedProperty_));

    if (ImGui::InputInt("Frame", &frame)) {
        if (frame < 0) {
            frame = 0;
        }
        if (frame > clip->GetLength()) {
            frame = clip->GetLength();
        }

        keyFrame.SetFrame(frame);
        track->SortKeyFrames();
        selectedKeyFrameIndex_ = track->FindKeyFrameIndex(frame);
    }

    if (ImGui::InputFloat("Value", &value)) {
        keyFrame.SetValue(value);
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

    if (ImGui::Button("Delete Key")) {
        track->RemoveKeyFrameAtIndex(static_cast<std::size_t>(selectedKeyFrameIndex_));
        selectedKeyFrameIndex_ = -1;
    }
#endif
}

void UIAnimationEditor::DrawTransport(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    ImGui::Text("Timeline");
    ImGui::Text("Current Frame: %d", currentFrame_);

    if (ImGui::Button("Play")) {
        player_.SeekFrame(static_cast<float>(currentFrame_));
        player_.Play();
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop")) {
        player_.Stop();
    }

    ImGui::SameLine();

    if (ImGui::Button("Start")) {
        currentFrame_ = 0;
        player_.SeekFrame(0.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("End")) {
        currentFrame_ = clip->GetLength();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    bool isLoop = player_.IsLoop();

    if (ImGui::Checkbox("Loop", &isLoop)) {
        player_.SetLoop(isLoop);
    }

    int length = clip->GetLength();

    if (ImGui::InputInt("Length", &length)) {
        clip->SetLength(length);

        if (currentFrame_ > clip->GetLength()) {
            currentFrame_ = clip->GetLength();
        }

        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    if (ImGui::SliderInt("Frame", &currentFrame_, 0, clip->GetLength())) {
        player_.Stop();
        player_.SeekFrame(static_cast<float>(currentFrame_));
    }

    float progress = 0.0f;

    if (clip->GetLength() > 0) {
        progress = static_cast<float>(currentFrame_) / static_cast<float>(clip->GetLength());
    }

    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
#endif
}

void UIAnimationEditor::DrawKeyFrameCreator(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    const char* propertyNames[] = {
        "PositionX",
        "PositionY",
        "ScaleX",
        "ScaleY",
        "Rotation",
        "ColorR",
        "ColorG",
        "ColorB",
        "ColorA"
    };

    int propertyIndex = UIAnimationPropertyToIndex(selectedProperty_);

    if (ImGui::Combo("Property", &propertyIndex, propertyNames, UIAnimationPropertyCount())) {
        selectedProperty_ = UIAnimationPropertyFromIndex(propertyIndex);
        selectedKeyFrameIndex_ = -1;
    }

    ImGui::InputInt("Key Frame", &newKeyFrame_);

    if (newKeyFrame_ < 0) {
        newKeyFrame_ = 0;
    }
    if (newKeyFrame_ > clip->GetLength()) {
        newKeyFrame_ = clip->GetLength();
    }

    ImGui::InputFloat("Key Value", &newKeyValue_);

    if (ImGui::Button("Add Key")) {
        if (clip->AddKeyFrame(selectedProperty_, newKeyFrame_, newKeyValue_)) {
            UIAnimationTrack* track = clip->GetTrack(selectedProperty_);

            if (track != nullptr) {
                selectedKeyFrameIndex_ = track->FindKeyFrameIndex(newKeyFrame_);
            }
        }
    }

    if (ImGui::Button("Add Current Position/Scale")) {
        if (previewSprite_ != nullptr) {
            Vector2 position = previewSprite_->GetPosition();
            Vector2 scale = previewSprite_->GetSize();

            clip->AddKeyFrame(UIAnimationProperty::PositionX, currentFrame_, position.x);
            clip->AddKeyFrame(UIAnimationProperty::PositionY, currentFrame_, position.y);
            clip->AddKeyFrame(UIAnimationProperty::ScaleX, currentFrame_, scale.x);
            clip->AddKeyFrame(UIAnimationProperty::ScaleY, currentFrame_, scale.y);
        }
    }
#endif
}

void UIAnimationEditor::DrawTrackTable(UIAnimationClip* clip)
{
#ifdef USE_IMGUI
    if (ImGui::BeginTable("UIAnimationTimelineTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Keys");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Select");
        ImGui::TableHeadersRow();

        std::vector<UIAnimationTrack>& tracks = clip->GetTracks();

        for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
            UIAnimationTrack& track = tracks[trackIndex];
            const std::vector<UIKeyFrame>& keyFrames = track.GetKeyFrames();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", UIAnimationPropertyToString(track.GetProperty()));

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", static_cast<int>(keyFrames.size()));

            ImGui::TableSetColumnIndex(2);
            float evaluatedValue = track.Evaluate(static_cast<float>(currentFrame_), 0.0f);
            ImGui::Text("%.3f", evaluatedValue);

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(static_cast<int>(trackIndex));

            for (std::size_t keyFrameIndex = 0; keyFrameIndex < keyFrames.size(); ++keyFrameIndex) {
                ImGui::PushID(static_cast<int>(keyFrameIndex));

                if (keyFrameIndex > 0) {
                    ImGui::SameLine();
                }

                char label[32] = {};
                std::snprintf(label, sizeof(label), "%d", keyFrames[keyFrameIndex].GetFrame());

                if (ImGui::SmallButton(label)) {
                    SelectKeyFrame(track.GetProperty(), static_cast<int>(keyFrameIndex));
                    currentFrame_ = keyFrames[keyFrameIndex].GetFrame();
                    player_.Stop();
                    player_.SeekFrame(static_cast<float>(currentFrame_));
                }

                ImGui::PopID();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
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
