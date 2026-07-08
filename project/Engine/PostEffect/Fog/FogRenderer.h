#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>

class FogRenderer {
public:
    void Initialize(DirectXCommon* dxCommon);

    void PreDrawDepth();
    void PostDrawDepth();
    void PrepareDepthForParticleDraw();
    void Apply(
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
        D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthDSVHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSRVHandle() const;

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateDepthResource();
    void CreateDepthDSV();
    void CreateDepthSRV();
    void TransitionDepthResource(D3D12_RESOURCE_STATES nextState);

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE depthDSVHandle_ {};
    D3D12_CPU_DESCRIPTOR_HANDLE depthSRVHandleCPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRVHandleGPU_ {};
    uint32_t depthSRVIndex_ = 0;
    D3D12_RESOURCE_STATES depthResourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    D3D12_VIEWPORT viewport_ {};
    D3D12_RECT scissorRect_ {};
};
