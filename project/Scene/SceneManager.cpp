#include "SceneManager.h"
#include <cassert>

SceneManager::~SceneManager()
{
    // 最後のシーンが残っていたら解放
    if (scene_) {
        scene_->Finalize();
        delete scene_;
        scene_ = nullptr;
    }
}

void SceneManager::Update()
{
    // 次シーン指定がある場合
    if (nextScene_) {

        // 現在シーンを終了＆破棄
        if (scene_) {
            scene_->Finalize();
            delete scene_;
        }

        // シーン切り替え
        scene_ = nextScene_;
        nextScene_ = nullptr;

        // 新シーン初期化
        assert(scene_ != nullptr);
        scene_->Initialize();
    }

    // 実行中シーンの更新
    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Draw()
{
    if (!scene_)
        return; // 安全処理

    scene_->Draw3D();
    scene_->Draw2D();
    scene_->DrawImGui();
}
