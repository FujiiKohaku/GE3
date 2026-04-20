#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>

class SkyBoxManager {
public:
    static SkyBoxManager* GetInstance();
    static void Finalize();

public:
    void Initialize(DirectXCommon* dxCommon);
    void PreDraw();

    ID3D12RootSignature* GetRootSignature() const;
    ID3D12PipelineState* GetGraphicsPipelineState() const;
    DirectXCommon* GetDxCommon() const;

private:
    SkyBoxManager() = default;
    ~SkyBoxManager() = default;

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    static SkyBoxManager* instance_;

    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};