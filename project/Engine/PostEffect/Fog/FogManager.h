#pragma once
#include "Engine/math/SettingsEdit.h"
#include "Engine/math/ValueValidation.h"

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/PostEffect/Fog/FogData.h"
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

    bool SetFogData(const FogData& value);
    SettingsEdit<FogData> BeginFogEdit();
    static bool IsValidParameter(const FogData& value);

private:
    void CreateConstantBuffer();
    void SetDefaultData();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    FogData* fogData_ = nullptr;
};
