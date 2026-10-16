// ゲームオーバー時の表示と入力を扱い、再挑戦やタイトルへの遷移につなげる。
#pragma once
#include "App/Scene/Common/BaseScene.h"

#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "App/Scene/Common/SceneManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include <string>
class GameOverScene : public BaseScene {

public:
    explicit GameOverScene(const std::string& stageId = "stage01")
        : stageId_(stageId) {}
    // ゲームオーバー表示と再挑戦・タイトルへ戻る操作案内を準備する。
    void Initialize()override;

    void Finalize() override;

    // 再挑戦なら同じステージを開き直し、戻る操作ならタイトルへの遷移を予約する。
    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::string stageId_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> titleObj_;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<Text> retryGuideText_;
    std::unique_ptr<Sprite> creditSprite_;
};
