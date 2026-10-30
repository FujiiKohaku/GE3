#include "EffectEditorApp.h"
#include "Engine/Effect/EffectConfigValidation.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Logger/Logger.h"
#include <algorithm>

namespace {
std::string FriendlyName(const std::string& id)
{
    if (id == "HandFlame") { return "手の炎"; }
    if (id == "HinokamiFlame") { return "火の竜"; }
    if (id == "HinokamiEmbers") { return "炎の火の粉"; }
    if (id == "ExplosionFlame") { return "爆発の炎"; }
    if (id == "ExplosionSmoke") { return "爆発の煙"; }
    if (id == "ExplosionSparks") { return "爆発の火花"; }
    if (id == "ExplosionFlash") { return "爆発の閃光"; }
    if (id == "Jet") { return "ジェット噴射"; }
    if (id == "IceMotes") { return "氷の粒"; }
    if (id == "OceanSpray") { return "水しぶき"; }
    if (id == "VortexTornado") { return "竜巻"; }
    if (id == "FootstepDust") { return "足元の砂ぼこり"; }
    if (id == "LandingDust") { return "着地の砂ぼこり"; }
    if (id == "Explosion") { return "爆発"; }
    if (id == "Smoke") { return "煙"; }
    if (id == "BackflipTrail") { return "回避の軌跡"; }
    if (id == "MissileTrail") { return "ミサイルの尾"; }
    if (id == "DamageHit") { return "命中の火花"; }
    if (id == "Snow") { return "雪"; }
    if (id == "FieldDemo") { return "風と渦のサンプル"; }
    return id;
}

bool IsValidId(const std::string& id)
{
    if (id.empty() || id.size() > 60) { return false; }
    for (char character : id) {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '_' || character == '-') { continue; }
        return false;
    }
    return true;
}
}

void EffectEditorApp::ScanEffects()
{
    entries_.clear(); postFiles_.clear();
    for (const auto& directory : std::filesystem::directory_iterator("resources/Effects")) {
        if (!directory.is_directory()) { continue; }
        const auto jsonPath = directory.path() / "Effect.json";
        if (!std::filesystem::is_regular_file(jsonPath) || !std::filesystem::is_regular_file(directory.path() / "Emit.CS.hlsl") ||
            !std::filesystem::is_regular_file(directory.path() / "Update.CS.hlsl")) { continue; }
        try {
            auto config = JsonFile::Load(jsonPath);
            EffectConfigValidation::NormalizeRenderType(config);
            EditorEffectEntry entry;
            entry.id = directory.path().filename().string();
            entry.directory = directory.path();
            entry.name = config.value("DisplayName", FriendlyName(entry.id));
            entry.isTrail = config.at("Render").at("Type") == "Trail";
            entries_.push_back(entry);
        } catch (const std::exception& exception) {
            Logger::Log("EffectEditor skipped invalid asset: " + jsonPath.generic_string() + " : " + exception.what());
        }
    }
    // Directory iteration is not ordered; insertion sort keeps the library stable without callbacks.
    for (size_t index = 1; index < entries_.size(); ++index) {
        size_t previous = index;
        while (previous > 0 && entries_[previous].id < entries_[previous - 1].id) {
            std::swap(entries_[previous], entries_[previous - 1]); --previous;
        }
    }
    const std::filesystem::path presets = "resources/Graphics/PostEffects";
    if (std::filesystem::is_directory(presets)) {
        for (const auto& entry : std::filesystem::directory_iterator(presets)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") { postFiles_.push_back(entry.path()); }
        }
        std::sort(postFiles_.begin(), postFiles_.end());
    }
}

void EffectEditorApp::SelectEffect(const std::string& id)
{
    const EditorEffectEntry* selected = nullptr;
    for (const auto& entry : entries_) { if (entry.id == id) { selected = &entry; break; } }
    if (selected == nullptr) { throw std::runtime_error("エフェクトが見つかりません"); }
    auto* effects = EffectManager::GetInstance();
    effects->StopAllEffects();
    const auto directory = selected->directory;
    const auto selectedPath = std::filesystem::absolute(directory / "Effect.json");
    EditorSourceWatch configWatch;
    configWatch.SetRoots({ selectedPath }, true);
    const auto disk = JsonFile::Load(selectedPath);
    dx_->RefreshShaderIncludeWriteTime();
    effects->RegisterEffect({ id, directory.generic_string(), (directory / "Effect.json").generic_string(),
        (directory / "Emit.CS.hlsl").generic_string(), (directory / "Update.CS.hlsl").generic_string() });
    std::string error;
    if (!effects->GetEffectConfig(id, particleDraft_, error)) { throw std::runtime_error(error); }
    selectedId_ = id; selectedName_ = selected->name;
    particlePath_ = selectedPath; particleConfigWatch_ = std::move(configWatch);
    particleSaved_ = particleDraft_; particleApplied_ = particleDraft_;
    particleDiskConfig_ = disk; hasParticleConflict_ = false;
    hasPendingParticleFile_ = false;
    particleHistory_ = {};
    shouldScrollToSelection_ = true;
    motion_ = 0;
    if (selected->isTrail) { motion_ = 1; }
    Restart();
    RefreshParticleWatch(false, true);
    shouldReload_ = true;
    SetStatus("読み込みました：" + selectedName_);
}

