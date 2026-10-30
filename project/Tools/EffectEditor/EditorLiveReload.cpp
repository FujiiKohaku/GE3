#include "EffectEditorApp.h"
#include "Engine/Effect/EffectConfigValidation.h"
#include "Engine/Serialization/JsonFile.h"
#include "Engine/Logger/Logger.h"

void EffectEditorApp::RefreshParticleWatch(bool shouldAcknowledgeConfig, bool shouldAcknowledgeShaders)
{
    particleConfigWatch_.SetRoots({ particlePath_ }, shouldAcknowledgeConfig);
    particleShaderWatch_.SetRoots(EffectManager::GetInstance()->GetEffectShaderPaths(selectedId_), shouldAcknowledgeShaders);
}

void EffectEditorApp::RefreshPostWatch(bool shouldAcknowledgeConfig, bool shouldAcknowledgeShaders)
{
    postConfigWatch_.SetRoots({ postPath_ }, shouldAcknowledgeConfig);
    postShaderWatch_.SetRoots(post_->GetShaderSourcePaths(), shouldAcknowledgeShaders);
}

void EffectEditorApp::SetReloadError(const std::string& message)
{
    reloadError_ = message;
    SetStatus("自動反映に失敗しました。現在の描画を維持しています。「再読み込みエラー」で詳細を確認できます", true);
    Logger::Error(message); Logger::Flush();
}

void EffectEditorApp::LoadExternalParticle()
{
    const auto disk = JsonFile::Load(particlePath_);
    auto config = disk;
    EffectConfigValidation::NormalizeRenderType(config);
    auto shaderPaths = EffectManager::GetInstance()->GetEffectShaderPaths(selectedId_);
    if (config.contains("Shaders") && config["Shaders"].is_object()) {
        for (const char* key : { "Emit", "Update", "Vertex", "Pixel" }) {
            if (!config["Shaders"].contains(key) || !config["Shaders"][key].is_string()) { continue; }
            std::filesystem::path shaderPath(config["Shaders"][key].get<std::string>());
            if (shaderPath.empty()) { continue; }
            if (!shaderPath.is_absolute() && !shaderPath.generic_string().starts_with("resources/")) { shaderPath = particlePath_.parent_path() / shaderPath; }
            shaderPaths.push_back(shaderPath);
        }
    }
    particleShaderWatch_.SetRoots(shaderPaths);
    dx_->RefreshShaderIncludeWriteTime();
    std::string error;
    if (!EffectManager::GetInstance()->ApplyEffectConfigAndRestart(selectedId_, config, error)) { throw std::runtime_error(error); }
    particleHistory_.Push(particleDraft_);
    particleDraft_ = config; particleSaved_ = config; particleApplied_ = config;
    particleDiskConfig_ = disk;
    selectedName_ = config.value("DisplayName", selectedName_);
    hasParticleConflict_ = false;
    hasPendingParticleFile_ = false;
    RefreshParticleWatch();
    if (!EffectManager::GetInstance()->IsEffectAlive(effectHandle_)) { shouldRestart_ = true; }
    reloadError_.clear();
    SetStatus("外部で変更されたパーティクル設定を反映しました");
}

void EffectEditorApp::LoadExternalPost()
{
    const auto disk = JsonFile::Load(postPath_);
    postShaderWatch_.SetRoots(post_->GetShaderSourcePaths(&disk));
    dx_->RefreshShaderIncludeWriteTime();
    std::string error;
    if (!post_->ApplyPresetConfig(disk, error)) { throw std::runtime_error(error); }
    postHistory_.Push(postDraft_);
    postDraft_ = post_->GetPresetConfig(); postSaved_ = postDraft_; postDiskConfig_ = disk;
    hasPostConflict_ = false;
    hasPendingPostFile_ = false;
    RefreshPostWatch();
    if (!isPostEnabled_) {
        auto bypass = postDraft_;
        for (auto& entry : bypass["effects"]) { entry["enabled"] = false; }
        bypass["finalPass"]["fxaa"] = false;
        if (!post_->ApplyPresetConfig(bypass, error)) { throw std::runtime_error(error); }
    }
    reloadError_.clear();
    SetStatus("外部で変更された画面効果の設定を反映しました");
}

