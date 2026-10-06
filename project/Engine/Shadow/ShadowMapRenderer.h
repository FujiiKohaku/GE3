#pragma once
#include "ShadowCamera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "ShadowMaterialSettings.h"
#include <map>
#include <array>
class Camera;

class ShadowMapRenderer {
public:
    ~ShadowMapRenderer();
    void Initialize(DirectXCommon* dx, uint32_t resolution, ID3D12Resource* sharedDepth = nullptr, uint32_t arraySlice = 0);
    void UpdatePerspective(const Vector3& position, const Vector3& direction, float distance, float fovY);
    void Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings);
    void BeginShadowPass();
    void EndShadowPass();
    void BindObject(const Matrix4x4& world);
    void BindObject(const Matrix4x4& world, const ShadowMaterialSettings& material,
        const Vector4& parameters);
    bool Intersects(const Vector3& center, float radius) const { return camera_.Intersects(center, radius); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantsAddress() const { return constantsBuffer_->GetGPUVirtualAddress(); }
    ID3D12Resource* GetDepthTexture() const { return depth_.Get(); }
    const Matrix4x4& GetLightViewProjection() const { return camera_.GetViewProjection(); }
    uint32_t GetResolution() const { return resolution_; }
    bool IsReadyForSampling() const {
        return shadowPassComplete_ && constants_ != nullptr && constants_->options.x > 0.5f;
    }
    Vector3 GetLightDirection() const { return lightDirection_; }
    float GetDepthBias() const { return constants_->parameters.y; }
private:
    uint32_t depthSubresource_ = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bool shadowPassComplete_ = false;
    Vector3 lightDirection_ {};
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
    std::map<std::wstring, std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2>> materialPipelines_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription_ {};
};
