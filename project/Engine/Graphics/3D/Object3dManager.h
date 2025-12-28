#pragma once
#include "../../Graphics/Camera/Camera.h"

#include "../../Core/DirectXCommon/DirectXCommon.h"
#include "../blend/BlendUtil.h"

class Object3dManager {
public:
    static Object3dManager* instance;

    // Singleton インターフェース
    static Object3dManager* GetInstance();
    static void Finalize();

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

private:
    // Singleton：外部から new できないようにする
    Object3dManager() = default;
    ~Object3dManager() = default;

    Object3dManager(const Object3dManager&) = delete;
    Object3dManager& operator=(const Object3dManager&) = delete;

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Camera* defaultCamera_ = nullptr;

    // RootSignature / PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates[kCountOfBlendMode];

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    int currentBlendMode = kBlendModeNormal;
};