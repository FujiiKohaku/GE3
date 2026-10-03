#pragma once
#include "ShadowMapRenderer.h"
#include <array>
#include <memory>
class LightManager;

struct LocalShadowConstants {
    std::array<Matrix4x4, 8> matrices {};
    Vector4 parameters { 1.0f / 512.0f, 0.00005f, 0.03f, 1.0f };
    std::array<Vector4, 8> pointSlots {};
    std::array<Vector4, 2> spotSlots {};
    LocalShadowConstants();
};

// 局所ライトの影だけを管理し、描画対象は呼び出し元が渡す。
class LocalShadowRenderer {
public:
    static constexpr uint32_t kResolution = 512;
    static constexpr uint32_t kMaxShadowedSpotLights = 2;
    static constexpr uint32_t kMaxShadowedPointLights = 1;
    static constexpr uint32_t kMaxFaces = 8;
    ~LocalShadowRenderer();
    bool Initialize(DirectXCommon* dxCommon);
    void Prepare(const LightManager& lights);
    uint32_t GetPassCount() const { return passCount_; }
    ShadowMapRenderer& GetPass(uint32_t passIndex) { return *faces_[passIndex]; }
    void Finish();
    bool IsReady() const { return isReady_; }
    bool HasValidFrame() const { return hasValidFrame_; }
    const LocalShadowConstants& GetFrameConstants() const { return frameConstants_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const { return srv_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetConstantsAddress() const { return constantsResource_->GetGPUVirtualAddress(); }
private:
    DirectXCommon* dxCommon_ = nullptr;
    std::array<std::unique_ptr<ShadowMapRenderer>, kMaxFaces> faces_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsResource_;
    LocalShadowConstants* constantsData_ = nullptr;
    LocalShadowConstants frameConstants_;
    uint32_t srvIndex_ = 0xffffffffu;
    D3D12_GPU_DESCRIPTOR_HANDLE srv_ {};
    uint32_t passCount_ = 0;
    bool isReady_ = false;
    bool hasValidFrame_ = false;
};
