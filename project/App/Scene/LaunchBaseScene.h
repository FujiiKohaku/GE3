#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Sprite.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/Camera/Camera.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class LaunchBaseScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    struct StagePanelData {
        std::string id;
        std::string name;
        std::string description;
    };

    Object3d* AddObject(
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    void UpdateAircraft();
    void UpdateCamera();
    void UpdateTimeOfDay(float deltaTime);
    void ApplyTimeOfDayLighting();
    void InitializeStageSelection();
    bool UpdateStageSelection();
    void RefreshStageSelectionText();

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<Object3d> aircraft_;
    std::vector<std::unique_ptr<Object3d>> baseObjects_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> controlsText_;
    std::unique_ptr<Text> speedText_;
    std::unique_ptr<Text> timeText_;
    std::unique_ptr<Sprite> atmosphereTint_;
    std::unique_ptr<Sprite> stagePanel_;
    std::unique_ptr<Text> stagePanelHeaderText_;
    std::unique_ptr<Text> stageNameText_;
    std::unique_ptr<Text> stageDescriptionText_;
    std::unique_ptr<Text> stagePageText_;
    std::unique_ptr<Text> stageConfirmText_;
    std::vector<uint32_t> baseLightHandles_;
    std::vector<Object3d*> runwayLightBulbs_;
    std::vector<StagePanelData> stages_;
    size_t currentStageIndex_ = 0;
    bool stageTransitionQueued_ = false;

    Vector3 aircraftPosition_ { 0.0f, 3.0f, -5.0f };
    float aircraftYaw_ = 0.0f;
    float timeOfDayHours_ = 7.5f;
};
