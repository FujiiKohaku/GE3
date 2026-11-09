#pragma once
#include "App/AlmaRun/Common/BaseScene.h"
#include <memory>
class AlmaTitleScene;
class AlmaGameScene;
class AlmaGameOverScene;
class AlmaClearScene;
class AlmaRunScene final : public BaseScene {
 enum class Phase {Title,Game,Over,Clear};
 Phase phase=Phase::Title;
 bool verification=false;
 int verificationFrame=0;
 std::unique_ptr<AlmaTitleScene> title;
 std::unique_ptr<AlmaGameScene> game;
 std::unique_ptr<AlmaGameOverScene> over;
 std::unique_ptr<AlmaClearScene> clear;
 void Start(Phase next);
public:
 AlmaRunScene(); ~AlmaRunScene() override;
 void Initialize() override;
 void Finalize() override;
 void Update() override;
 void Draw3D() override;
 void Draw2D() override;
 void DrawParticle() override {}
 void DrawImGui() override {}
};
