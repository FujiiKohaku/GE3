#pragma once
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <limits>
#include <wrl.h>
#include <array>

#include "Engine/DirectXCommon/DirectXCommon.h"

#include "Engine/SrvManager/SrvManager.h"
class OffscreenRenderer {
public:
    ~OffscreenRenderer();
    void Initialize();
    void ResizeSceneTargets();
    void PreDraw(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, bool shouldCaptureDirectionalLight = false, bool shouldCaptureReflections = false, bool shouldCaptureLocalShadows = false);
    void PostDraw();
    void SetClearColor(const Vector4& color) { clearColor_ = color; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetNormalSrvHandleGPU() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetIndirectSrvHandleGPU() const { return indirectSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetMaterialSrvHandleGPU() const { return materialSrvHandleGPU_; }
    ID3D12Resource* GetColorTexture() const { return renderTextureResource_.Get(); }
    ID3D12Resource* GetIndirectTexture() const { return indirectTextureResource_.Get(); }
    ID3D12Resource* GetNormalTexture() const { return normalTextureResource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetNormalRtvHandle() const { return normalRtvHandle_; }
    ID3D12Resource* GetDirectionalLightTexture() const { return directionalLightTexture_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetDirectionalLightSrv() const;
    bool IsDirectionalCaptureActive() const { return isDirectionalCaptureActive_; }
    uint64_t GetDirectionalLightAllocationBytes() const { return directionalLightAllocationBytes_; }
    bool IsReflectionCaptureActive() const { return isReflectionCaptureActive_; }
    ID3D12Resource* GetReflectionSurfaceTexture() const { return reflectionTextures_[0].Get(); }
    ID3D12Resource* GetReflectionEnvironmentTexture() const { return reflectionTextures_[1].Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetReflectionSurfaceSrv() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetReflectionEnvironmentSrv() const;
    uint64_t GetReflectionAllocationBytes() const { return reflectionAllocationBytes_; }
    ID3D12Resource* GetMaterialTexture() const { return materialTextureResource_.Get(); }
    bool IsLocalShadowCaptureActive() const { return isLocalShadowCaptureActive_; }
    ID3D12Resource* GetLocalLightTexture() const { return localLightTexture_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetLocalLightSrv() const;
    uint64_t GetLocalLightAllocationBytes() const { return localLightAllocationBytes_; }

private:
    void CreateLocalLightTarget();
    Microsoft::WRL::ComPtr<ID3D12Resource> localLightTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> localLightRtvHeap_;
    uint32_t localLightSrvIndex_ = UINT_MAX;
    uint64_t localLightAllocationBytes_ = 0;
    bool isLocalShadowCaptureActive_ = false;
    void CreateReflectionTargets();
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> reflectionTextures_;
    std::array<uint32_t, 2> reflectionSrvIndices_ = {UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> reflectionRtvHeap_;
    bool isReflectionCaptureActive_ = false;
    bool isReflectionRenderState_ = false;
    uint64_t reflectionAllocationBytes_ = 0;
    void CreateDirectionalLightTarget();
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> directionalLightRtvHeap_;
    uint32_t directionalLightSrvIndex_ = UINT_MAX;
    D3D12_RESOURCE_STATES directionalLightState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    bool isDirectionalCaptureActive_ = false;
    uint64_t directionalLightAllocationBytes_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,uint32_t width,uint32_t height,DXGI_FORMAT format,const Vector4& clearColor);
    
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> normalTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_;
    D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle_;

    Microsoft::WRL::ComPtr<ID3D12Resource> indirectTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE indirectRtvHandle_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE indirectSrvHandleGPU_ {};
    uint32_t indirectSrvIndex_ = kInvalidDescriptorIndex;
    D3D12_RESOURCE_STATES indirectCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE materialSrvHandleGPU_ {};
    uint32_t materialSrvIndex_ = kInvalidDescriptorIndex;
    D3D12_RESOURCE_STATES materialCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    DXGI_FORMAT format_;
    Vector4 clearColor_;

    static constexpr uint32_t kInvalidDescriptorIndex = (std::numeric_limits<uint32_t>::max)();
    uint32_t srvIndex_ = kInvalidDescriptorIndex;
    uint32_t normalSrvIndex_ = kInvalidDescriptorIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE normalSrvHandleGPU_ {};

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ {};
    D3D12_CPU_DESCRIPTOR_HANDLE normalSrvHandleCPU_ {};


    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES normalCurrentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_VIEWPORT viewport_ = {};
    D3D12_RECT scissorRect_ = {};
    // depth
    void CreateRenderTexture();
    void CreateDescriptorViews();
};