void EffectEditorApp::Restart()
{
    auto* effects = EffectManager::GetInstance();
    effects->StopAllEffects();
    if (isLoop_) { effectHandle_ = effects->PlayLoopEffect(selectedId_, { 0, 0.15f, 0 }); }
    else { effectHandle_ = effects->PlayOnceEffect(selectedId_, { 0, 0.15f, 0 }); }
    if (effectHandle_ == kInvalidEffectHandle) { throw std::runtime_error("エフェクトを再生できません"); }
    elapsedSeconds_ = 0;
}

void EffectEditorApp::RequestSelect(const std::string& id)
{
    if (id == selectedId_) { return; }
    if (IsParticleDirty()) { deferredSelection_ = id; shouldConfirmSelection_ = true; }
    else { pendingSelection_ = id; }
}

void EffectEditorApp::CreateEffect(const std::string& id, const std::string& displayName, bool isTrail, bool isDuplicate)
{
    if (!IsValidId(id)) { throw std::runtime_error("識別名には英数字・_・-を使ってください（60文字まで）"); }
    std::string sourceId = "HandFlame";
    if (isTrail) { sourceId = "BackflipTrail"; }
    if (isDuplicate) { sourceId = selectedId_; }
    const auto root = std::filesystem::weakly_canonical("resources/Effects");
    const auto destination = root / id;
    if (destination.parent_path() != root || std::filesystem::exists(destination)) { throw std::runtime_error("同じ識別名がすでに存在します"); }
    const auto source = root / sourceId;
    auto config = JsonFile::Load(source / "Effect.json");
    if (isDuplicate) { config = particleDraft_; }
    EffectConfigValidation::NormalizeRenderType(config);
    config["DisplayName"] = displayName;
    // Copy into a new directory only. Relative shader includes keep their sibling-directory meaning.
    std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive);
    JsonFile::Save(destination / "Effect.json", config);
    ScanEffects();
    SelectEffect(id);
    SetStatus("作成しました：" + displayName);
}

void EffectEditorApp::LoadPost(const std::filesystem::path& path)
{
    std::string error;
    EditorSourceWatch configWatch;
    configWatch.SetRoots({ path }, true);
    const auto disk = JsonFile::Load(path);
    if (!post_->ApplyPresetConfig(disk, error)) { throw std::runtime_error(error); }
    postPath_ = path;
    postConfigWatch_ = std::move(configWatch);
    postDraft_ = post_->GetPresetConfig(); postSaved_ = postDraft_; postHistory_ = {};
    postDiskConfig_ = disk; hasPostConflict_ = false;
    hasPendingPostFile_ = false;
    RefreshPostWatch();
    if (!isPostEnabled_) { shouldApplyPost_ = true; }
    selectedPost_ = 0;
    for (size_t index = 0; index < postDraft_["effects"].size(); ++index) {
        if (postDraft_["effects"][index]["type"] == "Bloom") { selectedPost_ = static_cast<int>(index); break; }
    }
    SetStatus("プリセットを読み込みました");
}

void EffectEditorApp::SavePost(bool isOverwriteApproved)
{
    std::string error;
    auto destination = postPath_;
    const bool isSaveAs = !pendingPostSavePath_.empty();
    if (!pendingPostSavePath_.empty()) { destination = pendingPostSavePath_; pendingPostSavePath_.clear(); }
    if (!post_->ApplyPresetConfig(postDraft_, error)) { throw std::runtime_error(error); }
    if (isSaveAs && std::filesystem::exists(destination)) { throw std::runtime_error("保存先が外部で作成されました。別の名前で保存してください"); }
    if (!isSaveAs && !isOverwriteApproved && CheckExternalSaveConflict()) { return; }
    EditorSourceWatch configWatch;
    configWatch.SetRoots({ destination }, true);
    if (!post_->SavePreset(destination.generic_string(), error)) { throw std::runtime_error(error); }
    postPath_ = destination;
    postDraft_ = post_->GetPresetConfig(); postSaved_ = postDraft_;
    postDiskConfig_ = postDraft_; hasPostConflict_ = false;
    hasPendingPostFile_ = false;
    postConfigWatch_ = std::move(configWatch);
    RefreshPostWatch();
    ScanEffects();
    if (!isPostEnabled_) { shouldApplyPost_ = true; }
    SetStatus("プリセットを保存しました：" + postPath_.filename().string());
}

