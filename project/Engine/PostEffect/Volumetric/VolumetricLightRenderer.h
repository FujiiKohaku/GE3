#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <wrl.h>
class Camera;
class ShadowMapRenderer;

struct VolumetricLightParameters {
    bool enabled = true;
    Vector3 lightColor { 1.0f, 1.0f, 1.0f };
    float lightIntensity = 0.35f;
    float fogDensity = 0.003f;
    float maxDistance = 240.0f;
    float anisotropy = 0.35f;
    int32_t sampleCount = 32;
};

// One directional light. The scene owns its direction and its matching shadow map.
class VolumetricLightRenderer {
public:
    ~VolumetricLightRenderer();
    bool Initialize(DirectXCommon* dxCommon);
    void SetEnabled(bool enabled) { parameters_.enabled = enabled; }
    void SetLightColor(const Vector3& color);
    void SetLightIntensity(float intensity);
    void SetLightDirection(const Vector3& direction);
    void SetFogDensity(float density);
    void SetMaxDistance(float distance);
    void SetAnisotropy(float anisotropy);
    void SetSampleCount(int32_t samples);
    const VolumetricLightParameters& GetParameters() const { return parameters_; }
    // Must be supplied after shadow rendering, for every frame.
    void SetFrameInputs(const Camera* camera, const ShadowMapRenderer* shadows);
    bool Generate(D3D12_GPU_DESCRIPTOR_HANDLE depthHandle, bool depthReady);
    void Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void DrawImGui();
    bool IsReady() const { return ready_; }
    bool HasValidFrameInputs() const { return frameValid_; }
private:
    struct Constants {
        Matrix4x4 inverseViewProjection {};
        Matrix4x4 lightViewProjection {};
        Vector4 cameraAndDistance {};
        Vector4 lightDirectionAndDensity {};
        Vector4 lightColorAndIntensity {};
        Vector4 settings {}; // anisotropy, sample count, shadow bias, reserved
    };
    void CreateResources();
    void CreatePipelines();
    void Draw(ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE color,
        D3D12_GPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_DESCRIPTOR_HANDLE shadow);
    void Transition(D3D12_RESOURCE_STATES state);
    DirectXCommon* dxCommon_ = nullptr;
    VolumetricLightParameters parameters_;
    Constants frameConstants_ {};
    Constants* constantsData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> volumeTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> raymarchPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipeline_;
    uint32_t srvIndex_ = 0xffffffffu;
    D3D12_GPU_DESCRIPTOR_HANDLE volumeSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv_ {};
    D3D12_RESOURCE_STATES textureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    bool ready_ = false;
    bool frameValid_ = false;
    bool generated_ = false;
};
