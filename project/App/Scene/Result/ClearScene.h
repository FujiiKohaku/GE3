// クリア時の表示と入力を扱い、次の画面へ移るまでの結果画面を構成する。
#pragma once
#include "App/Scene/Common/BaseScene.h"

#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "App/Scene/Common/SceneManager.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
class ClearScene : public BaseScene {
public:
    // クリア表示用のモデル・文字と画面配置を準備する。
    void Initialize() override;

    void Finalize() override;

    // クリア画面の入力を受け付け、選ばれた画面への遷移を予約する。
    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;
    private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> titleObj_;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<Sprite> creditSprite_;
    std::unique_ptr<Text> returnGuideText_;
};
