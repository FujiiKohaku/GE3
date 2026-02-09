#pragma once

#include "Camera.h"
#include "DirectXCommon.h"
#include "blendutil.h"
#include <memory>
class SkinningObject3dManager {
public:
    static SkinningObject3dManager* GetInstance();
    static void Finalize();

private:
    static std::unique_ptr<SkinningObject3dManager> instance_;
    // Singleton インターフェース

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class SkinningObject3dManager;
    };
    explicit SkinningObject3dManager(ConstructorKey);
    //=========================================
    // 初期化処理
    //=========================================
    void Initialize(DirectXCommon* dxCommon);

    //=========================================
    // 共通描画前処理
    //=========================================
    void PreDraw();

    // getter
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    Camera* GetDefaultCamera() const { return defaultCamera_; }
    BlendMode GetBlendMode() const { return static_cast<BlendMode>(currentBlendMode); }

    // setter
    void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
    void SetBlendMode(BlendMode mode)
    {
        currentBlendMode = mode;
    }

    ~SkinningObject3dManager() = default;
private:
    // Singleton：外部から new できないようにする
    SkinningObject3dManager() = default;

public:
    ~SkinningObject3dManager() = default;

private:
    SkinningObject3dManager(const SkinningObject3dManager&) = delete;
    SkinningObject3dManager& operator=(const SkinningObject3dManager&) = delete;

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Camera* defaultCamera_ = nullptr;

    // RootSignature / PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[kCountOfBlendMode];

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    int currentBlendMode = kBlendModeNormal;
};