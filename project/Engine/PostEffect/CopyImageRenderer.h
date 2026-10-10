#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/EngineStruct.h"
#include "PostEffectType.h"
#include <d3d12.h>
#include <cstddef>
#include <unordered_map>
#include <wrl.h>

class CopyImageRenderer {
public:
    struct PostEffectParameter {
        float grayScaleStrength;
        float vignetteStrength;
        float outlineScale;
        float time;

        Vector2 radialBlurCenter;
        int32_t radialBlurSampleCount;
        float radialBlurWidth;

        float dissolveThreshold;
        float dissolveEdgeWidth;
        float dissolveEdgeStrength;
        float dissolvePadding;

        float radialBlurImpulseStrength;
        float pixelSize;
        float colorBrightness;
        float colorContrast;

        float colorSaturation;
        float customParameter0;
        float customParameter1;
        float customParameter2;

        float focusDepth;
        float focusRange;
        float depthOfFieldRadius;
        float motionBlurStrength;

        Vector2 motionBlurDirection;
        int32_t motionBlurSampleCount;
        float chromaticAberrationStrength;

        float lensDistortionStrength;
        float filmGrainStrength;
        float lensDirtStrength;
        float cameraShakeStrength;

        float bokehRadius;
        int32_t bokehSides;
        float fisheyeStrength;
        int32_t animationEnabled;

        float lightThreshold;
        float lightStrength;
        float lightRadius;
        float lightAngle;

        float paintProgress;
        float paintIntensity;
        float paintSeed;
        int32_t paintPatternType;
        Vector3 paintColor;
        float sonicBoomProgress;
        Vector2 sonicBoomCenter;
        float boostSparkIntensity;
        float boostSparkElapsedSeconds;
        Vector2 blackHoleCenter;
        float blackHoleRadius;
        float blackHoleStrength;
        float waterEffectIntensity;
        Vector3 paddingWaterEffect;
        float outlineNearClip;
        float outlineFarClip;
        float outlineThreshold;
        float outlineSoftness;
        float outlineNormalThreshold;
        float outlineNormalSoftness;
        float outlineNormalStrength;
        float outlineNormalPadding;
        float fxaaStrength;
        float fxaaSubpixel;
        float fxaaEdgeThreshold;
        float fxaaEdgeThresholdMin;
        int32_t toneMapEnabled;
        float toneExposure;
        float toneContrast;
        float toneSaturation;
        Matrix4x4 screenInverseProjection;
        Matrix4x4 screenCameraRotation;
        Vector4 ssaoSettings;
        Vector4 screenCameraSettings;
        Vector4 atmosphereSettings;
        Vector4 screenSunDirection;
        Vector4 screenSunColor;
        Vector4 colorFinishSettings;
    };
    void Initialize(DirectXCommon* dxCommon);
    void Draw(
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE normalTextureHandle);

    void SetPostEffectType(PostEffectType postEffectType);
    void SetOutputFormat(DXGI_FORMAT format) { outputFormat_ = format; }

    void SetExposureTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { exposureTextureHandle_ = handle; }
    void SetIndirectTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { indirectTextureHandle_ = handle; }
    void SetMaskTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);
    PostEffectParameter& GetPostEffectParameter();

private:
    void CreateRootSignature();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const std::wstring& pixelShaderPath);
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreateGraphicsPipeline(PostEffectType type);
    const wchar_t* GetPixelShaderPath(PostEffectType type) const;
    void CreatePostEffectParameterResource();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
    DXGI_FORMAT outputFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DirectXCommon* dxCommon_ = nullptr;
    PostEffectType currentPostEffectType_ = PostEffectType::Copy;

    Microsoft::WRL::ComPtr<ID3D12Resource> postEffectParameterResource_;
    PostEffectParameter* postEffectParameterData_ = nullptr;
    static constexpr size_t kDrawParameterStride = (sizeof(PostEffectParameter) + 255) & ~size_t(255);
    static constexpr size_t kDrawParameterCount = static_cast<size_t>(PostEffectType::ScreenLighting) + 1;
    Microsoft::WRL::ComPtr<ID3D12Resource> drawParameterResource_;
    unsigned char* drawParameterData_ = nullptr;
    // マスクテクスチャのGPUディスクリプタハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE exposureTextureHandle_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE indirectTextureHandle_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE maskTextureHandle_ {};
};
