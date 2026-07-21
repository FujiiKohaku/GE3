#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/EngineStruct.h"
#include "PostEffectType.h"
#include <d3d12.h>
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

        float boostKickStrength;
        float pixelSize;
        float colorBrightness;
        float colorContrast;

        float colorSaturation;
        float padding0;
        float padding1;
        float padding2;

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
        Vector2 paddingSonicBoom;
    };
    void Initialize(DirectXCommon* dxCommon);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle);

    void SetPostEffectType(PostEffectType postEffectType);

    void SetMaskTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);
    PostEffectParameter& GetPostEffectParameter();

private:
    void CreateRootSignature();
    // void CreateGraphicsPipeline();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const std::wstring& pixelShaderPath);
    void CreatePostEffectParameterResource();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::unordered_map<PostEffectType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
    DirectXCommon* dxCommon_ = nullptr;
    PostEffectType currentPostEffectType_ = PostEffectType::Copy;

    Microsoft::WRL::ComPtr<ID3D12Resource> postEffectParameterResource_;
    PostEffectParameter* postEffectParameterData_ = nullptr;
    // マスクテクスチャのGPUディスクリプタハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE maskTextureHandle_ {};
};
