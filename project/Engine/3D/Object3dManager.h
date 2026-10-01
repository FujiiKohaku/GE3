#pragma once

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/blend/blendutil.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Object3dRootParameter.h"
#include <string>
#include <unordered_map>

class ShadowMapRenderer;

class Object3dManager {
public:
    // Append the static-object VS parameters after the shared root slots.
    static constexpr uint32_t kVertexShaderParametersRootIndex =
        RootParameterIndex(Object3dRootParameter::Count);
    static constexpr uint32_t kShadowConstantsRootIndex = kVertexShaderParametersRootIndex + 1;
    static constexpr uint32_t kShadowTextureRootIndex = kShadowConstantsRootIndex + 1;
    static constexpr uint32_t kShadowReceiverRootIndex = kShadowTextureRootIndex + 1;
    void SetShadowRenderer(ShadowMapRenderer* renderer) { shadowRenderer_ = renderer; }
    void BindShadowResources(bool receiveShadow);
    // Singleton インターフェース
    static Object3dManager* GetInstance();
    static void Finalize();

    //=========================================
    // 初期化処理
    //=========================================
    void Initialize(DirectXCommon* dxCommon);

    //=========================================
    // 共通描画前処理
    //=========================================
    void PreDraw();

    // getter
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    Camera* GetDefaultCamera() const { return defaultCamera_; }
    BlendMode GetBlendMode() const { return static_cast<BlendMode>(currentBlendMode); }

    // setter
    void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
    void SetBlendMode(BlendMode mode)
    {
        currentBlendMode = mode;
    }
    void SetNormalPSO();
    void SetGlowPSO();
    // Call between frames, after waiting for GPU work to finish.
    void ReloadMaterialPipelines();
    void BindPipeline(const std::string& pixelShaderPath, bool transparent = false,
        bool transparentDepthWrite = false,
        const std::string& vertexShaderPath = "resources/Shaders/Object3D/Object3d.VS.hlsl");


    D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentTexture();
    void SetEnvironmentTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle);

private:
    static std::unique_ptr<Object3dManager> instance_;
    // Singleton：外部から new できないようにする
    Object3dManager() = default;
    Object3dManager(const Object3dManager&) = delete;
    Object3dManager& operator=(const Object3dManager&) = delete;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class Object3dManager;
    };
    explicit Object3dManager(ConstructorKey);
    ~Object3dManager();

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateMaterialPipeline(
        const std::string& pixelShaderPath, BlendMode blendMode, bool transparent,
        bool transparentDepthWrite, const std::string& vertexShaderPath);

private:
    ShadowMapRenderer* shadowRenderer_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> disabledShadowConstants_;
    uint32_t nullShadowSrv_ = 0xffffffffu;
    DirectXCommon* dxCommon_ = nullptr;
    Camera* defaultCamera_ = nullptr;

    // RootSignature / PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    // Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
    // PSOを保存する配列

    // 通常描画
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> materialPipelineCache_;

    // Glow描画
    Microsoft::WRL::ComPtr<ID3D12PipelineState> glowPipelineStates[kCountOfBlendMode];

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    D3D12_GPU_DESCRIPTOR_HANDLE environmentHandle_;
    int currentBlendMode = kBlendModeNormal;

    // 環境マップ
    D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureHandle_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE defaultEnvironmentTextureHandle_ = {};
};
