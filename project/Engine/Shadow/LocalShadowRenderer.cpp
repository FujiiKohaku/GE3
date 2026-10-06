#include "LocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Logger/Logger.h"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace {
void CheckLocalShadow(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Local shadow resource creation failed"); }
}
void SetLightSlot(Vector4& destination, uint32_t componentIndex, float slot) {
    if (componentIndex == 0) { destination.x = slot; }
    else if (componentIndex == 1) { destination.y = slot; }
    else if (componentIndex == 2) { destination.z = slot; }
    else { destination.w = slot; }
}
}
LocalShadowConstants::LocalShadowConstants() {
    for (Vector4& slot : pointSlots) { slot = { -1.0f, -1.0f, -1.0f, -1.0f }; }
    for (Vector4& slot : spotSlots) { slot = { -1.0f, -1.0f, -1.0f, -1.0f }; }
}
LocalShadowRenderer::~LocalShadowRenderer() {
    if (srvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(srvIndex_); }
}
bool LocalShadowRenderer::Initialize(DirectXCommon* dxCommon) {
    if (isReady_) { return true; }
    if (dxCommon == nullptr) { return false; }
    dxCommon_ = dxCommon;
    try {
        D3D12_HEAP_PROPERTIES heap {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = kResolution; desc.Height = kResolution;
        desc.DepthOrArraySize = kMaxFaces; desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32_TYPELESS; desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE clear {}; clear.Format = DXGI_FORMAT_D32_FLOAT; clear.DepthStencil.Depth = 1.0f;
        CheckLocalShadow(dxCommon_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&depth_)));
        depth_->SetName(L"LocalShadows::DepthArray");
        if (!SrvManager::GetInstance()->CanAllocate()) { throw std::runtime_error("Local shadow SRV heap full"); }
        srvIndex_ = SrvManager::GetInstance()->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT; srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2DArray.ArraySize = kMaxFaces; srvDesc.Texture2DArray.MipLevels = 1;
        dxCommon_->GetDevice()->CreateShaderResourceView(depth_.Get(), &srvDesc, SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_));
        srv_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
        constantsResource_ = dxCommon_->CreateBufferResource(sizeof(LocalShadowConstants));
        CheckLocalShadow(constantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantsData_)));
        *constantsData_ = {};
        for (uint32_t faceIndex = 0; faceIndex < kMaxFaces; ++faceIndex) {
            faces_[faceIndex] = std::make_unique<ShadowMapRenderer>();
            faces_[faceIndex]->Initialize(dxCommon_, kResolution, depth_.Get(), faceIndex);
        }
        isReady_ = true;
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        isReady_ = false;
    }
    return isReady_;
}
void LocalShadowRenderer::Prepare(const LightManager& lights) {
    hasValidFrame_ = false;
    passCount_ = 0;
    frameConstants_ = {};
    if (!isReady_ || !lights.IsInitialized()) { return; }
    uint32_t spotCount = 0;
    for (uint32_t lightIndex = 0; lightIndex < LightManager::kMaxSpotLights && spotCount < kMaxShadowedSpotLights; ++lightIndex) {
        const SpotLight light = lights.GetSpotLight(lightIndex);
        if (!lights.IsSpotLightShadowEnabled(lightIndex) || light.isActive == 0 || light.intensity <= 0.0f) { continue; }
        faces_[passCount_]->UpdatePerspective(light.position, light.direction, light.distance, 2.0f * std::acos(light.cosAngle));
        SetLightSlot(frameConstants_.spotSlots[lightIndex / 4], lightIndex % 4, static_cast<float>(passCount_));
        frameConstants_.matrices[passCount_] = faces_[passCount_]->GetLightViewProjection();
        ++passCount_; ++spotCount;
    }
    const Vector3 directions[6] { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    uint32_t pointCount = 0;
    for (uint32_t lightIndex = 0; lightIndex < LightManager::kMaxPointLights && pointCount < kMaxShadowedPointLights; ++lightIndex) {
        const PointLight light = lights.GetPointLight(lightIndex);
        if (!lights.IsPointLightShadowEnabled(lightIndex) || light.isActive == 0 || light.intensity <= 0.0f) { continue; }
        SetLightSlot(frameConstants_.pointSlots[lightIndex / 4], lightIndex % 4, static_cast<float>(passCount_));
        for (const Vector3& direction : directions) {
            faces_[passCount_]->UpdatePerspective(light.position, direction, light.radius, std::numbers::pi_v<float> * 0.5f);
            frameConstants_.matrices[passCount_] = faces_[passCount_]->GetLightViewProjection();
            ++passCount_;
        }
        ++pointCount;
    }
}
void LocalShadowRenderer::Finish() {
    if (!isReady_ || passCount_ == 0) { return; }
    for (uint32_t faceIndex = 0; faceIndex < passCount_; ++faceIndex) {
        if (!faces_[faceIndex]->IsReadyForSampling()) { return; }
    }
    *constantsData_ = frameConstants_;
    hasValidFrame_ = true;
}
