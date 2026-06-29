#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Fog {
public:
    struct FogData {
        int32_t isEnabled;
        float startDistance;
        float endDistance;
        float curve;
        Vector3 color;
        float maxFog;
        float nearClip;
        float farClip;
        float fovY;
        float aspectRatio;
    };

    struct DebugInfo {
        float depth;
        float linearDepth;
        float viewDistance;
        float fogFactor;
    };

    void Initialize(DirectXCommon* dxCommon);
    void Update();
    void Apply(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void DrawImGui();
    void SetClipRange(float nearClip, float farClip);
    void SetCameraInfo(float nearClip, float farClip, float fovY, float aspectRatio);

    void PreDrawDepth();
    void PostDrawDepth();
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthDSVHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSRVHandle() const;

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();
    void CreateDepthResource();
    void CreateDepthDSV();
    void CreateDepthSRV();
    void CreateDebugReadbackResource();
    void CopyDebugDepth();
    void UpdateDebugInfo();
    float RestoreViewZ(float depth) const;
    float RestoreCenterViewDistance(float depth) const;
    float CalculateFogFactor(float viewDistance) const;

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    FogData* fogData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE depthDSVHandle_ {};
    D3D12_CPU_DESCRIPTOR_HANDLE depthSRVHandleCPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSRVHandleGPU_ {};
    uint32_t depthSRVIndex_ = 0;
    D3D12_RESOURCE_STATES depthResourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    Microsoft::WRL::ComPtr<ID3D12Resource> debugReadbackResource_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT debugReadbackLayout_ {};
    uint64_t debugReadbackSize_ = 0;
    DebugInfo debugInfo_ {};
    bool isDebugReadbackEnabled_ = false;

    D3D12_VIEWPORT viewport_ {};
    D3D12_RECT scissorRect_ {};
};
