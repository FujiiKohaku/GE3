#include "ModelPreviewApp.h"
#include "App/Scene/Common/SceneManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include <stdexcept>
#include <vector>

void ModelPreviewApp::Draw()
{
    TextureManager::GetInstance()->FlushUploads();
    SrvManager::GetInstance()->PreDraw();
    auto* commands = dx_->GetCommandList();
    postEffects_->PreDrawDepth();
    Vector4 clearColor = background_;
    if (gameLook_) { clearColor = SceneManager::GetInstance()->GetSceneClearColor(); }
    offscreen_->SetClearColor(clearColor);
    offscreen_->PreDraw(postEffects_->GetDepthDSVHandle());
    if (gameLook_) {
        SkyBoxManager::GetInstance()->PreDraw();
        skyBox_->Draw(commands);
    }
    Object3dManager::GetInstance()->PreDraw();
    try {
        for (const auto& item : items_) {
            if (!item->transparent) { item->object->Draw(); }
        }
        // Small preview scenes: select the farthest remaining transparent object without lambdas.
        std::vector<bool> drawn(items_.size(), false);
        for (std::size_t pass = 0; pass < items_.size(); ++pass) {
            int farthest = -1;
            float farthestDistance = -1.0f;
            for (std::size_t index = 0; index < items_.size(); ++index) {
                if (drawn[index] || !items_[index]->transparent) { continue; }
                Vector3 offset = ItemCenter(*items_[index]) - camera_->GetTranslate();
                float distance = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                if (distance > farthestDistance) {
                    farthestDistance = distance;
                    farthest = static_cast<int>(index);
                }
            }
            if (farthest < 0) { break; }
            items_[farthest]->object->Draw();
            drawn[farthest] = true;
        }
    } catch (const std::exception& error) { error_ = error.what(); }
    postEffects_->PostDrawDepth();
    offscreen_->PostDraw();
    dx_->PreDraw();
    postEffects_->SetBoostRadialBlurParameters(false);
    // Use the exact game's composition order, with an empty particle layer.
    postEffects_->PrepareSceneForParticleDraw(SceneManager::GetInstance(), offscreen_->GetSrvHandleGPU());
    postEffects_->PrepareDepthForParticleDraw();
    postEffects_->BeginParticleDraw();
    postEffects_->EndParticleDraw();
    postEffects_->PostDrawDepth();
    postEffects_->ApplyAfterParticleDraw(SceneManager::GetInstance());
    capture_.Prepare();
    ImGuiManager::GetInstance()->Draw();
    dx_->PostDraw();
    capture_.Complete();
}
