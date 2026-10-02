#pragma once
#include "ShadowCamera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
class Camera;

class ShadowMapRenderer {
public:
    ~ShadowMapRenderer();
    void Initialize(DirectXCommon* dx, uint32_t resolution);
    void Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings);
    void BeginShadowPass();
    void EndShadowPass();
    void BindObject(const Matrix4x4& world);
    void BindJellyfishObject(const Matrix4x4& world, const Vector4& animation);
    bool Intersects(const Vector3& center, float radius) const { return camera_.Intersects(center, radius); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantsAddress() const { return constantsBuffer_->GetGPUVirtualAddress(); }
    ID3D12Resource* GetDepthTexture() const { return depth_.Get(); }
    const Matrix4x4& GetLightViewProjection() const { return camera_.GetViewProjection(); }
    uint32_t GetResolution() const { return resolution_; }
private:
    DirectXCommon* dx_ = nullptr;
    ShadowCamera camera_;
    uint32_t resolution_ = 2048;
    uint32_t srvIndex_ = 0xffffffffu;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsBuffer_;
    ShadowConstants* constants_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> jellyfishPipeline_;
};
