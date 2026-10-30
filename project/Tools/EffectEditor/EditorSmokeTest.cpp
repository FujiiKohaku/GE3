#include "EffectEditorApp.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/Logger/Logger.h"
#include <fstream>

namespace {
void Require(bool isValid, const std::string& error)
{
    if (!isValid) { throw std::runtime_error(error); }
}
void RemoveSmokeDirectory(const std::filesystem::path& root, const std::string& id)
{
    const auto absoluteRoot = std::filesystem::weakly_canonical(root);
    const auto path = absoluteRoot / id;
    if (!id.starts_with("EditorSmoke_") || path.parent_path() != absoluteRoot) { throw std::runtime_error("Unsafe test cleanup path"); }
    if (std::filesystem::is_directory(path)) { std::filesystem::remove_all(path); }
}
}

void EffectEditorApp::RunSmokeTest()
{
    isSmokeTest_ = true;
    for (int frame = 0; frame < 30; ++frame) { Update(); Draw(); }
    std::filesystem::create_directories("runtime/EffectEditorSmoke");
    preview_->SavePng("runtime/EffectEditorSmoke/Particle.png", lastBackBuffer_);
    preview_->SavePng("runtime/EffectEditorSmoke/Preview.png");
    const uint32_t descriptorCount = SrvManager::GetInstance()->GetAllocatedCount();
    const auto originalParticleDraft = particleDraft_;
    const auto originalPostDraft = postDraft_;
    for (int cycle = 0; cycle < 2; ++cycle) {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F11, true); Update(); Draw();
        Require(WinApp::GetInstance()->IsBorderlessFullscreen(), "F11 did not enter fullscreen");
        Require(preview_->GetWidth() == WinApp::kFullscreenWidth && preview_->GetHeight() == WinApp::kFullscreenHeight, "Fullscreen preview size is incorrect");
        Update(); Draw(); // Holding F11 must not toggle repeatedly.
        Require(WinApp::GetInstance()->IsBorderlessFullscreen(), "Holding F11 toggled fullscreen repeatedly");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F11, false); Update(); Draw();
        if (cycle == 0) { preview_->SavePng("runtime/EffectEditorSmoke/Fullscreen.png", lastBackBuffer_); }
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F11, true); Update(); Draw();
        Require(!WinApp::GetInstance()->IsBorderlessFullscreen(), "F11 did not return to windowed mode");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F11, false); Update(); Draw();
        Require(preview_->GetWidth() == WinApp::kClientWidth && preview_->GetHeight() == WinApp::kClientHeight, "Windowed preview size was not restored");
        Require(SrvManager::GetInstance()->GetAllocatedCount() == descriptorCount, "Fullscreen toggling leaked descriptors");
    }
    Require(particleDraft_ == originalParticleDraft && postDraft_ == originalPostDraft, "Fullscreen toggling changed editor settings");
    Logger::Log("EffectEditor fullscreen PASS: F11 entry/exit, held-key guard, preview resizing, retained edits, repeated toggles without descriptor growth");
    const auto sourceBefore = JsonFile::Load("resources/Effects/HandFlame/Effect.json");
    const std::string suffix = std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64());
    const std::string meshId = "EditorSmoke_Mesh_" + suffix;
    const std::string trailId = "EditorSmoke_Trail_" + suffix;
    const std::string copyId = "EditorSmoke_Copy_" + suffix;
    CreateEffect(meshId, "検証用の炎", false, false);
    Update(); Draw();
    Require(particleDraft_["Render"]["Type"] == "Mesh", "New mesh type is wrong");
    const auto originalDraft = particleDraft_;
    particleHistory_.Push(particleDraft_);
    particleDraft_["Emitter"]["Count"] = 12;
    Require(particleHistory_.Undo(particleDraft_) && particleDraft_ == originalDraft, "Undo failed");
    Require(particleHistory_.Redo(particleDraft_) && particleDraft_["Emitter"]["Count"] == 12, "Redo failed");
    shouldApply_ = true; Update(); Draw();
    workspace_ = 0; shouldSave_ = true; Update(); Draw();
    Require(!isStatusError_, status_);
    isLoop_ = false; Restart(); Update(); Draw();
    nlohmann::json oneShotConfig;
    std::string oneShotError;
    Require(EffectManager::GetInstance()->GetEffectConfig(selectedId_, oneShotConfig, oneShotError), oneShotError);
    Require(EffectManager::GetInstance()->ApplyEffectConfigAndRestart(selectedId_, oneShotConfig, oneShotError), oneShotError);
    for (int frame = 0; frame < 160; ++frame) { Update(); Draw(); }
    Require(!EffectManager::GetInstance()->IsEffectAlive(effectHandle_), "One-shot still runs after apply despite disabling loop");
    isLoop_ = true; Restart();
    const auto saved = JsonFile::Load(std::filesystem::path("resources/Effects") / meshId / "Effect.json");
    Require(saved["Emitter"]["Count"] == 12 && saved["Render"]["Type"] == "Mesh", "Particle save did not preserve settings/type");
    isPlaying_ = false;
    particleDraft_["Particle"]["LifeTime"] = 0.8f;
    shouldApply_ = true; Update(); Draw(); Update(); Draw();
    Require(!isStatusError_, "Apply while paused failed: " + status_);
    isPlaying_ = true;
    CreateEffect(copyId, "複製の確認", false, true); Update(); Draw();
    Require(particleDraft_["Particle"]["LifeTime"] == 0.8f, "Duplicate did not keep the draft");
    CreateEffect(trailId, "検証用トレイル", true, false);
    for (int frame = 0; frame < 20; ++frame) { Update(); Draw(); }
    Require(particleDraft_["Render"]["Type"] == "Trail", "New trail type is wrong");
    preview_->SavePng("runtime/EffectEditorSmoke/Trail.png", lastBackBuffer_);
    const auto validTrail = particleDraft_;
    particleDraft_["Render"]["Type"] = "Mesh"; shouldApply_ = true; Update(); Draw();
    Require(isStatusError_, "Changing the fixed particle type was accepted");
    particleDraft_ = validTrail;
    std::string error;
    const auto preset = post_->GetPresetConfig();
    auto invalid = preset;
    invalid["effects"].push_back({ { "type", "UnknownEffect" }, { "stage", "AfterParticle" }, { "enabled", true } });
    Require(!post_->ApplyPresetConfig(invalid, error), "Invalid post preset was accepted");
    Require(post_->GetPresetConfig() == preset, "Failed preset apply changed the previous state");
    invalid = preset; invalid["bloom"]["intensity"] = -1;
    Require(!post_->ApplyPresetConfig(invalid, error), "Negative bloom strength was accepted");
    invalid = preset; invalid["effects"][0]["stage"] = "AfterParticle";
    Require(!post_->ApplyPresetConfig(invalid, error), "Invalid pass stage was accepted");
    workspace_ = 1;
    postDraft_["parameters"]["colorSaturation"] = 0.25f;
    postDraft_["bloom"]["intensity"] = 1.2f;
    pendingPostSavePath_ = std::filesystem::path("runtime/EffectEditorSmoke") / ("PostPreset_" + suffix + ".json");
    shouldApplyPost_ = true; Update(); Draw();
    shouldSave_ = true; Update(); Draw();
    Require(!isStatusError_, status_);
    const auto postSaved = JsonFile::Load(postPath_);
    Require(postSaved["parameters"]["colorSaturation"] == 0.25f, "Post preset save failed");
    Require(post_->LoadPreset(postPath_.generic_string(), error), error);
    for (int frame = 0; frame < 3; ++frame) { Update(); Draw(); }
    preview_->SavePng("runtime/EffectEditorSmoke/Post.png", lastBackBuffer_);
    Require(JsonFile::Load("resources/Effects/HandFlame/Effect.json") == sourceBefore, "Template was modified");
    RunLiveReloadTests(meshId);
    EffectManager::GetInstance()->StopAllEffects();
    for (const auto& id : { meshId, trailId, copyId }) {
        RemoveSmokeDirectory("resources/Effects", id);
        RemoveSmokeDirectory("resources/CompiledShaders/Effects", id);
    }
    Logger::Log("EffectEditor smoke PASS: real DX12 preview/UI, new Mesh/Trail, duplicate, undo/redo, apply/save, paused apply, type guard, post validation/save/load");
}
