#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Fog/FogData.h"
#include <d3d12.h>
#include <wrl.h>

class FogManager {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void DrawImGui();

    void SetCameraInfo(float nearClip, float farClip, float fovY, float aspectRatio);

    const FogData& GetFogData() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferView() const;

private:
    void CreateConstantBuffer();
    void SetDefaultData();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    FogData* fogData_ = nullptr;
};
