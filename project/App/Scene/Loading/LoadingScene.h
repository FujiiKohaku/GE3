// 次のステージのリソースや文字を準備し、読み込みが完了したらプレイ画面へ切り替える。
#pragma once

#include "App/Scene/Common/BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"

#include <memory>

class LoadingScene : public BaseScene {
public:
    LoadingScene(std::unique_ptr<BaseScene> nextScene, bool simulationBoot = false)
        : nextScene_(std::move(nextScene)), simulationBoot_(simulationBoot) {}

    // 読み込み画面を準備し、遷移先に必要なリソースの読み込みを開始する。
    void Initialize() override;
    void Finalize() override;
    // 読み込みの完了を確認し、必要な文字も準備してから遷移先へ進む。
    void Update() override;
    // 読み込み中の表示を画面へ描画する。
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::unique_ptr<BaseScene> nextScene_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> progressBackSprite_;
    std::unique_ptr<Sprite> progressSprite_;
    bool hasDrawnFirstFrame_ = false;
    bool gameResourcesInitialized_ = false;
    bool simulationBoot_ = false;
    bool hasPreparedSceneFont_ = false;
    std::unique_ptr<Text> simulationText_;
};
