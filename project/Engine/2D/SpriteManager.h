#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "SpriteMeshManager.h"
#include "SpriteMaterialManager.h"
#include "SpriteRenderManager.h"
#include <memory>

class SpriteManager {
public:
    static SpriteManager* GetInstance();
    static void Finalize();

    void Initialize(DirectXCommon* dxCommon);
    void PreDraw();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SpriteRenderManager* GetRenderManager() const { return renderManager_.get(); }
    SpriteMeshManager* GetMeshManager() const { return meshManager_.get(); }
    SpriteMaterialManager* GetMaterialManager() const { return materialManager_.get(); }

private:
    static std::unique_ptr<SpriteManager> instance_;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class SpriteManager;
    };

    explicit SpriteManager(ConstructorKey);
    ~SpriteManager() = default;

private:
    DirectXCommon* dxCommon_ = nullptr;
    std::unique_ptr<SpriteRenderManager> renderManager_;
    std::unique_ptr<SpriteMeshManager> meshManager_;
    std::unique_ptr<SpriteMaterialManager> materialManager_;
};