bool EffectEditorApp::CheckExternalSaveConflict()
{
    if (shouldOverwriteExternal_) { shouldOverwriteExternal_ = false; return false; }
    if (workspace_ == 1 && !pendingPostSavePath_.empty()) {
        if (std::filesystem::exists(pendingPostSavePath_)) {
            pendingPostSavePath_.clear();
            throw std::runtime_error("保存先が外部で作成されました。別のファイル名を指定してください");
        }
        return false;
    }
    bool hasConflict = false;
    try {
        if (workspace_ == 0) { hasConflict = JsonFile::Load(particlePath_) != particleDiskConfig_; }
        else { hasConflict = JsonFile::Load(postPath_) != postDiskConfig_; }
    } catch (const std::exception&) { hasConflict = true; }
    if (!hasConflict) { return false; }
    if (workspace_ == 0) { hasParticleConflict_ = true; }
    else { hasPostConflict_ = true; }
    externalConflictTarget_ = workspace_;
    shouldOpenExternalConflict_ = true;
    SetStatus("外部の変更があります。読み込む内容か、上書き保存する内容を選択してください", true);
    return true;
}

void EffectEditorApp::ProcessLiveChanges(uint64_t nowMs)
{
    if (!isLiveReloadEnabled_ || !dx_->IsCommandListPristine() || nowMs < nextLivePollMs_) { return; }
    constexpr uint64_t kPollIntervalMs = 200;
    nextLivePollMs_ = nowMs + kPollIntervalMs;
    bool hasPostConfigChange = postConfigWatch_.Poll(nowMs);
    const bool hasPostShaderChange = postShaderWatch_.Poll(nowMs);
    const bool hasParticleShaderChange = particleShaderWatch_.Poll(nowMs);
    bool hasParticleConfigChange = particleConfigWatch_.Poll(nowMs);
    if (hasPostConfigChange) { hasPendingPostFile_ = true; }
    if (hasParticleConfigChange) { hasPendingParticleFile_ = true; }
    if (hasPostShaderChange && hasPendingPostFile_) { hasPostConfigChange = true; }
    if (hasParticleShaderChange && hasPendingParticleFile_) { hasParticleConfigChange = true; }
    const bool changes[] = { hasPostConfigChange, hasPostShaderChange, hasParticleShaderChange, hasParticleConfigChange };
    std::string failure;
    for (int action = 0; action < 4; ++action) {
        if (!changes[action]) { continue; }
        try {
            std::string error;
            if (action == 0) {
                const auto disk = JsonFile::Load(postPath_);
                if (disk == postDiskConfig_) { hasPendingPostFile_ = false; continue; }
                if (IsPostDirty()) {
                    hasPostConflict_ = true; externalConflictTarget_ = 1; shouldOpenExternalConflict_ = true;
                    SetStatus("外部のプリセット変更を保留しました。画面の未保存編集を保持しています", true);
                } else { LoadExternalPost(); }
            }
            if (action == 1) {
                if (!post_->ReloadShaders(error)) { throw std::runtime_error(error); }
                RefreshPostWatch(); reloadError_.clear();
                SetStatus("ポストエフェクトのシェーダー変更を反映しました");
            }
            if (action == 2) {
                if (!EffectManager::GetInstance()->ReloadEffectShaders(selectedId_, error)) { throw std::runtime_error(error); }
                RefreshParticleWatch(); reloadError_.clear();
                SetStatus("パーティクルのシェーダー変更を反映しました");
            }
            if (action == 3) {
                const auto disk = JsonFile::Load(particlePath_);
                if (disk == particleDiskConfig_) { hasPendingParticleFile_ = false; continue; }
                if (IsParticleDirty()) {
                    hasParticleConflict_ = true; externalConflictTarget_ = 0; shouldOpenExternalConflict_ = true;
                    SetStatus("外部の粒子設定の変更を保留しました。画面の未保存編集を保持しています", true);
                } else { LoadExternalParticle(); }
            }
        } catch (const std::exception& exception) { failure = exception.what(); }
    }
    if (!failure.empty()) { SetReloadError(failure); }
}
