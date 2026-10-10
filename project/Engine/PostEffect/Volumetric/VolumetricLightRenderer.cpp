#include "Engine/Renderer/SceneRenderResolution.h"
#include "VolumetricLightRenderer.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Shadow/LocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
namespace {
uint64_t HashVolumeBytes(uint64_t hash, const void* value, size_t sizeBytes) {
    const auto* bytes = static_cast<const uint8_t*>(value);
    for (size_t index = 0; index < sizeBytes; ++index) { hash = (hash ^ bytes[index]) * 1099511628211ull; }
    return hash;
}
void CheckVolume(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Volumetric light resource/pipeline creation failed"); }
}
bool FiniteMatrix(const Matrix4x4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix.m[row][column])) { return false; }
        }
    }
    return true;
}
float SafeVolumeValue(float value, float minimum, float maximum, float fallback) {
    if (!std::isfinite(value)) { return fallback; }
    return std::clamp(value, minimum, maximum);
}
bool IsFiniteVector(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateVolumePipeline(DirectXCommon* dxCommon_, ID3D12RootSignature* rootSignature, const std::wstring& pixelShaderPath, bool isRaymarch)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(pixelShaderPath);

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    if (isRaymarch) {
        blendDesc.IndependentBlendEnable = TRUE;
        blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature;
    pipelineStateDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
    pipelineStateDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
    pipelineStateDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
    pipelineStateDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (isRaymarch) {
        pipelineStateDesc.NumRenderTargets = 2;
        pipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R16_FLOAT;
    }
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = device->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    CheckVolume(result);

    return pipelineState;
}


}
VolumetricLightRenderer::~VolumetricLightRenderer() {
    for (uint32_t index : historySrvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
    for (uint32_t index : historyTransmittanceSrvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
    if (srvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(srvIndex_); }
    if (transmittanceSrvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(transmittanceSrvIndex_); }
    if (localShadowFallbackSrvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(localShadowFallbackSrvIndex_); }
}
bool VolumetricLightRenderer::Initialize(DirectXCommon* dxCommon) {
    if (dxCommon == nullptr) { return false; }
    dxCommon_ = dxCommon;
    try {
        CreateResources();
        CreatePipelines();
        raymarchTimer_.Initialize(); temporalTimer_.Initialize(); compositeTimer_.Initialize();
        ready_ = true;
    } catch (const std::exception& error) {
        Logger::Error(error.what());
        ready_ = false;
    }
    return ready_;
}
void VolumetricLightRenderer::ResetHistory() { hasHistory_ = false; hasUsedHistory_ = false; }
void VolumetricLightRenderer::ReadCompleted() { raymarchTimer_.ReadCompleted(); temporalTimer_.ReadCompleted(); compositeTimer_.ReadCompleted(); }
void VolumetricLightRenderer::SetEnabled(bool isEnabled) {
    if (parameters_.enabled != isEnabled) { ResetHistory(); }
    parameters_.enabled = isEnabled;
}
void VolumetricLightRenderer::SetLocalFogEnabled(bool isEnabled) {
    if (localFog_.isEnabled != isEnabled) { ResetHistory(); }
    localFog_.isEnabled = isEnabled;
}
void VolumetricLightRenderer::SetTemporalEnabled(bool isEnabled) {
    if (parameters_.shouldUseTemporalHistory != isEnabled) { ResetHistory(); }
    parameters_.shouldUseTemporalHistory = isEnabled;
}
bool VolumetricLightRenderer::SetHistoryWeight(float weight) {
    if (!std::isfinite(weight) || weight < 0 || weight > 0.95f) { return false; }
    if (parameters_.historyWeight != weight) { ResetHistory(); }
    parameters_.historyWeight = weight; return true;
}
bool VolumetricLightRenderer::SetScatteringAlbedo(float albedo) {
    if (!std::isfinite(albedo) || albedo < 0 || albedo > 1) { return false; }
    if (parameters_.scatteringAlbedo != albedo) { ResetHistory(); }
    parameters_.scatteringAlbedo = albedo; return true;
}
bool VolumetricLightRenderer::SetQuality(VolumetricQuality quality) {
    if (quality == VolumetricQuality::Low) { SetSampleCount(16); }
    else if (quality == VolumetricQuality::Medium) { SetSampleCount(32); }
    else if (quality == VolumetricQuality::High) { SetSampleCount(64); }
    else if (quality != VolumetricQuality::Custom) { return false; }
    parameters_.quality = quality; return true;
}
ID3D12Resource* VolumetricLightRenderer::GetFilteredTransmittanceTexture() const {
    if (parameters_.shouldUseTemporalHistory && hasHistory_) { return historyTransmittanceTextures_[historyIndex_].Get(); }
    return transmittanceTexture_.Get();
}
void VolumetricLightRenderer::SetLightColor(const Vector3& color) {
    parameters_.lightColor = { SafeVolumeValue(color.x, 0.0f, 4.0f, 1.0f),
        SafeVolumeValue(color.y, 0.0f, 4.0f, 1.0f), SafeVolumeValue(color.z, 0.0f, 4.0f, 1.0f) };
}
void VolumetricLightRenderer::SetLightIntensity(float value) { parameters_.lightIntensity = SafeVolumeValue(value, 0.0f, 4.0f, 0.0f); }
void VolumetricLightRenderer::SetFogDensity(float value) { parameters_.fogDensity = SafeVolumeValue(value, 0.0f, 0.05f, 0.0f); }
void VolumetricLightRenderer::SetMaxDistance(float value) { parameters_.maxDistance = SafeVolumeValue(value, 1.0f, 1000.0f, 240.0f); }
void VolumetricLightRenderer::SetAnisotropy(float value) { parameters_.anisotropy = SafeVolumeValue(value, -0.8f, 0.8f, 0.0f); }
void VolumetricLightRenderer::SetSampleCount(int32_t samples) {
    int32_t sampleCount = std::clamp(samples, 8, 64);
    if (parameters_.sampleCount != sampleCount) { ResetHistory(); }
    parameters_.sampleCount = sampleCount; parameters_.quality = VolumetricQuality::Custom;
}
bool VolumetricLightRenderer::SetFogVolume(uint32_t volumeIndex, const FogVolumeSettings& settings) {
    if (volumeIndex >= kMaxFogVolumes || !IsFiniteVector(settings.center) || !IsFiniteVector(settings.halfExtents) ||
        !std::isfinite(settings.radius) || !std::isfinite(settings.density) || !std::isfinite(settings.edgeSoftness)) { return false; }
    if (settings.shape != FogVolumeShape::Sphere && settings.shape != FogVolumeShape::Box) { return false; }
    if (settings.radius <= 0.0f || settings.radius > 10000.0f || settings.halfExtents.x <= 0.0f ||
        settings.halfExtents.y <= 0.0f || settings.halfExtents.z <= 0.0f || settings.halfExtents.x > 10000.0f ||
        settings.halfExtents.y > 10000.0f || settings.halfExtents.z > 10000.0f || settings.density < 0.0f ||
        settings.density > 0.05f || settings.edgeSoftness <= 0.0f) { return false; }
    float minimumExtent = settings.radius;
    if (settings.shape == FogVolumeShape::Box) {
        minimumExtent = (std::min)({ settings.halfExtents.x, settings.halfExtents.y, settings.halfExtents.z });
    }
    if (settings.edgeSoftness > minimumExtent) { return false; }
    fogVolumes_[volumeIndex] = settings;
    return true;
}
void VolumetricLightRenderer::ClearFogVolumes() { fogVolumes_ = {}; }
bool VolumetricLightRenderer::ApplyFogPreset(const FogPreset& preset) {
    const LocalFogParameters previousParameters = localFog_;
    const auto previousVolumes = fogVolumes_;
    const LocalFogParameters& settings = preset.parameters;
    bool isValid = SetFogColor(settings.color) &&
        SetHeightFog(settings.baseHeight, settings.heightDensity, settings.heightFalloff) &&
        SetNoiseParameters(settings.noiseScale, settings.noiseStrength, settings.noiseVelocity);
    for (uint32_t volumeIndex = 0; isValid && volumeIndex < kMaxFogVolumes; ++volumeIndex) {
        isValid = SetFogVolume(volumeIndex, preset.volumes[volumeIndex]);
    }
    if (!isValid) {
        localFog_ = previousParameters;
        fogVolumes_ = previousVolumes;
        return false;
    }
    localFog_.isEnabled = settings.isEnabled;
    noiseOffset_ = {};
    frameValid_ = false;
    generated_ = false;
    return true;
}
void VolumetricLightRenderer::ResetLocalFog() {
    ClearFogVolumes();
    localFog_ = {};
    noiseOffset_ = {};
    frameValid_ = false;
    generated_ = false;
}
bool VolumetricLightRenderer::SetHeightFog(float baseHeight, float density, float heightFalloff) {
    if (!std::isfinite(baseHeight) || !std::isfinite(density) || !std::isfinite(heightFalloff) ||
        density < 0.0f || density > 0.05f || heightFalloff <= 0.0f || heightFalloff > 10.0f) { return false; }
    localFog_.baseHeight = baseHeight;
    localFog_.heightDensity = density;
    localFog_.heightFalloff = heightFalloff;
    return true;
}
bool VolumetricLightRenderer::SetNoiseParameters(float scale, float strength, const Vector3& velocity) {
    if (!std::isfinite(scale) || !std::isfinite(strength) || !IsFiniteVector(velocity) || scale <= 0.0f ||
        scale > 10.0f || strength < 0.0f || strength > 1.0f || std::abs(velocity.x) > 100.0f ||
        std::abs(velocity.y) > 100.0f || std::abs(velocity.z) > 100.0f) { return false; }
    localFog_.noiseScale = scale;
    localFog_.noiseStrength = strength;
    localFog_.noiseVelocity = velocity;
    return true;
}
bool VolumetricLightRenderer::SetFogColor(const Vector3& color) {
    if (!IsFiniteVector(color) || color.x < 0.0f || color.y < 0.0f || color.z < 0.0f ||
        color.x > 4.0f || color.y > 4.0f || color.z > 4.0f) { return false; }
    localFog_.color = color;
    return true;
}
void VolumetricLightRenderer::SetLightDirection(const Vector3& direction) {
    float length = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!std::isfinite(length) || length < 0.000001f) { return; }
    if (!LightManager::GetInstance()->IsInitialized()) { return; }
    float inverseLength = 1.0f / std::sqrt(length);
    LightManager::GetInstance()->SetDirection({ direction.x * inverseLength, direction.y * inverseLength, direction.z * inverseLength });
    // The next shadow pass must use the changed direction before this effect can run.
    frameValid_ = false;
}
void VolumetricLightRenderer::SetFrameInputs(const Camera* camera, const ShadowMapRenderer* shadows,
    const LocalShadowRenderer* localShadows, uint64_t sceneRevision) {
    if (frameValid_) { ResetHistory(); }
    ReadCompleted(); frameCamera_ = camera; frameSceneRevision_ = sceneRevision;
    frameValid_ = false;
    generated_ = false;
    shadowSrv_ = {};
    localShadowSrv_ = {};
    frameConstants_.spotCountAndBias = {};
    frameConstants_.spotLights = {}; frameConstants_.volumes = {};
    if (!ready_ || camera == nullptr) { ResetHistory(); return; }
    bool hasValidShadow = false;
    Vector3 direction { 0.0f, -1.0f, 0.0f };
    frameConstants_.lightColorAndIntensity = {};
    frameConstants_.lightViewProjection = {};
    frameConstants_.settings = { parameters_.anisotropy, static_cast<float>(parameters_.sampleCount), 0.00025f, 0.0f };
    if (shadows != nullptr && shadows->IsReadyForSampling() && LightManager::GetInstance()->IsInitialized()) {
        const auto light = LightManager::GetInstance()->GetDirectionalLight();
        direction = shadows->GetLightDirection();
        const float sunLength = light.direction.x * light.direction.x + light.direction.y * light.direction.y + light.direction.z * light.direction.z;
        const float shadowLength = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
        if (std::isfinite(sunLength) && std::isfinite(shadowLength) && sunLength > 0.000001f && shadowLength > 0.000001f &&
            std::isfinite(light.intensity) && FiniteMatrix(shadows->GetLightViewProjection())) {
            const float alignment = (direction.x * light.direction.x + direction.y * light.direction.y + direction.z * light.direction.z) / std::sqrt(shadowLength * sunLength);
            if (std::isfinite(alignment) && alignment >= 0.9999f && shadows->GetSrv().ptr != 0) {
                const float inverseLength = 1.0f / std::sqrt(shadowLength);
                direction = { direction.x * inverseLength, direction.y * inverseLength, direction.z * inverseLength };
                frameConstants_.lightViewProjection = shadows->GetLightViewProjection();
                frameConstants_.lightColorAndIntensity = {
                    SafeVolumeValue(light.color.x, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.x,
                    SafeVolumeValue(light.color.y, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.y,
                    SafeVolumeValue(light.color.z, 0.0f, 8.0f, 0.0f) * parameters_.lightColor.z,
                    SafeVolumeValue(light.intensity, 0.0f, 8.0f, 0.0f) * parameters_.lightIntensity };
                frameConstants_.settings.z = SafeVolumeValue(shadows->GetDepthBias(), 0.0f, 0.01f, 0.00025f);
                frameConstants_.settings.w = 1.0f;
                shadowSrv_ = shadows->GetSrv();
                hasValidShadow = true;
            }
        }
    }
    if (LightManager::GetInstance()->IsInitialized()) {
        const LightManager* lights = LightManager::GetInstance();
        uint32_t spotCount = 0;
        for (uint32_t lightIndex = 0; lightIndex < LightManager::kMaxSpotLights &&
            spotCount < kMaxVolumetricSpotLights; ++lightIndex) {
            if (!lights->IsSpotLightVolumetricEnabled(lightIndex)) { continue; }
            const SpotLight light = lights->GetSpotLight(lightIndex);
            if (light.isActive == 0 || light.intensity <= 0.0f || light.distance <= 0.0f) { continue; }
            int shadowFace = -1;
            if (lights->IsSpotLightShadowEnabled(lightIndex)) {
                // A requested shadow must be complete this frame; never leak light through walls on failure.
                if (localShadows == nullptr || !localShadows->HasValidFrame() || localShadows->GetSrv().ptr == 0) { continue; }
                const Vector4& slots = localShadows->GetFrameConstants().spotSlots[lightIndex / 4];
                float slot = slots.x;
                if (lightIndex % 4 == 1) { slot = slots.y; }
                else if (lightIndex % 4 == 2) { slot = slots.z; }
                else if (lightIndex % 4 == 3) { slot = slots.w; }
                shadowFace = static_cast<int>(slot);
                if (shadowFace < 0 || shadowFace >= static_cast<int>(localShadows->GetPassCount())) { continue; }
                localShadowSrv_ = localShadows->GetSrv();
                frameConstants_.spotCountAndBias.y = localShadows->GetFrameConstants().parameters.y;
            }
            Constants::SpotConstants& destination = frameConstants_.spotLights[spotCount];
            if (shadowFace >= 0) {
                destination.lightViewProjection = localShadows->GetFrameConstants().matrices[shadowFace];
            }
            const Matrix4x4& cameraWorld = camera->GetWorldMatrix();
            const Vector3 position { cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2] };
            destination.positionAndDistance = { light.position.x - position.x,
                light.position.y - position.y, light.position.z - position.z, light.distance };
            destination.directionAndCosAngle = { light.direction.x, light.direction.y, light.direction.z, light.cosAngle };
            destination.colorAndIntensity = { light.color.x, light.color.y, light.color.z,
                light.intensity * parameters_.lightIntensity };
            destination.decayAndFalloffAndShadow = { light.decay, light.cosFalloffStart, static_cast<float>(shadowFace), 0.0f };
            ++spotCount;
        }
        frameConstants_.spotCountAndBias.x = static_cast<float>(spotCount);
    }
    if (!hasValidShadow && !localFog_.isEnabled && frameConstants_.spotCountAndBias.x == 0.0f) { ResetHistory(); return; }
    if (!hasValidShadow) { direction = { 0.0f, -1.0f, 0.0f }; }
    if (camera->GetNearClip() <= 0.0f || camera->GetFarClip() <= camera->GetNearClip()) { ResetHistory(); return; }
    const auto& world = camera->GetWorldMatrix();
    const auto& projection = camera->GetProjectionMatrix();
    if (!FiniteMatrix(world) || !FiniteMatrix(projection) ||
        !FiniteMatrix(camera->GetViewProjectionMatrix())) { ResetHistory(); return; }
    // Validate the camera basis without its translation. A translated VP inverse
    // loses precision and falsely toggles this effect as the camera moves.
    const double determinant =
        double(world.m[0][0]) * (double(world.m[1][1]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][1]) -
        double(world.m[0][1]) * (double(world.m[1][0]) * world.m[2][2] - double(world.m[1][2]) * world.m[2][0]) +
        double(world.m[0][2]) * (double(world.m[1][0]) * world.m[2][1] - double(world.m[1][1]) * world.m[2][0]);
    if (!std::isfinite(determinant) || std::abs(determinant) < 0.00000001) { ResetHistory(); return; }
    const auto inverseProjection = MatrixMath::Inverse(projection);
    if (!FiniteMatrix(inverseProjection)) { ResetHistory(); return; }
    const auto identity = MatrixMath::Multiply(projection, inverseProjection);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float expected = 0.0f;
            if (row == column) { expected = 1.0f; }
            if (!std::isfinite(identity.m[row][column]) || std::abs(identity.m[row][column] - expected) > 0.001f) { ResetHistory(); return; }
        }
    }
    Matrix4x4 relativeWorld = world;
    relativeWorld.m[3][0] = 0.0f; relativeWorld.m[3][1] = 0.0f; relativeWorld.m[3][2] = 0.0f;
    frameConstants_.inverseViewProjection = MatrixMath::Multiply(inverseProjection, relativeWorld);
    if (!FiniteMatrix(frameConstants_.inverseViewProjection)) { ResetHistory(); return; }
    frameConstants_.cameraAndDistance = { world.m[3][0], world.m[3][1], world.m[3][2], parameters_.maxDistance };
    frameConstants_.lightDirectionAndDensity = { direction.x, direction.y, direction.z, parameters_.fogDensity };
    frameConstants_.fogColorAndEnabled = { localFog_.color.x, localFog_.color.y, localFog_.color.z, 0.0f };
    if (localFog_.isEnabled) { frameConstants_.fogColorAndEnabled.w = 1.0f; }
    frameConstants_.heightAndVolumeCount = { localFog_.baseHeight, localFog_.heightDensity, localFog_.heightFalloff, 0.0f };
    const float deltaSeconds = SafeVolumeValue(TimeManager::GetInstance()->GetDeltaTime(), 0.0f, 0.1f, 0.0f);
    // 周期ノイズの座標だけを折り返し、長時間実行時の精度低下を防ぐ。
    noiseOffset_.x = std::fmod(noiseOffset_.x + localFog_.noiseVelocity.x * localFog_.noiseScale * deltaSeconds, 256.0f);
    noiseOffset_.y = std::fmod(noiseOffset_.y + localFog_.noiseVelocity.y * localFog_.noiseScale * deltaSeconds, 256.0f);
    noiseOffset_.z = std::fmod(noiseOffset_.z + localFog_.noiseVelocity.z * localFog_.noiseScale * deltaSeconds, 256.0f);
    frameConstants_.noiseScaleAndOffset = { localFog_.noiseScale, noiseOffset_.x, noiseOffset_.y, noiseOffset_.z };
    frameConstants_.noiseSettings = { localFog_.noiseStrength, 0.0f, 0.0f, 0.0f };
    uint32_t volumeCount = 0;
    for (const FogVolumeSettings& volume : fogVolumes_) {
        if (!localFog_.isEnabled || !volume.isEnabled || volume.density <= 0.0f) { continue; }
        const Vector3 relativeCenter { volume.center.x - world.m[3][0], volume.center.y - world.m[3][1], volume.center.z - world.m[3][2] };
        float boundingRadius = volume.radius;
        if (volume.shape == FogVolumeShape::Box) {
            boundingRadius = std::sqrt(volume.halfExtents.x * volume.halfExtents.x + volume.halfExtents.y * volume.halfExtents.y + volume.halfExtents.z * volume.halfExtents.z);
        }
        const float centerDistance = std::sqrt(relativeCenter.x * relativeCenter.x + relativeCenter.y * relativeCenter.y + relativeCenter.z * relativeCenter.z);
        if (centerDistance > parameters_.maxDistance + boundingRadius) { continue; }
        FogVolumeConstants& destination = frameConstants_.volumes[volumeCount];
        float shape = 0.0f;
        if (volume.shape == FogVolumeShape::Box) { shape = 1.0f; }
        destination.centerAndShape = { relativeCenter.x, relativeCenter.y, relativeCenter.z, shape };
        destination.extentsAndDensity = { volume.halfExtents.x, volume.halfExtents.y, volume.halfExtents.z, volume.density };
        destination.radiusAndSoftness = { volume.radius, volume.edgeSoftness, 0.0f, 0.0f };
        ++volumeCount;
    }
    frameConstants_.heightAndVolumeCount.w = static_cast<float>(volumeCount);
    frameValid_ = true;

}
void VolumetricLightRenderer::CreateResources() {
    auto* device = dxCommon_->GetDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors = 6;
    CheckVolume(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)));
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = SceneRenderResolution::GetWidth() / 2; desc.Height = SceneRenderResolution::GetHeight() / 2;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear {}; clear.Format = desc.Format;
    CheckVolume(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, textureState_, &clear, IID_PPV_ARGS(&volumeTexture_)));
    allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
    volumeTexture_->SetName(L"VolumetricLight::HalfResolutionScattering");
    device->CreateRenderTargetView(volumeTexture_.Get(), nullptr, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    if (!SrvManager::GetInstance()->CanAllocate()) { throw std::runtime_error("Volumetric light SRV heap full"); }
    if (srvIndex_ == UINT_MAX) { srvIndex_ = SrvManager::GetInstance()->Allocate(); }
    SrvManager::GetInstance()->CreateSRVforTexture2D(srvIndex_, volumeTexture_.Get(), desc.Format, 1);
    volumeSrv_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    desc.Format = DXGI_FORMAT_R16_FLOAT;
    clear.Format = desc.Format;
    clear.Color[0] = 1.0f;
    CheckVolume(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, textureState_, &clear, IID_PPV_ARGS(&transmittanceTexture_)));
    allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
    transmittanceTexture_->SetName(L"VolumetricLight::HalfResolutionTransmittance");
    auto transmittanceRtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    transmittanceRtv.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(transmittanceTexture_.Get(), nullptr, transmittanceRtv);
    if (!SrvManager::GetInstance()->CanAllocate()) { throw std::runtime_error("Fog transmittance SRV heap full"); }
    if (transmittanceSrvIndex_ == UINT_MAX) { transmittanceSrvIndex_ = SrvManager::GetInstance()->Allocate(); }
    SrvManager::GetInstance()->CreateSRVforTexture2D(transmittanceSrvIndex_, transmittanceTexture_.Get(), desc.Format, 1);
    transmittanceSrv_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(transmittanceSrvIndex_);
    if (!SrvManager::GetInstance()->CanAllocate()) { throw std::runtime_error("Local fog shadow SRV heap full"); }
    if (localShadowFallbackSrvIndex_ == UINT_MAX) { localShadowFallbackSrvIndex_ = SrvManager::GetInstance()->Allocate(); }
    D3D12_SHADER_RESOURCE_VIEW_DESC nullShadowDesc {};
    nullShadowDesc.Format = DXGI_FORMAT_R32_FLOAT;
    nullShadowDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    nullShadowDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullShadowDesc.Texture2DArray.ArraySize = LocalShadowRenderer::kMaxFaces;
    nullShadowDesc.Texture2DArray.MipLevels = 1;
    device->CreateShaderResourceView(nullptr, &nullShadowDesc,
        SrvManager::GetInstance()->GetCPUDescriptorHandle(localShadowFallbackSrvIndex_));
    D3D12_HEAP_PROPERTIES uploadHeap {}; uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    auto buffer = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Constants) + 255) & ~size_t(255));
    CheckVolume(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantsResource_)));
    CheckVolume(constantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantsData_)));
}
void VolumetricLightRenderer::CreatePipelines() {
    D3D12_DESCRIPTOR_RANGE ranges[7] {};
    D3D12_ROOT_PARAMETER root[8] {};
    for (uint32_t index = 0; index < 4; ++index) {
        ranges[index].BaseShaderRegister = index; ranges[index].NumDescriptors = 1;
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        root[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        root[index].DescriptorTable = { 1, &ranges[index] };
    }
    root[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; root[4].Descriptor.ShaderRegister = 0;
    ranges[4].BaseShaderRegister = 4;
    ranges[4].NumDescriptors = 1;
    ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    root[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root[5].DescriptorTable = { 1, &ranges[4] };
    for (uint32_t index = 5; index < 7; ++index) {
        ranges[index].BaseShaderRegister = index; ranges[index].NumDescriptors = 1;
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        root[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        root[index + 1].DescriptorTable = {1, &ranges[index]};
    }
    D3D12_STATIC_SAMPLER_DESC samplers[2] {};
    for (uint32_t index = 0; index < 2; ++index) {
        samplers[index].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[index].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[index].ShaderRegister = index; samplers[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[index].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    }
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.NumParameters = 8; desc.pParameters = root;
    desc.NumStaticSamplers = 2; desc.pStaticSamplers = samplers;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errors;
    CheckVolume(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
    CheckVolume(dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)));
    raymarchPipeline_ = CreateVolumePipeline(dxCommon_, rootSignature_.Get(), L"resources/Shaders/PostEffect/Volumetric/Raymarch.PS.hlsl", true);
    temporalPipeline_ = CreateVolumePipeline(dxCommon_, rootSignature_.Get(), L"resources/Shaders/PostEffect/Volumetric/Temporal.PS.hlsl", true);
    compositePipeline_ = CreateVolumePipeline(dxCommon_, rootSignature_.Get(), L"resources/Shaders/PostEffect/Volumetric/Composite.PS.hlsl", false);
}
void VolumetricLightRenderer::CreateHistoryResources() {
    if (historyTextures_[0]) { return; }
    auto* device = dxCommon_->GetDevice(); auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (!SrvManager::GetInstance()->CanAllocate(4)) { throw std::runtime_error("Volumetric history descriptors exhausted"); }
    for (uint32_t index = 0; index < 2; ++index) {
        auto description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, SceneRenderResolution::GetWidth() / 2,
            SceneRenderResolution::GetHeight() / 2, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        D3D12_CLEAR_VALUE clear = {}; clear.Format = description.Format;
        CheckVolume(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&historyTextures_[index])));
        allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
        if (historySrvIndices_[index] == UINT_MAX) { historySrvIndices_[index] = SrvManager::GetInstance()->Allocate(); }
        SrvManager::GetInstance()->CreateSRVforTexture2D(historySrvIndices_[index], historyTextures_[index].Get(), description.Format, 1);
        auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (2 + index * 2) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(historyTextures_[index].Get(), nullptr, rtv);
        description.Format = DXGI_FORMAT_R16_FLOAT; clear.Format = description.Format; clear.Color[0] = 1;
        CheckVolume(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&historyTransmittanceTextures_[index])));
        allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
        if (historyTransmittanceSrvIndices_[index] == UINT_MAX) { historyTransmittanceSrvIndices_[index] = SrvManager::GetInstance()->Allocate(); }
        SrvManager::GetInstance()->CreateSRVforTexture2D(historyTransmittanceSrvIndices_[index], historyTransmittanceTextures_[index].Get(), description.Format, 1);
        rtv.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(historyTransmittanceTextures_[index].Get(), nullptr, rtv);
    }
}
void VolumetricLightRenderer::RenderTemporal() {
    uint32_t writeIndex = 1 - historyIndex_;
    auto scatteringBarrier = CD3DX12_RESOURCE_BARRIER::Transition(historyTextures_[writeIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto transmissionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(historyTransmittanceTextures_[writeIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_RESOURCE_BARRIER barriers[] = {scatteringBarrier, transmissionBarrier};
    auto* commandList = dxCommon_->GetCommandList(); commandList->ResourceBarrier(2, barriers);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (2 + writeIndex * 2) * dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    commandList->OMSetRenderTargets(2, &rtv, TRUE, nullptr);
    temporalTimer_.Begin(); Draw(temporalPipeline_.Get(), depthSrv_, depthSrv_, volumeSrv_); temporalTimer_.End();
    scatteringBarrier = CD3DX12_RESOURCE_BARRIER::Transition(historyTextures_[writeIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transmissionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(historyTransmittanceTextures_[writeIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const D3D12_RESOURCE_BARRIER completedBarriers[] = {scatteringBarrier, transmissionBarrier};
    commandList->ResourceBarrier(2, completedBarriers); historyIndex_ = writeIndex; hasHistory_ = true;
}
void VolumetricLightRenderer::Transition(D3D12_RESOURCE_STATES state) {
    if (textureState_ == state) { return; }
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(volumeTexture_.Get(), textureState_, state);
    const auto transmittanceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(transmittanceTexture_.Get(), textureState_, state);
    const D3D12_RESOURCE_BARRIER barriers[] = { barrier, transmittanceBarrier };
    dxCommon_->GetCommandList()->ResourceBarrier(2, barriers);
    textureState_ = state;
}
void VolumetricLightRenderer::Draw(ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE color,
    D3D12_GPU_DESCRIPTOR_HANDLE depth, D3D12_GPU_DESCRIPTOR_HANDLE shadow) {
    auto* cmd = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(rootSignature_.Get()); cmd->SetPipelineState(pipeline);
    cmd->SetGraphicsRootDescriptorTable(0, color); cmd->SetGraphicsRootDescriptorTable(1, depth);
    cmd->SetGraphicsRootDescriptorTable(2, shadow);
    D3D12_GPU_DESCRIPTOR_HANDLE transmittance = depth;
    if (pipeline == compositePipeline_.Get() || pipeline == temporalPipeline_.Get()) { transmittance = transmittanceSrv_; }
    if (pipeline == compositePipeline_.Get() && parameters_.shouldUseTemporalHistory && hasHistory_) {
        transmittance = SrvManager::GetInstance()->GetGPUDescriptorHandle(historyTransmittanceSrvIndices_[historyIndex_]);
    }
    cmd->SetGraphicsRootDescriptorTable(3, transmittance);
    cmd->SetGraphicsRootConstantBufferView(4, constantsResource_->GetGPUVirtualAddress());
    D3D12_GPU_DESCRIPTOR_HANDLE localShadow = localShadowSrv_;
    if (localShadow.ptr == 0) {
        localShadow = SrvManager::GetInstance()->GetGPUDescriptorHandle(localShadowFallbackSrvIndex_);
    }
    cmd->SetGraphicsRootDescriptorTable(5, localShadow);
    D3D12_GPU_DESCRIPTOR_HANDLE history = depth; D3D12_GPU_DESCRIPTOR_HANDLE historyTransmission = depth;
    if (pipeline == temporalPipeline_.Get()) {
        history = volumeSrv_; historyTransmission = transmittanceSrv_;
        if (hasHistory_) {
            history = SrvManager::GetInstance()->GetGPUDescriptorHandle(historySrvIndices_[historyIndex_]);
            historyTransmission = SrvManager::GetInstance()->GetGPUDescriptorHandle(historyTransmittanceSrvIndices_[historyIndex_]);
        }
    }
    cmd->SetGraphicsRootDescriptorTable(6, history); cmd->SetGraphicsRootDescriptorTable(7, historyTransmission);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
}
bool VolumetricLightRenderer::Generate(D3D12_GPU_DESCRIPTOR_HANDLE depthHandle, bool depthReady) {
    generated_ = false; hasUsedHistory_ = false;
    raymarchTimer_.ResetSample(); temporalTimer_.ResetSample(); compositeTimer_.ResetSample();
    bool valid = frameValid_;
    frameValid_ = false; // Consume the frame; a missing update never reuses old shadows.
    if (!ready_ || !valid || !depthReady || depthHandle.ptr == 0 || !parameters_.enabled ||
        (!localFog_.isEnabled && (parameters_.fogDensity <= 0.0f ||
            (frameConstants_.lightColorAndIntensity.w <= 0.0f && frameConstants_.spotCountAndBias.x == 0.0f)))) { ResetHistory(); return false; }
    if (shadowSrv_.ptr == 0) { shadowSrv_ = depthHandle; }
    uint64_t lightingHash = 14695981039346656037ull;
    const Vector4 kValues[] = {frameConstants_.lightDirectionAndDensity, frameConstants_.lightColorAndIntensity,
        frameConstants_.settings, frameConstants_.fogColorAndEnabled, frameConstants_.heightAndVolumeCount, frameConstants_.spotCountAndBias,
        {localFog_.noiseScale, localFog_.noiseStrength, parameters_.scatteringAlbedo, parameters_.maxDistance}};
    lightingHash = HashVolumeBytes(lightingHash, kValues, sizeof(kValues));
    lightingHash = HashVolumeBytes(lightingHash, &frameConstants_.lightViewProjection, sizeof(Matrix4x4));
    for (const auto& volume : fogVolumes_) {
        lightingHash = HashVolumeBytes(lightingHash, &volume.center, sizeof(Vector3));
        lightingHash = HashVolumeBytes(lightingHash, &volume.halfExtents, sizeof(Vector3));
        const Vector4 kVolume = {volume.radius, volume.density, volume.edgeSoftness, static_cast<float>(volume.shape)};
        lightingHash = HashVolumeBytes(lightingHash, &kVolume, sizeof(kVolume));
        lightingHash = HashVolumeBytes(lightingHash, &volume.isEnabled, sizeof(bool));
    }
    for (uint32_t lightIndex = 0; lightIndex < static_cast<uint32_t>(frameConstants_.spotCountAndBias.x); ++lightIndex) {
        auto light = frameConstants_.spotLights[lightIndex];
        light.positionAndDistance.x += frameConstants_.cameraAndDistance.x;
        light.positionAndDistance.y += frameConstants_.cameraAndDistance.y;
        light.positionAndDistance.z += frameConstants_.cameraAndDistance.z;
        lightingHash = HashVolumeBytes(lightingHash, &light, sizeof(light));
    }
    const auto& cameraWorld = frameCamera_->GetWorldMatrix();
    Vector3 cameraPosition = {cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2]};
    Vector3 cameraDelta = cameraPosition - previousCameraPosition_;
    Vector3 cameraForward = {cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]};
    float forwardDot = cameraForward.x * previousCameraForward_.x + cameraForward.y * previousCameraForward_.y + cameraForward.z * previousCameraForward_.z;
    float forwardLengthSquared = cameraForward.x * cameraForward.x + cameraForward.y * cameraForward.y + cameraForward.z * cameraForward.z;
    float previousForwardLengthSquared = previousCameraForward_.x * previousCameraForward_.x + previousCameraForward_.y * previousCameraForward_.y + previousCameraForward_.z * previousCameraForward_.z;
    if (hasHistory_ && forwardDot < 0.98f * std::sqrt(forwardLengthSquared * previousForwardLengthSquared)) { ResetHistory(); }
    if (previousCamera_ != frameCamera_ || previousSceneRevision_ != frameSceneRevision_
        || previousCameraHistoryId_ != frameCamera_->GetMotionHistoryId() || previousLightingHash_ != lightingHash
        || cameraDelta.x * cameraDelta.x + cameraDelta.y * cameraDelta.y + cameraDelta.z * cameraDelta.z > 4) { ResetHistory(); }
    frameConstants_.temporalControls = {0, parameters_.historyWeight, -1, parameters_.scatteringAlbedo};
    frameConstants_.previousRelativeViewProjection = previousRelativeViewProjection_;
    frameConstants_.previousCameraDelta = {cameraDelta.x, cameraDelta.y, cameraDelta.z, 0};
    if (parameters_.shouldUseTemporalHistory) {
        CreateHistoryResources(); frameConstants_.temporalControls.z = static_cast<float>(frameIndex_ & 63u);
        if (hasHistory_) { frameConstants_.temporalControls.x = 1; hasUsedHistory_ = true; }
    } else { ResetHistory(); }
    *constantsData_ = frameConstants_;
    depthSrv_ = depthHandle;
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto* cmd = dxCommon_->GetCommandList();
    D3D12_VIEWPORT viewport { 0, 0, float(SceneRenderResolution::GetWidth() / 2), float(SceneRenderResolution::GetHeight() / 2), 0, 1 };
    D3D12_RECT scissor { 0, 0, SceneRenderResolution::GetWidth() / 2, SceneRenderResolution::GetHeight() / 2 };
    cmd->RSSetViewports(1, &viewport); cmd->RSSetScissorRects(1, &scissor);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    cmd->OMSetRenderTargets(2, &rtv, TRUE, nullptr);
    raymarchTimer_.Begin();
    Draw(raymarchPipeline_.Get(), depthHandle, depthHandle, shadowSrv_);
    raymarchTimer_.End();
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (parameters_.shouldUseTemporalHistory) { RenderTemporal(); }
    Matrix4x4 relativeWorld = cameraWorld; relativeWorld.m[3][0] = 0; relativeWorld.m[3][1] = 0; relativeWorld.m[3][2] = 0;
    previousRelativeViewProjection_ = MatrixMath::Multiply(MatrixMath::Inverse(relativeWorld), frameCamera_->GetProjectionMatrix());
    previousCameraForward_ = cameraForward; previousCameraPosition_ = cameraPosition; previousCamera_ = frameCamera_; previousSceneRevision_ = frameSceneRevision_;
    previousCameraHistoryId_ = frameCamera_->GetMotionHistoryId(); previousLightingHash_ = lightingHash;
    ++frameIndex_; generated_ = true;
    return true;
}
void VolumetricLightRenderer::Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle) {
    if (!generated_ || sceneColorHandle.ptr == 0) { return; }
    D3D12_VIEWPORT viewport { 0, 0, float(SceneRenderResolution::GetWidth()), float(SceneRenderResolution::GetHeight()), 0, 1 };
    D3D12_RECT scissor { 0, 0, SceneRenderResolution::GetWidth(), SceneRenderResolution::GetHeight() };
    auto* cmd = dxCommon_->GetCommandList();
    cmd->RSSetViewports(1, &viewport); cmd->RSSetScissorRects(1, &scissor);
    D3D12_GPU_DESCRIPTOR_HANDLE scattering = volumeSrv_;
    if (parameters_.shouldUseTemporalHistory && hasHistory_) { scattering = SrvManager::GetInstance()->GetGPUDescriptorHandle(historySrvIndices_[historyIndex_]); }
    compositeTimer_.Begin(); Draw(compositePipeline_.Get(), sceneColorHandle, depthSrv_, scattering); compositeTimer_.End();
    generated_ = false;
}
void VolumetricLightRenderer::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Begin("Volumetric Light");
    bool enabled = parameters_.enabled;
    if (ImGui::Checkbox("Enabled", &enabled)) { SetEnabled(enabled); }
    Vector3 color = parameters_.lightColor;
    if (ImGui::ColorEdit3("Light Tint", &color.x)) { SetLightColor(color); }
    float intensity = parameters_.lightIntensity;
    if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 2.0f)) { SetLightIntensity(intensity); }
    float density = parameters_.fogDensity;
    if (ImGui::SliderFloat("Density", &density, 0.0f, 0.02f, "%.4f")) { SetFogDensity(density); }
    float distance = parameters_.maxDistance;
    if (ImGui::SliderFloat("Distance", &distance, 1.0f, 1000.0f)) { SetMaxDistance(distance); }
    float anisotropy = parameters_.anisotropy;
    if (ImGui::SliderFloat("Anisotropy", &anisotropy, -0.8f, 0.8f)) { SetAnisotropy(anisotropy); }
    int samples = parameters_.sampleCount;
    if (ImGui::SliderInt("Samples", &samples, 8, 64)) { SetSampleCount(samples); }
    bool shouldUseTemporalHistory = parameters_.shouldUseTemporalHistory;
    if (ImGui::Checkbox("Temporal history", &shouldUseTemporalHistory)) { SetTemporalEnabled(shouldUseTemporalHistory); }
    float weight = parameters_.historyWeight; if (ImGui::SliderFloat("History weight", &weight, 0, 0.95f)) { SetHistoryWeight(weight); }
    float albedo = parameters_.scatteringAlbedo; if (ImGui::SliderFloat("Scattering albedo", &albedo, 0, 1)) { SetScatteringAlbedo(albedo); }
    int quality = static_cast<int>(parameters_.quality);
    if (ImGui::Combo("Quality", &quality, "Low (16)\0Medium (32)\0High (64)\0Custom\0") && quality < 4) { SetQuality(static_cast<VolumetricQuality>(quality)); }
    ImGui::Text("GPU march %.3f / history %.3f / composite %.3f ms", GetRaymarchGpuTimeMs(), GetTemporalGpuTimeMs(), GetCompositeGpuTimeMs());
    ImGui::Text("Image allocation %.2f MiB", static_cast<double>(allocationBytes_) / (1024 * 1024));
    ImGui::Text("Uses scene sun direction and current-frame shadows.");
    ImGui::End();
#endif
}

void VolumetricLightRenderer::ResizeSceneTargets() {
    if (!ready_ || (volumeTexture_->GetDesc().Width == SceneRenderResolution::GetWidth() / 2 && volumeTexture_->GetDesc().Height == SceneRenderResolution::GetHeight() / 2)) { return; }
    ResetHistory(); allocationBytes_ = 0; historyIndex_ = 0;
    for (auto& texture : historyTextures_) { texture.Reset(); }
    for (auto& texture : historyTransmittanceTextures_) { texture.Reset(); }
    textureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    CreateResources();
}
