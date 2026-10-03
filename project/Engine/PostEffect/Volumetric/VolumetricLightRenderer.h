#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <array>
#include <wrl.h>
class Camera;
class ShadowMapRenderer;
class LocalShadowRenderer;

enum class FogVolumeShape { Sphere, Box };

struct FogVolumeSettings {
    bool isEnabled = false;
    FogVolumeShape shape = FogVolumeShape::Sphere;
    Vector3 center {};
    Vector3 halfExtents { 20.0f, 10.0f, 20.0f };
    float radius = 20.0f;
    float density = 0.01f;
    float edgeSoftness = 2.0f;
};

struct LocalFogParameters {
    bool isEnabled = false;
    Vector3 color { 0.58f, 0.80f, 0.96f };
    float baseHeight = 0.0f;
    float heightDensity = 0.0f;
    float heightFalloff = 0.1f;
    float noiseScale = 0.05f;
    float noiseStrength = 0.0f;
    Vector3 noiseVelocity {};
};

struct VolumetricLightParameters {
    bool enabled = true;
    Vector3 lightColor { 1.0f, 1.0f, 1.0f };
    float lightIntensity = 0.35f;
    float fogDensity = 0.003f;
    float maxDistance = 240.0f;
    float anisotropy = 0.35f;
    int32_t sampleCount = 32;
};

struct FogPreset {
    LocalFogParameters parameters;
    std::array<FogVolumeSettings, 8> volumes {};
};

// One directional light. The scene owns its direction and its matching shadow map.
class VolumetricLightRenderer {
public:
    static constexpr uint32_t kMaxFogVolumes = 8;
    static constexpr uint32_t kMaxVolumetricSpotLights = 2;
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
    void SetLocalFogEnabled(bool isEnabled) { localFog_.isEnabled = isEnabled; }
    bool ApplyFogPreset(const FogPreset& preset);
    bool SetFogVolume(uint32_t volumeIndex, const FogVolumeSettings& settings);
    const std::array<FogVolumeSettings, kMaxFogVolumes>& GetFogVolumes() const { return fogVolumes_; }
    const LocalFogParameters& GetLocalFogParameters() const { return localFog_; }
    void ClearFogVolumes();
    void ResetLocalFog();
    bool SetHeightFog(float baseHeight, float density, float heightFalloff);
    bool SetNoiseParameters(float scale, float strength, const Vector3& velocity);
    bool SetFogColor(const Vector3& color);
    // Must be supplied after shadow rendering, for every frame.
    void SetFrameInputs(const Camera* camera, const ShadowMapRenderer* shadows,
        const LocalShadowRenderer* localShadows = nullptr);
    bool Generate(D3D12_GPU_DESCRIPTOR_HANDLE depthHandle, bool depthReady);
    void Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void DrawImGui();
    bool IsReady() const { return ready_; }
    bool HasValidFrameInputs() const { return frameValid_; }
private:
    struct FogVolumeConstants {
        Vector4 centerAndShape {};
        Vector4 extentsAndDensity {};
        Vector4 radiusAndSoftness {};
    };
    struct Constants {
        Matrix4x4 inverseViewProjection {};
        Matrix4x4 lightViewProjection {};
        Vector4 cameraAndDistance {};
        Vector4 lightDirectionAndDensity {};
        Vector4 lightColorAndIntensity {};
        Vector4 settings {}; // anisotropy, sample count, shadow bias, reserved
        Vector4 fogColorAndEnabled {};
        Vector4 heightAndVolumeCount {};
        Vector4 noiseScaleAndOffset {};
        Vector4 noiseSettings {};
        std::array<FogVolumeConstants, kMaxFogVolumes> volumes {};
        Vector4 spotCountAndBias {};
        struct SpotConstants {
            Matrix4x4 lightViewProjection {};
            Vector4 positionAndDistance {};
            Vector4 directionAndCosAngle {};
            Vector4 colorAndIntensity {};
            Vector4 decayAndFalloffAndShadow {};
        };
        std::array<SpotConstants, kMaxVolumetricSpotLights> spotLights {};
    };
    void CreateResources();
    void CreatePipelines();
    void Draw(ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE color,
        D3D12_GPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_DESCRIPTOR_HANDLE shadow);
    void Transition(D3D12_RESOURCE_STATES state);
    DirectXCommon* dxCommon_ = nullptr;
    VolumetricLightParameters parameters_;
    LocalFogParameters localFog_;
    std::array<FogVolumeSettings, kMaxFogVolumes> fogVolumes_ {};
    Vector3 noiseOffset_ {};
    Constants frameConstants_ {};
    Constants* constantsData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> volumeTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transmittanceTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> raymarchPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipeline_;
    uint32_t srvIndex_ = 0xffffffffu;
    uint32_t transmittanceSrvIndex_ = 0xffffffffu;
    D3D12_GPU_DESCRIPTOR_HANDLE transmittanceSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE volumeSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE localShadowSrv_ {};
    uint32_t localShadowFallbackSrvIndex_ = 0xffffffffu;
    D3D12_RESOURCE_STATES textureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    bool ready_ = false;
    bool frameValid_ = false;
    bool generated_ = false;
};
