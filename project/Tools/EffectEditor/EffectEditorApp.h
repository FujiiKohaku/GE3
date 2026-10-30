#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "PreviewTarget.h"
#include "EditorSourceWatch.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct EditorEffectEntry {
    std::string id;
    std::string name;
    std::filesystem::path directory;
    bool isTrail = false;
};

struct EditorHistory {
    std::vector<nlohmann::json> undo;
    std::vector<nlohmann::json> redo;
    void Push(const nlohmann::json& value);
    bool Undo(nlohmann::json& value);
    bool Redo(nlohmann::json& value);
};

class EffectEditorApp {
public:
    EffectEditorApp();
    ~EffectEditorApp();
    void Initialize();
    void Update();
    void Draw();
    void Finalize();
    void RunSmokeTest();
    void RequestClose() { shouldConfirmQuit_ = true; }
    bool ShouldQuit() const { return shouldQuit_; }

private:
    void ConfigureStyle();
    void ScanEffects();
    void SelectEffect(const std::string& id);
    void Restart();
    void ProcessPending();
    bool ToggleFullscreen();
    void ResizePreviewTargets();
    void ProcessLiveChanges(uint64_t nowMs);
    void RefreshParticleWatch(bool shouldAcknowledgeConfig = false, bool shouldAcknowledgeShaders = false);
    void RefreshPostWatch(bool shouldAcknowledgeConfig = false, bool shouldAcknowledgeShaders = false);
    void LoadExternalParticle();
    void LoadExternalPost();
    bool CheckExternalSaveConflict();
    void SetReloadError(const std::string& message);
    void RunLiveReloadTests(const std::string& meshId);
    void PumpLiveReloadTest(uint64_t& nowMs);
    void CreateEffect(const std::string& id, const std::string& displayName, bool isTrail, bool isDuplicate);
    void CreateGeometry();
    void LoadModel(const std::filesystem::path& path);
    void DrawUi();
    void DrawToolbar();
    void DrawLibrary();
    void DrawViewport();
    void DrawInspector();
    void DrawPostChain();
    void EditPostProperties(const std::string& type, bool isCompact = false);
    void DrawPopups();
    void EditProperties(nlohmann::json& value, const std::string& parent = "");
    void RequestSave();
    void RequestSelect(const std::string& id);
    void LoadPost(const std::filesystem::path& path);
    void SavePost(bool isOverwriteApproved = false);
    bool IsParticleDirty() const;
    bool IsPostDirty() const;
    void SetStatus(const std::string& text, bool isError = false);

    DirectXCommon* dx_ = nullptr;
    Camera camera_;
    ModelCommon modelCommon_;
    std::unique_ptr<Model> floorModel_;
    std::unique_ptr<Model> gridModel_;
    std::unique_ptr<Model> sampleModel_;
    std::unique_ptr<Object3d> floor_;
    std::unique_ptr<Object3d> grid_;
    std::unique_ptr<Object3d> sample_;
    std::unique_ptr<OffscreenRenderer> offscreen_;
    std::unique_ptr<PostEffectManager> post_;
    std::unique_ptr<PreviewTarget> preview_;
    std::vector<EditorEffectEntry> entries_;
    std::vector<std::filesystem::path> postFiles_;
    std::vector<std::string> texturePaths_;
    std::vector<std::filesystem::path> modelPaths_;
    std::string selectedId_;
    std::string selectedName_;
    std::string pendingSelection_;
    std::string deferredSelection_;
    std::filesystem::path postPath_ = "resources/Graphics/PostEffects/Standard.json";
    nlohmann::json particleDraft_;
    nlohmann::json particleSaved_;
    nlohmann::json particleApplied_;
    nlohmann::json postDraft_;
    nlohmann::json postSaved_;
    EditorHistory particleHistory_;
    EditorHistory postHistory_;
    EffectHandle effectHandle_ = kInvalidEffectHandle;
    int workspace_ = 0;
    int inspectorTab_ = 0;
    int selectedPost_ = 0;
    int addPostType_ = 0;
    int motion_ = 0;
    float motionRadius_ = 1.5f;
    float elapsedSeconds_ = 0;
    float speed_ = 1;
    float yaw_ = 0.35f;
    float pitch_ = 0.28f;
    float distance_ = 5;
    Vector3 target_ { 0, 0.7f, 0 };
    Vector4 background_ { 0.025f, 0.03f, 0.04f, 1 };
    bool isPlaying_ = true;
    bool isLoop_ = true;
    bool isFloorVisible_ = true;
    bool isGridVisible_ = true;
    bool isModelVisible_ = false;
    bool isPostEnabled_ = true;
    bool isViewportHovered_ = false;
    bool isStatusError_ = false;
    bool isInitialized_ = false;
    bool shouldApply_ = false;
    bool shouldRestart_ = false;
    bool shouldReload_ = false;
    bool shouldSave_ = false;
    bool shouldApplyPost_ = false;
    bool shouldCapture_ = false;
    bool shouldToggleFullscreen_ = false;
    bool shouldCreate_ = false;
    bool isNewTrail_ = false;
    bool isDuplicate_ = false;
    bool shouldOpenCreate_ = false;
    bool shouldConfirmSelection_ = false;
    bool shouldScrollToSelection_ = false;
    bool shouldConfirmCreate_ = false;
    bool shouldQuit_ = false;
    bool shouldConfirmQuit_ = false;
    bool isSmokeTest_ = false;
    bool didUseHistory_ = false;
    bool shouldOpenPostSave_ = false;
    std::filesystem::path deferredPostPath_;
    std::filesystem::path pendingPostPath_;
    std::filesystem::path pendingPostSavePath_;
    bool shouldConfirmPost_ = false;
    std::string status_ = "エフェクトを選択してください";
    char search_[128] {};
    char createId_[64] = "Effect01";
    char createName_[128] = "新しいエフェクト";
    char postSaveName_[64] = "MyPreset";
    std::filesystem::path pendingModel_;
    ID3D12Resource* lastBackBuffer_ = nullptr;
    EditorSourceWatch particleConfigWatch_;
    EditorSourceWatch particleShaderWatch_;
    EditorSourceWatch postConfigWatch_;
    EditorSourceWatch postShaderWatch_;
    std::filesystem::path particlePath_;
    nlohmann::json particleDiskConfig_;
    nlohmann::json postDiskConfig_;
    uint64_t nextLivePollMs_ = 0;
    bool isLiveReloadEnabled_ = true;
    bool hasParticleConflict_ = false;
    bool hasPostConflict_ = false;
    bool hasPendingParticleFile_ = false;
    bool hasPendingPostFile_ = false;
    bool shouldOpenExternalConflict_ = false;
    bool shouldLoadExternalParticle_ = false;
    bool shouldLoadExternalPost_ = false;
    bool shouldOverwriteExternal_ = false;
    bool shouldReloadPostShaders_ = false;
    int externalConflictTarget_ = 0;
    std::string reloadError_;
};
