#include "EffectEditorApp.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/Logger/Logger.h"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
void RequireLive(bool isValid, const std::string& message)
{
    if (!isValid) { throw std::runtime_error("Live reload test: " + message); }
}
std::string ReadSource(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { throw std::runtime_error("Cannot read test source"); }
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
void WriteSource(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    if (!stream) { throw std::runtime_error("Cannot write test source"); }
}
class WorkingDirectoryScope {
public:
    explicit WorkingDirectoryScope(const std::filesystem::path& directory) : previous_(std::filesystem::current_path())
    {
        std::filesystem::current_path(directory);
    }
    ~WorkingDirectoryScope()
    {
        std::error_code error;
        std::filesystem::current_path(previous_, error);
    }
private:
    std::filesystem::path previous_;
};
}

void EffectEditorApp::PumpLiveReloadTest(uint64_t& nowMs)
{
    nowMs += 1000; ProcessLiveChanges(nowMs);
    nowMs += 1000; ProcessLiveChanges(nowMs);
}

void EffectEditorApp::RunLiveReloadTests(const std::string& meshId)
{
    SelectEffect(meshId);
    for (int frame = 0; frame < 3; ++frame) { Update(); Draw(); }
    uint64_t nowMs = GetTickCount64() + 1000;
    workspace_ = 0;
    const auto path = particlePath_;
    auto config = JsonFile::Load(path);
    isPlaying_ = false;
    config["Emitter"]["Count"] = 9;
    JsonFile::Save(path, config);
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_ && particleDraft_["Emitter"]["Count"] == 9 && !IsParticleDirty(), "External particle JSON was not applied: " + status_);
    Update(); Draw();
    isPlaying_ = true;

    particleDraft_["Emitter"]["Count"] = 17;
    config["Emitter"]["Count"] = 5;
    JsonFile::Save(path, config);
    PumpLiveReloadTest(nowMs);
    RequireLive(hasParticleConflict_ && particleDraft_["Emitter"]["Count"] == 17, "External JSON discarded the UI draft");
    shouldSave_ = true; ProcessPending();
    RequireLive(JsonFile::Load(path)["Emitter"]["Count"] == 5, "Normal save overwrote the external edit");
    shouldOpenExternalConflict_ = false;
    shouldOverwriteExternal_ = true; shouldSave_ = true; ProcessPending();
    RequireLive(!hasParticleConflict_ && JsonFile::Load(path)["Emitter"]["Count"] == 17, "Explicit overwrite did not save the UI draft");
    Update(); Draw();

    WriteSource(path, "{ incomplete JSON");
    PumpLiveReloadTest(nowMs);
    RequireLive(isStatusError_ && particleApplied_["Emitter"]["Count"] == 17, "Malformed JSON changed the running particle");
    config["Emitter"]["Count"] = 7; JsonFile::Save(path, config);
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_ && particleDraft_["Emitter"]["Count"] == 7, "Fixed JSON was not retried");
    Update(); Draw();

    isLiveReloadEnabled_ = false;
    config["Emitter"]["Count"] = 8; JsonFile::Save(path, config);
    PumpLiveReloadTest(nowMs);
    RequireLive(particleDraft_["Emitter"]["Count"] == 7, "Disabled automatic reload still applied an edit");
    shouldSave_ = true; ProcessPending();
    RequireLive(JsonFile::Load(path)["Emitter"]["Count"] == 8, "Save with auto reload disabled overwrote an external change");
    shouldOpenExternalConflict_ = false;
    isLiveReloadEnabled_ = true; PumpLiveReloadTest(nowMs);
    RequireLive(particleDraft_["Emitter"]["Count"] == 8, "Re-enabling automatic reload missed the edit");
    Update(); Draw();

    const auto emitPath = path.parent_path() / "Emit.CS.hlsl";
    const auto includePath = path.parent_path() / "LiveReloadTest.hlsli";
    const auto emitSource = ReadSource(emitPath);
    WriteSource(includePath, "#define LIVE_RELOAD_TEST 1\n");
    WriteSource(emitPath, emitSource + "\n#include \"LiveReloadTest.hlsli\"\n");
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_, "Valid particle shader edit failed: " + reloadError_);
    Update(); Draw();
    WriteSource(includePath, "#error deliberate_live_reload_test_error\n");
    PumpLiveReloadTest(nowMs);
    RequireLive(isStatusError_ && !reloadError_.empty(), "A transitive include error was not detected");
    Update(); Draw(); // The previous pipeline must remain drawable.
    WriteSource(includePath, "#define LIVE_RELOAD_TEST 2\n");
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_ && reloadError_.empty(), "A corrected include was not reloaded");
    Update(); Draw();

    const auto replacementEmitPath = path.parent_path() / "LiveEmit.CS.hlsl";
    WriteSource(replacementEmitPath, "#error deliberate_new_shader_path_error\n");
    config["Shaders"]["Emit"] = "LiveEmit.CS.hlsl"; JsonFile::Save(path, config);
    PumpLiveReloadTest(nowMs);
    RequireLive(isStatusError_ && particleDraft_["Shaders"]["Emit"] != "LiveEmit.CS.hlsl", "Invalid replacement shader changed the runtime");
    Update(); Draw();
    WriteSource(replacementEmitPath, emitSource + "\n// Newly selected shader recovery\n");
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_ && particleDraft_["Shaders"]["Emit"] == "LiveEmit.CS.hlsl", "Fixing a newly selected shader did not retry the pending JSON");
    Update(); Draw();

    const auto presetPath = std::filesystem::absolute(postPath_);
    isPostEnabled_ = false;
    auto preset = JsonFile::Load(presetPath);
    preset["parameters"]["colorSaturation"] = 0.7f; JsonFile::Save(presetPath, preset);
    PumpLiveReloadTest(nowMs);
    RequireLive(!isStatusError_ && postDraft_["parameters"]["colorSaturation"] == 0.7f, "External post preset was not applied");
    const auto bypassedPreset = post_->GetPresetConfig();
    for (const auto& entry : bypassedPreset["effects"]) { RequireLive(entry["enabled"] == false, "External preset loading re-enabled a bypassed preview"); }
    isPostEnabled_ = true; shouldApplyPost_ = true; Update(); Draw();
    postDraft_["bloom"]["intensity"] = 0.45f;
    preset["bloom"]["intensity"] = 0.9f; JsonFile::Save(presetPath, preset);
    PumpLiveReloadTest(nowMs);
    RequireLive(hasPostConflict_ && postDraft_["bloom"]["intensity"] == 0.45f, "External post preset discarded the UI draft");
    shouldOpenExternalConflict_ = false;
    shouldLoadExternalPost_ = true; ProcessPending();
    RequireLive(!hasPostConflict_ && !IsPostDirty() && postDraft_["bloom"]["intensity"] == 0.9f, "Explicit external preset load failed");
    Update(); Draw();

    // Compile intentionally broken post shaders in an isolated copy. Never edit the user's shaders.
    const auto scratch = std::filesystem::absolute(std::filesystem::path("runtime/EffectEditorSmoke") / ("LiveShaders_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64())));
    std::filesystem::create_directories(scratch / "resources");
    std::filesystem::copy("resources/Shaders", scratch / "resources/Shaders", std::filesystem::copy_options::recursive);
    const auto shaderPath = scratch / "resources/Shaders/PostEffect/ColorAdjust.PS.hlsl";
    const std::string originalShader = ReadSource(shaderPath);
    const uint64_t revision = post_->GetShaderRevision();
    postPath_ = presetPath;
    {
        WorkingDirectoryScope directory(scratch);
        postShaderWatch_.SetRoots(post_->GetShaderSourcePaths(), true);
        WriteSource(shaderPath, "#error deliberate_post_reload_test_error\n");
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(isStatusError_ && post_->GetShaderRevision() == revision, "Failed post shader reload committed a pipeline");
    Update(); Draw();
    {
        WorkingDirectoryScope directory(scratch);
        WriteSource(shaderPath, originalShader + "\n// Live reload recovery test\n");
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(!isStatusError_ && post_->GetShaderRevision() == revision + 1, "Corrected post shader did not commit: " + reloadError_);
    RequireLive(postDraft_["bloom"]["intensity"] == 0.9f, "Post shader reload reset user parameters");
    Update(); Draw();
    {
        WorkingDirectoryScope directory(scratch);
        WriteSource(shaderPath, "Texture2D<float4> unsupportedTexture : register(t30);\nfloat4 main(float4 position : SV_POSITION) : SV_TARGET { return unsupportedTexture.Load(int3(int2(position.xy), 0)); }\n");
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(isStatusError_ && post_->GetShaderRevision() == revision + 1, "An incompatible PSO replaced the previous post pipelines");
    Update(); Draw();
    {
        WorkingDirectoryScope directory(scratch);
        WriteSource(shaderPath, originalShader + "\n// Binding recovery\n");
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(!isStatusError_ && post_->GetShaderRevision() == revision + 2, "Failed to recover from incompatible shader bindings");
    Update(); Draw();
    const auto vignettePath = scratch / "resources/Shaders/PostEffect/Vignette.PS.hlsl";
    const auto vignetteSource = ReadSource(vignettePath);
    preset = JsonFile::Load(presetPath);
    preset["effects"].push_back({ { "type", "Vignette" }, { "stage", "AfterParticle" }, { "enabled", true } });
    {
        WorkingDirectoryScope directory(scratch);
        WriteSource(vignettePath, "#error deliberate_new_post_pass_error\n");
        JsonFile::Save(presetPath, preset);
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(isStatusError_ && postDraft_["effects"].size() + 1 == preset["effects"].size(), "A bad new post pass changed the preset");
    Update(); Draw();
    {
        WorkingDirectoryScope directory(scratch);
        WriteSource(vignettePath, vignetteSource + "\n// New pass recovery\n");
        PumpLiveReloadTest(nowMs);
    }
    RequireLive(!isStatusError_ && postDraft_["effects"].size() == preset["effects"].size(), "Fixing a new post pass did not retry the preset: " + reloadError_);
    Update(); Draw();
    RefreshPostWatch(true, true);
    workspace_ = 1;
    preview_->SavePng("runtime/EffectEditorSmoke/LiveReload.png", lastBackBuffer_);
    Logger::Log("EffectEditor live reload PASS: external JSON, dirty conflicts, save guard, explicit choices, malformed/repaired files, disable/enable, particle HLSL/include errors/recovery, post HLSL rollback/recovery and retained settings");
}
