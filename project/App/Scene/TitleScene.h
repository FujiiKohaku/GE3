#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/Camera/Camera.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class TitleScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    struct StageRoomObject {
        std::unique_ptr<Object3d> object;
        Vector3 basePosition {};
        Vector3 baseRotation {};
        float motionScale = 1.0f;
        float delay = 0.0f;
    };

    enum class ViewState {
        Title,
        EnteringStageSelect,
        StageSelect,
        LeavingStageSelect,
        ConfirmingStage,
        EnteringSettings,
        Settings,
        LeavingSettings,
    };

    Object3d* AddBaseObject(
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    Object3d* AddStageRoomObject(
        std::vector<StageRoomObject>& roomObjects,
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f },
        float motionScale = 1.0f,
        float delay = 0.0f);
    void InitializeLaunchBase();
    void InitializeStageRoomSets();
    void InitializeInterface();
    void InitializeStageSelection();
    void RefreshStageSelectionText();
    void StartStageRoomTransition(int direction);
    void UpdateStageRoomTransition(float deltaTime);
    void UpdateStageRoomObjectGroup(
        std::vector<StageRoomObject>& roomObjects,
        float offsetX,
        float settleAmount,
        float settleDirection);
    std::vector<StageRoomObject>* FindStageRoomObjects(const std::string& stageId);
    const std::vector<StageRoomObject>* FindStageRoomObjects(const std::string& stageId) const;
    void StartStageSelectTransition();
    void StartTitleReturn();
    void ConfirmStage();
    void StartSettingsTransition(bool opening);
    void UpdateTitleCamera(float deltaTime);
    void UpdateStageSelectTransition(float deltaTime);
    void UpdateStageSelection(float deltaTime);
    void UpdateStageConfirmation(float deltaTime);
    void UpdateSettingsTransition(float deltaTime);
    void UpdateInterface(float deltaTime);

    static float Clamp01(float value);
    static float SmoothStep(float value);
    static float EaseOutBack(float value);

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<Object3d> aircraft_;
    std::unique_ptr<Object3d> stageSwitchAircraft_;
    std::vector<std::unique_ptr<Object3d>> baseObjects_;
    std::vector<StageRoomObject> firstFlightRoomObjects_;
    std::vector<StageRoomObject> frozenPassageRoomObjects_;

    std::unique_ptr<Sprite> atmosphereTint_;
    std::unique_ptr<Sprite> settingsBackdrop_;
    std::unique_ptr<Sprite> settingsAccent_;
    std::unique_ptr<Sprite> stagePanel_;
    std::unique_ptr<Sprite> frozenFogOverlay_;
    std::unique_ptr<Sprite> stageSwitchBlackout_;
    std::unique_ptr<Text> logoText_;
    std::unique_ptr<Text> pushToStartText_;
    std::unique_ptr<Text> settingsShortcutText_;
    std::unique_ptr<Text> settingsTitleText_;
    std::unique_ptr<Text> settingsItemsText_;
    std::unique_ptr<Text> settingsBackText_;
    std::unique_ptr<Text> stageHeaderText_;
    std::unique_ptr<Text> stageNameText_;
    std::unique_ptr<Text> stageDescriptionText_;
    std::unique_ptr<Text> stagePageText_;
    std::unique_ptr<Text> stageControlsText_;

    struct StagePanelData {
        std::string id;
        std::string name;
        std::string description;
    };
    std::vector<StagePanelData> stages_;
    size_t currentStageIndex_ = 0;
    size_t previousStageIndex_ = 0;
    size_t nextStageIndex_ = 0;
    bool stageTransitionQueued_ = false;
    bool stageRoomTransitionActive_ = false;
    bool stageRoomSelectionSwapped_ = false;
    int stageRoomTransitionDirection_ = 1;
    float stageRoomTransitionTime_ = 0.0f;
    float frozenRoomAmount_ = 0.0f;

    ViewState viewState_ = ViewState::Title;
    float animationTime_ = 0.0f;
    float idleTime_ = 0.0f;
};
