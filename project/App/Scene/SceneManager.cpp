#include "SceneManager.h"
#include <cassert>



void SceneManager::Update()
{
    if (nextScene_) {

        if (scene_) {
            scene_->Finalize();
        }

        scene_ = std::move(nextScene_);

        scene_->Initialize();
    }

    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Finalize()
{
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
}

void SceneManager::Draw2D()
{
    // 実行中シーンの2D描画
    if (scene_) {
        scene_->Draw2D();
    }
}
void SceneManager::Draw3D()
{
    // 実行中シーンの3D描画
    if (scene_) {
        scene_->Draw3D();
    }
}

void SceneManager::DrawImGui()
{
    // 実行中シーンのImGui描画
    if (scene_) {
        scene_->DrawImGui();
    }
}