void EffectEditorApp::RequestSave() { shouldSave_ = true; }

void EffectEditorApp::ProcessPending()
{
    try {
        std::string error;
        if (shouldToggleFullscreen_ && dx_->IsCommandListPristine()) {
            shouldToggleFullscreen_ = false;
            ToggleFullscreen();
            return;
        }
        auto* effects = EffectManager::GetInstance();
        if (shouldLoadExternalPost_) { shouldLoadExternalPost_ = false; LoadExternalPost(); }
        if (shouldLoadExternalParticle_) { shouldLoadExternalParticle_ = false; LoadExternalParticle(); }
        if (shouldReloadPostShaders_) {
            shouldReloadPostShaders_ = false;
            if (!post_->ReloadShaders(error)) { throw std::runtime_error(error); }
            RefreshPostWatch(); reloadError_.clear(); SetStatus("画面効果のシェーダーを再読み込みしました");
        }
        if (!pendingSelection_.empty()) {
            const auto id = pendingSelection_; pendingSelection_.clear(); SelectEffect(id);
        }
        if (shouldCreate_) { shouldCreate_ = false; CreateEffect(createId_, createName_, isNewTrail_, isDuplicate_); }
        if (!pendingModel_.empty()) { const auto path = pendingModel_; pendingModel_.clear(); LoadModel(path); }
        if (!pendingPostPath_.empty()) { const auto path = pendingPostPath_; pendingPostPath_.clear(); LoadPost(path); }
        if (shouldApply_) {
            shouldApply_ = false;
            dx_->RefreshShaderIncludeWriteTime();
            if (!effects->ApplyEffectConfigAndRestart(selectedId_, particleDraft_, error)) { throw std::runtime_error(error); }
            particleApplied_ = particleDraft_;
            RefreshParticleWatch();
            if (!effects->IsEffectAlive(effectHandle_)) { Restart(); }
            SetStatus("設定を適用しました");
        }
        if (shouldReload_ && dx_->IsCommandListPristine()) {
            shouldReload_ = false;
            if (!effects->ReloadEffectShaders(selectedId_, error)) { throw std::runtime_error(error); }
            RefreshParticleWatch(); reloadError_.clear();
            SetStatus("シェーダーを再読み込みしました");
        }
        if (shouldRestart_) { shouldRestart_ = false; Restart(); SetStatus("再開始しました"); }
        if (shouldSave_) {
            shouldSave_ = false;
            const bool isOverwriteApproved = shouldOverwriteExternal_;
            if (CheckExternalSaveConflict()) { return; }
            if (workspace_ == 1) { SavePost(isOverwriteApproved); }
            else {
                if (particleDraft_ != particleApplied_) {
                    dx_->RefreshShaderIncludeWriteTime();
                    if (!effects->ApplyEffectConfigAndRestart(selectedId_, particleDraft_, error)) { throw std::runtime_error(error); }
                    particleApplied_ = particleDraft_;
                }
                if (!isOverwriteApproved && CheckExternalSaveConflict()) { return; }
                EditorSourceWatch configWatch;
                configWatch.SetRoots({ particlePath_ }, true);
                if (!effects->SaveEffectConfig(selectedId_, error)) { throw std::runtime_error(error); }
                particleSaved_ = particleDraft_;
                particleDiskConfig_ = particleDraft_; hasParticleConflict_ = false;
                hasPendingParticleFile_ = false;
                particleConfigWatch_ = std::move(configWatch);
                RefreshParticleWatch();
                selectedName_ = particleDraft_.value("DisplayName", selectedName_);
                ScanEffects();
                SetStatus("保存しました：" + selectedName_);
            }
        }
        if (shouldApplyPost_) {
            shouldApplyPost_ = false;
            auto config = postDraft_;
            if (!isPostEnabled_) {
                for (auto& entry : config["effects"]) { entry["enabled"] = false; }
                config["finalPass"]["fxaa"] = false;
            }
            if (!post_->ApplyPresetConfig(config, error)) { throw std::runtime_error(error); }
            RefreshPostWatch();
            SetStatus("画面効果をプレビューに反映しました");
        }
        if (shouldCapture_) {
            shouldCapture_ = false;
            const auto stamp = std::to_string(GetTickCount64());
            preview_->SavePng(std::filesystem::path("runtime/captures/EffectEditor") / (selectedId_ + "_" + stamp + ".png"));
            SetStatus("プレビュー画像を保存しました：runtime/captures/EffectEditor");
        }
    } catch (const std::exception& exception) {
        shouldSave_ = false;
        reloadError_ = exception.what();
        SetStatus(std::string("操作に失敗しました：") + exception.what(), true);
    }
}
