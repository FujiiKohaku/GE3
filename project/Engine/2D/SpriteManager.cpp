#include "SpriteManager.h"
#include <cassert>

std::unique_ptr<SpriteManager> SpriteManager::instance_ = nullptr;

SpriteManager* SpriteManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<SpriteManager>(ConstructorKey());
    }
    return instance_.get();
}

void SpriteManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;

    renderManager_ = std::make_unique<SpriteRenderManager>();
    renderManager_->Initialize(dxCommon_);

    meshManager_ = std::make_unique<SpriteMeshManager>();
    meshManager_->Initialize(dxCommon_);

    materialManager_ = std::make_unique<SpriteMaterialManager>();
}

void SpriteManager::PreDraw()
{
    assert(renderManager_);
    renderManager_->PreDraw();
}

SpriteManager::SpriteManager(ConstructorKey)
{
}

void SpriteManager::Finalize()
{
    instance_.reset();
}
