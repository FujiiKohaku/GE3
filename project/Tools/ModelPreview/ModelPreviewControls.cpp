#include "ModelPreviewApp.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Winapp/WinApp.h"
#include <commdlg.h>
#include <stdexcept>

#pragma comment(lib, "comdlg32.lib")

void ModelPreviewApp::ChooseFile()
{
    wchar_t file[32768] {};
    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = WinApp::GetInstance()->GetHwnd();
    dialog.lpstrFilter = L"3D Models\0*.obj;*.gltf;*.glb;*.fbx;*.dae;*.ply\0All files\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = _countof(file);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        Load(file, append_);
    }
}

void ModelPreviewApp::Controls()
{
    if (!showControls_) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 690), ImGuiCond_FirstUseEver);
    ImGui::Begin("Model Preview");
    ImGui::TextWrapped("Drag RMB: orbit | MMB: pan | Wheel: zoom | F: fit | Tab: hide UI | F12: PNG");
    ImGui::TextWrapped("Stage03 game lighting, sky, outlines and fog. Static model poses; no gameplay particles.");
    if (ImGui::Button("Save PNG")) { capture_.RequestPng(); }
    ImGui::SameLine();
    if (capture_.IsRecording()) {
        if (ImGui::Button("Stop GIF")) { capture_.StopGif(); }
        ImGui::Text("Frame %d / %d", capture_.FrameIndex(), capture_.FrameCount());
    } else {
        ImGui::BeginDisabled(items_.empty());
        if (ImGui::Button("Record GIF")) {
            recordingYaw_ = orbitYaw_;
            capture_.StartGif(gifFps_, gifFps_ * gifSeconds_);
        }
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("%s", capture_.Status().c_str());
    ImGui::Separator();
    ImGui::BeginDisabled(capture_.IsRecording());
    ImGui::Checkbox("Game rendering (Stage03)", &gameLook_);
    if (ImGui::Button("Open model...")) { ChooseFile(); }
    ImGui::SameLine();
    ImGui::Checkbox("Add", &append_);
    ImGui::InputTextWithHint("##filter", "Filter project models", filter_, sizeof(filter_));
    if (ImGui::BeginListBox("##models", ImVec2(-1, 145))) {
        for (const auto& path : modelPaths_) {
            if (filter_[0] != '\0' && path.find(filter_) == std::string::npos) {
                continue;
            }
            std::string label = path.substr(std::string("resources/Models/").size());
            if (ImGui::Selectable(label.c_str())) { Load(path, append_); }
        }
        ImGui::EndListBox();
    }
    if (ImGui::Button("Rescan")) { ScanModels(); }
    ImGui::SameLine();
    if (ImGui::Button("Fit view")) { Fit(); }
    if (!items_.empty()) {
        if (ImGui::BeginCombo("Object", items_[selected_]->name.c_str())) {
            for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
                ImGui::PushID(index);
                if (ImGui::Selectable(items_[index]->name.c_str(), selected_ == index)) { selected_ = index; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        auto& item = *items_[selected_];
        if (ImGui::BeginCombo("Material / PS", item.shader.c_str())) {
            for (const auto& shader : shaderNames_) {
                if (ImGui::Selectable(shader.c_str(), shader == item.shader)) { item.shader = shader; }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Reload shaders")) {
            try {
                Object3dManager::GetInstance()->ReloadMaterialPipelines();
                error_.clear();
            } catch (const std::exception& error) { error_ = error.what(); }
        }
        ImGui::ColorEdit4("Tint", &item.color.x);
        ImGui::Checkbox("Transparent", &item.transparent);
        ImGui::Checkbox("Environment reflection", &item.environment);
        ImGui::SliderFloat("Reflection", &item.reflection, 0.0f, 0.5f);
        ImGui::SliderFloat("Shininess", &item.shininess, 0.0f, 128.0f);
        ImGui::DragFloat3("Position", &item.position.x, sceneRadius_ * 0.01f);
        ImGui::DragFloat3("Rotation (rad)", &item.rotation.x, 0.01f);
        ImGui::DragFloat("Scale", &item.scale, 0.01f, 0.01f, 100.0f);
        if (item.stageTransform) {
            ImGui::Text("Stage scale: %.1f, %.1f, %.1f", item.axisScale.x, item.axisScale.y, item.axisScale.z);
        }
        ApplyMaterial(item);
        if (ImGui::Button("Remove object")) {
            dx_->WaitForGPU();
            items_.erase(items_.begin() + selected_);
            selected_ = 0;
            Fit();
        }
    }
    ImGui::BeginDisabled(gameLook_);
    ImGui::ColorEdit3("Background", &background_.x);
    ImGui::SliderFloat3("Light direction", &lightDirection_.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Light intensity", &lightIntensity_, 0.0f, 2.0f);
    ImGui::SliderFloat("Ambient", &ambient_, 0.0f, 1.0f);
    ImGui::EndDisabled();
    ImGui::TextWrapped("Ice/Archive presets use fixed shader lighting; light controls affect Standard/Toon.");
    ImGui::Checkbox("Animate shader", &animate_);
    ImGui::SliderInt("GIF fps", &gifFps_, 5, 30);
    ImGui::SliderInt("GIF seconds", &gifSeconds_, 1, 10);
    ImGui::Checkbox("One full orbit", &turntable_);
    ImGui::EndDisabled();
    ImGui::TextWrapped("PNG: 1280x720 | GIF: 640x360, 256 colors");
    if (!error_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "%s", error_.c_str());
    }
    ImGui::End();
}
