#include "EffectEditorApp.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/Logger/Logger.h"
#include "Tests/EffectCapacityTestAccess.h"
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
    {
        const auto previousId = selectedId_;
        const float previousDistance = distance_;
        const float previousYaw = yaw_;
        const float previousPitch = pitch_;
        const Vector3 previousTarget = target_;
        const bool wasFloorVisible = isFloorVisible_;
        const bool wasGridVisible = isGridVisible_;
        const bool wasLoopEnabled = isLoop_;
        SelectEffect("MissileExplosion");
        isLoop_ = false; distance_ = 30.0f; target_ = { 0, 0.15f, 0 }; pitch_ = 0.15f; yaw_ = 0.0f;
        isFloorVisible_ = false; isGridVisible_ = false;
        Update(); Draw();
        Restart();
        EffectCapacityTestAccess sphereReadback;
        Update(); sphereReadback.Record(EffectManager::GetInstance(), effectHandle_); Draw();
        sphereReadback.VerifySphere(particleDraft_["Particle"]["EndScale"].get<float>(), { 0, 0.15f, 0 });
        isPlaying_ = false;
        const char* captures[] = { "MissileSphereFront.png", "MissileSphereSide.png", "MissileSphereBack.png" };
        for (int viewIndex = 0; viewIndex < 3; ++viewIndex) {
            yaw_ = static_cast<float>(viewIndex) * 1.5707963f;
            Update(); Draw();
            preview_->SavePng(std::filesystem::path("runtime/EffectEditorSmoke") / captures[viewIndex]);
        }
        // Editing the shared radius must also reach the GPU; keep the asset on disk untouched.
        particleDraft_["Particle"]["EndScale"] = 5.0f;
        shouldApply_ = true; Update(); Draw();
        isPlaying_ = true;
        Update(); sphereReadback.Record(EffectManager::GetInstance(), effectHandle_); Draw();
        sphereReadback.VerifySphere(5.0f, { 0, 0.15f, 0 });
        SelectEffect(previousId);
        distance_ = previousDistance; yaw_ = previousYaw; pitch_ = previousPitch; target_ = previousTarget;
        isFloorVisible_ = wasFloorVisible; isGridVisible_ = wasGridVisible; isLoop_ = wasLoopEnabled;
        Restart(); Update(); Draw();
        Logger::Log("Missile sphere PASS: GPU radius and center, live radius change, front/side/back captures");
    }
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
    Require(editorSchema_.Matches(particleDraft_), "New effect did not inherit its editor schema");
    Require(!editorSchema_.IsSupported(particleDraft_, "Particle", "StartColor"), "Fixed flame color was exposed");
    Require(!editorSchema_.IsSupported(particleDraft_, "Emitter", "Shape"), "Hardcoded emitter shape was exposed");
    Require(editorSchema_.IsSupported(particleDraft_, "Simulation", "Gravity"), "Supported flame setting was hidden");
    const auto hiddenBefore = particleDraft_["Simulation"]["AttractionStrength"];
    const auto hiddenColor = particleDraft_["Particle"]["StartColor"];
    inspectorTab_ = 0; Update(); Draw();
    Require(particleDraft_["Simulation"]["AttractionStrength"] == hiddenBefore &&
        particleDraft_["Particle"]["StartColor"] == hiddenColor, "Filtering deleted hidden engine settings");
    Require(!particleDraft_["Particle"].contains("MaxParticles"), "Rendering UI materialized the implicit capacity");
    const auto schemaPath = particlePath_.parent_path() / "Editor.json";
    const auto schemaBefore = JsonFile::Load(schemaPath);
    auto changedSchema = schemaBefore;
    changedSchema["Properties"].erase("Emitter.Radius");
    changedSchema["Properties"]["Emitter.Count"]["Label"] = "検証用の発生数";
    JsonFile::Save(schemaPath, changedSchema);
    uint64_t schemaNowMs = GetTickCount64() + 1000;
    const auto draftBeforeSchemaChange = particleDraft_;
    PumpLiveReloadTest(schemaNowMs);
    Require(!editorSchema_.IsSupported(particleDraft_, "Emitter", "Radius") &&
        editorSchema_.GetText(particleDraft_, "Emitter", "Count", "Label") == "検証用の発生数", "Live schema reload failed");
    Require(particleDraft_ == draftBeforeSchemaChange, "Schema reload changed effect values");
    auto differentShaders = particleDraft_;
    differentShaders["Shaders"]["Update"] = "Other.CS.hlsl";
    Require(!editorSchema_.IsSupported(differentShaders, "Emitter", "Count") &&
        editorSchema_.IsSupported(differentShaders, "Emitter", "Frequency"), "Mismatched shaders reused the old schema");
    changedSchema["Version"] = 99; JsonFile::Save(schemaPath, changedSchema);
    PumpLiveReloadTest(schemaNowMs);
    Require(!editorSchema_.GetWarning(particleDraft_).empty() &&
        !editorSchema_.IsSupported(particleDraft_, "Emitter", "Count"), "Invalid schema was accepted");
    std::filesystem::remove(schemaPath); PumpLiveReloadTest(schemaNowMs);
    Require(!editorSchema_.GetWarning(particleDraft_).empty(), "Missing schema was silently accepted");
    JsonFile::Save(schemaPath, schemaBefore); PumpLiveReloadTest(schemaNowMs);
    Require(editorSchema_.Matches(particleDraft_), "Schema did not recover after repair");
    // Return the polling clock to real time for the remaining interactive-frame tests.
    nextLivePollMs_ = 0;
    Logger::Log("EffectEditor schema PASS: template inheritance, fixed controls hidden, values retained, live labels, shader mismatch, invalid/missing/repaired metadata");
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
    Require(editorSchema_.Matches(particleDraft_), "Duplicate did not inherit its editor schema");
    CreateEffect(trailId, "検証用トレイル", true, false);
    for (int frame = 0; frame < 20; ++frame) { Update(); Draw(); }
    Require(particleDraft_["Render"]["Type"] == "Trail", "New trail type is wrong");
    Require(editorSchema_.Matches(particleDraft_) && editorSchema_.IsSupported(particleDraft_, "Trail", "StartColor"), "Trail color controls were hidden");
    Require(editorSchema_.Filter(particleDraft_, "Particle", particleDraft_["Particle"]).empty(), "Unused mesh settings were shown for a trail");
    Require(!editorSchema_.IsSupported(particleDraft_, "Trail", "TextureTiling"), "Unused trail tiling was exposed");
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
