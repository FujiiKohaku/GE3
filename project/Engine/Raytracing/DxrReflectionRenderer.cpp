#include "Engine/Renderer/SceneRenderResolution.h"
#include "DxrReflectionRenderer.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Light/LightManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
constexpr uint64_t kHitStrideBytes = 96;
void RequireReflection(HRESULT result, const char* message) {
    if (FAILED(result)) { throw std::runtime_error(message); }
}
Microsoft::WRL::ComPtr<ID3D12Resource> CreateReflectionUpload(uint64_t sizeBytes) {
    return DirectXCommon::GetInstance()->CreateBufferResource((sizeBytes + 255) / 256 * 256);
}
void CreateReflectionRoot(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& description,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& root) {
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (FAILED(result) && errors) { throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer())); }
    RequireReflection(result, "Reflection root serialization failed");
    RequireReflection(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "Reflection root creation failed");
}
uint64_t HashLightingBytes(uint64_t hash, const void* value, size_t sizeBytes) {
    const auto* bytes = static_cast<const uint8_t*>(value);
    for (size_t index = 0; index < sizeBytes; ++index) { hash = (hash ^ bytes[index]) * 1099511628211ull; }
    return hash;
}
uint64_t GetReflectionLightingHash(const LightManager& lights) {
    uint64_t hash = 14695981039346656037ull;
    DirectionalLight directional = lights.GetDirectionalLight();
    hash = HashLightingBytes(hash, &directional, sizeof(directional));
    Vector3 ambient = lights.GetAmbientColor(); float intensity = lights.GetAmbientIntensity();
    hash = HashLightingBytes(hash, &ambient, sizeof(ambient)); hash = HashLightingBytes(hash, &intensity, sizeof(intensity));
    const Vector4 kSettings[] = {lights.GetHemisphereSkyColor(), lights.GetHemisphereGroundColor(),
        lights.GetEnvironmentLighting(), lights.GetLightingComponents(), lights.GetAtmosphereSettings()};
    hash = HashLightingBytes(hash, kSettings, sizeof(kSettings));
    for (uint32_t index = 0; index < LightManager::kMaxPointLights; ++index) {
        PointLight point = lights.GetPointLight(index); hash = HashLightingBytes(hash, &point, sizeof(point));
    }
    for (uint32_t index = 0; index < LightManager::kMaxSpotLights; ++index) {
        SpotLight spot = lights.GetSpotLight(index); hash = HashLightingBytes(hash, &spot, sizeof(spot));
    }
    auto cube = Object3dManager::GetInstance()->GetEnvironmentTexture();
    return HashLightingBytes(hash, &cube.ptr, sizeof(cube.ptr));
}
bool IsFiniteReflectionMatrix(const Matrix4x4& matrix) {
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) { if (!std::isfinite(matrix.m[row][column])) { return false; } }
    }
    return true;
}
}
DxrReflectionRenderer::DxrReflectionRenderer(DxrLightingMode mode) : mode_(mode) {
    if (mode_ == DxrLightingMode::LocalShadow) { status_ = "RT local shadows disabled"; }
    if (mode_ == DxrLightingMode::DiffuseIndirect) {
        settings_.sampleCount = 1; settings_.maxDistance = 12; settings_.maxRoughness = 1;
        status_ = "RT indirect light disabled";
    }
}
DxrReflectionRenderer::~DxrReflectionRenderer() {
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
    ResetResources();
}
void DxrReflectionRenderer::Initialize() {
    traceTimer_.Initialize(); filterTimer_.Initialize(); compositeTimer_.Initialize();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (mode_ == DxrLightingMode::LocalShadow) { return; }
    std::string sourceId = "rtReflections"; std::string label = "RT反射";
    if (mode_ == DxrLightingMode::DiffuseIndirect) { sourceId = "rtGlobalIllumination"; label = "RT間接光"; }
    DevelopmentWebPanel::GetInstance().RegisterSource<DxrReflectionRenderer>(this, sourceId, label, false,
        &DxrReflectionRenderer::GetDevelopmentState, &DxrReflectionRenderer::GetDevelopmentControls,
        &DxrReflectionRenderer::SetDevelopmentBool, &DxrReflectionRenderer::SetDevelopmentNumber, nullptr);
#endif
}
bool DxrReflectionRenderer::SetSettings(const DxrReflectionSettings& settings) {
    if (settings.sampleCount < 1 || settings.sampleCount > 16 || settings.spatialPassCount > 3
        || settings.maxReflectionBounces < 1 || settings.maxReflectionBounces > 2
        || !std::isfinite(settings.maxDistance) || settings.maxDistance <= 0 || settings.maxDistance > 100000
        || !std::isfinite(settings.maxRoughness) || settings.maxRoughness < 0 || settings.maxRoughness > 1
        || !std::isfinite(settings.normalBias) || settings.normalBias < 0 || settings.normalBias > 10
        || !std::isfinite(settings.rayBias) || settings.rayBias <= 0 || settings.rayBias >= settings.maxDistance
        || !std::isfinite(settings.strength) || settings.strength < 0 || settings.strength > 1
        || !std::isfinite(settings.historyWeight) || settings.historyWeight < 0 || settings.historyWeight > 0.95f
        || !std::isfinite(settings.indirectDistanceFadeRatio) || settings.indirectDistanceFadeRatio < 0 || settings.indirectDistanceFadeRatio > 1
        || !std::isfinite(settings.maxRadiance) || settings.maxRadiance <= 0 || settings.maxRadiance > 65000) { return false; }
    if (settings.isEnabled != settings_.isEnabled || settings.shouldUseTemporalHistory != settings_.shouldUseTemporalHistory
        || settings.shouldTraceMultipleReflections != settings_.shouldTraceMultipleReflections
        || settings.maxReflectionBounces != settings_.maxReflectionBounces
        || settings.shouldUseLowDiscrepancySampling != settings_.shouldUseLowDiscrepancySampling
        || settings.indirectDistanceFadeRatio != settings_.indirectDistanceFadeRatio
        || settings.shouldUseTextureMipmaps != settings_.shouldUseTextureMipmaps
        || settings.shouldTraceSunShadows != settings_.shouldTraceSunShadows || settings.sampleCount != settings_.sampleCount
        || settings.spatialPassCount != settings_.spatialPassCount || settings.maxDistance != settings_.maxDistance
        || settings.maxRoughness != settings_.maxRoughness || settings.normalBias != settings_.normalBias
        || settings.rayBias != settings_.rayBias || settings.historyWeight != settings_.historyWeight
        || settings.maxRadiance != settings_.maxRadiance) { ResetHistory(); }
    settings_ = settings; ++settingsRevision_; return true;
}
void DxrReflectionRenderer::ResetHistory() { hasHistory_ = false; hasUsedHistory_ = false; }
void DxrReflectionRenderer::ResetResources() {
    for (auto& index : srvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); index = UINT_MAX; } }
    for (auto& index : uavIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); index = UINT_MAX; } }
    for (auto& texture : textures_) { texture.Reset(); }
    for (auto& pipeline : filterPipelines_) { pipeline.Reset(); }
    rtvHeap_.Reset(); parameterBuffer_.Reset(); shaderTable_.Reset();
    rayRoot_.Reset(); materialRoot_.Reset(); filterRoot_.Reset(); rayPipeline_.Reset(); rayProperties_.Reset();
    allocationBytes_ = 0; isReady_ = false; hasValidFrame_ = false; ResetHistory();
}
void DxrReflectionRenderer::CreateResources() {
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate(kTargetCount + 3)) { throw std::runtime_error("Reflection descriptors exhausted"); }
    rtvHeap_ = DirectXCommon::GetInstance()->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kTargetCount, false);
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    uint32_t incrementBytes = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t index = 0; index < kTargetCount; ++index) {
        uint32_t width = SceneRenderResolution::GetWidth() / 2; uint32_t height = SceneRenderResolution::GetHeight() / 2;
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (index < 2 || index == 9) { flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
        if (index == 8) { width = SceneRenderResolution::GetWidth(); height = SceneRenderResolution::GetHeight(); }
        DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (index >= 9) { format = DXGI_FORMAT_R32G32B32A32_FLOAT; }
        auto description = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, flags);
        RequireReflection(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index])), "Reflection target allocation failed");
        allocationBytes_ += device_->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
        srvIndices_[index] = srvManager->Allocate();
        srvManager->CreateSRVforTexture2D(srvIndices_[index], textures_[index].Get(), description.Format, 1);
        auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += index * incrementBytes;
        device_->CreateRenderTargetView(textures_[index].Get(), nullptr, rtv);
        if (index < 2 || index == 9) {
            uint32_t unorderedIndex = index;
            if (index == 9) { unorderedIndex = 2; }
            uavIndices_[unorderedIndex] = srvManager->Allocate();
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {}; uav.Format = description.Format; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device_->CreateUnorderedAccessView(textures_[index].Get(), nullptr, &uav, srvManager->GetCPUDescriptorHandle(uavIndices_[unorderedIndex]));
        }
    }
    parameterBuffer_ = CreateReflectionUpload(sizeof(Parameters));
    shaderTable_ = CreateReflectionUpload(256);
}
void DxrReflectionRenderer::CreatePipelines(const DxrRenderer& scene) {
    D3D12_DESCRIPTOR_RANGE ranges[18] = {};
    D3D12_ROOT_PARAMETER parameters[20] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    for (uint32_t index = 0; index < 7; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[index].BaseShaderRegister = index + 1; ranges[index].NumDescriptors = 1;
        if (index >= 5) { ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[index].BaseShaderRegister = index - 5; }
        parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index + 1].DescriptorTable = {1, &ranges[index]};
    }
    for (uint32_t index = 8; index < 13; ++index) {
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; parameters[index].Descriptor.ShaderRegister = index - 8;
    }
    ranges[7].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[7].BaseShaderRegister = 6; ranges[7].NumDescriptors = 1;
    parameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; parameters[13].DescriptorTable = {1, &ranges[7]};
    ranges[8].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[8].BaseShaderRegister = 2; ranges[8].NumDescriptors = 1;
    parameters[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; parameters[14].DescriptorTable = {1, &ranges[8]};
    D3D12_ROOT_SIGNATURE_DESC root = {15, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    CreateReflectionRoot(device_.Get(), root, rayRoot_);
    materialRoot_ = scene.GetMaterialRootSignature();
    auto library = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/Reflection.LIB.hlsl");
    D3D12_EXPORT_DESC exports[] = {{L"ReflectionRayGeneration", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"ReflectionMiss", nullptr, D3D12_EXPORT_FLAG_NONE}, {L"ReflectionAnyHit", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"ReflectionClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE}};
    D3D12_DXIL_LIBRARY_DESC libraryDescription = {{library->GetBufferPointer(), library->GetBufferSize()}, 4, exports};
    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"ReflectionHitGroup"; hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.ClosestHitShaderImport = L"ReflectionClosestHit"; hitGroup.AnyHitShaderImport = L"ReflectionAnyHit";
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {40, 8}; D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {3};
    ID3D12RootSignature* rayRoot = rayRoot_.Get(); ID3D12RootSignature* materialRoot = materialRoot_.Get();
    D3D12_STATE_SUBOBJECT subobjects[7] = {
        {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libraryDescription}, {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroup},
        {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig}, {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig},
        {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &rayRoot}, {D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &materialRoot}, {}
    };
    const wchar_t* kHitExports[] = {L"ReflectionHitGroup"};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {&subobjects[5], 1, kHitExports};
    subobjects[6] = {D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association};
    D3D12_STATE_OBJECT_DESC stateDescription = {D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE, 7, subobjects};
    RequireReflection(device_->CreateStateObject(&stateDescription, IID_PPV_ARGS(&rayPipeline_)), "Reflection ray pipeline creation failed");
    RequireReflection(rayPipeline_.As(&rayProperties_), "Reflection identifiers unavailable");
    for (uint32_t index = 0; index < 18; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[index].BaseShaderRegister = index; ranges[index].NumDescriptors = 1;
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[index].DescriptorTable = {1, &ranges[index]};
    }
    parameters[18].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; parameters[18].Descriptor = {}; parameters[18].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[19].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[19].Constants = {1, 0, 1}; parameters[19].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler = {}; sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root = {20, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    CreateReflectionRoot(device_.Get(), root, filterRoot_);
    const wchar_t* kPaths[] = {L"resources/Shaders/Raytracing/ReflectionTemporal.PS.hlsl",
        L"resources/Shaders/Raytracing/ReflectionSpatial.PS.hlsl", L"resources/Shaders/Raytracing/ReflectionComposite.PS.hlsl"};
    auto vertex = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    for (uint32_t index = 0; index < 3; ++index) {
        auto pixel = DirectXCommon::GetInstance()->LoadCompiledShader(kPaths[index]);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description = {}; description.pRootSignature = filterRoot_.Get();
        description.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()}; description.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
        description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        description.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        description.NumRenderTargets = 1; description.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (index == 0) {
            description.NumRenderTargets = 4; description.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            description.RTVFormats[2] = description.RTVFormats[3] = DXGI_FORMAT_R32G32B32A32_FLOAT;
            description.BlendState.IndependentBlendEnable = TRUE;
            description.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            description.BlendState.RenderTarget[2].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            description.BlendState.RenderTarget[3].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        description.SampleMask = UINT_MAX; description.SampleDesc.Count = 1;
        description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        RequireReflection(device_->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&filterPipelines_[index])), "Reflection filter pipeline creation failed");
    }
}
void DxrReflectionRenderer::RenderFilter(uint32_t pipelineIndex, uint32_t signalIndex, uint32_t targetIndex,
    const DxrReflectionInputs& inputs, uint32_t filterStep) {
    uint32_t readIndex = 5 - historyWriteIndex_;
    auto* srvManager = SrvManager::GetInstance();
    auto motionSrv = inputs.surfaceSrv; if (inputs.motionVectorSrv.ptr != 0) { motionSrv = inputs.motionVectorSrv; }
    auto localLightSrv = inputs.surfaceSrv; if (inputs.localLightSrv.ptr != 0) { localLightSrv = inputs.localLightSrv; }
    auto indirectSrv = inputs.surfaceSrv; if (inputs.indirectSrv.ptr != 0) { indirectSrv = inputs.indirectSrv; }
    auto reprojectionSrv = inputs.surfaceSrv; auto previousReprojectionSrv = inputs.surfaceSrv;
    if (inputs.reprojectionSrv.ptr != 0 && inputs.previousReprojectionSrv.ptr != 0) { reprojectionSrv = inputs.reprojectionSrv; previousReprojectionSrv = inputs.previousReprojectionSrv; }
    const D3D12_GPU_DESCRIPTOR_HANDLE kInputs[] = {srvManager->GetGPUDescriptorHandle(srvIndices_[signalIndex]),
        srvManager->GetGPUDescriptorHandle(srvIndices_[1]), srvManager->GetGPUDescriptorHandle(srvIndices_[readIndex]),
        srvManager->GetGPUDescriptorHandle(srvIndices_[readIndex + 2]), inputs.depthSrv, inputs.surfaceSrv,
        inputs.environmentSrv, inputs.colorSrv, motionSrv, inputs.materialSrv, localLightSrv,
        srvManager->GetGPUDescriptorHandle(srvIndices_[9]), srvManager->GetGPUDescriptorHandle(srvIndices_[readIndex + 8]),
        srvManager->GetGPUDescriptorHandle(srvIndices_[readIndex + 10]), srvManager->GetGPUDescriptorHandle(srvIndices_[historyWriteIndex_ + 10]), indirectSrv, reprojectionSrv, previousReprojectionSrv};
    uint32_t targetCount = 1; if (pipelineIndex == 0) { targetCount = 4; }
    uint32_t indices[] = {targetIndex, targetIndex + 2, targetIndex + 8, targetIndex + 10};
    D3D12_CPU_DESCRIPTOR_HANDLE targets[4] = {};
    uint32_t incrementBytes = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t index = 0; index < targetCount; ++index) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[indices[index]].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->ResourceBarrier(1, &barrier);
        targets[index] = rtvHeap_->GetCPUDescriptorHandleForHeapStart(); targets[index].ptr += indices[index] * incrementBytes;
    }
    auto description = textures_[targetIndex]->GetDesc();
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(description.Width), static_cast<float>(description.Height), 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(description.Width), static_cast<LONG>(description.Height)};
    commandList_->RSSetViewports(1, &viewport); commandList_->RSSetScissorRects(1, &scissor);
    commandList_->OMSetRenderTargets(targetCount, targets, FALSE, nullptr);
    commandList_->SetGraphicsRootSignature(filterRoot_.Get()); commandList_->SetPipelineState(filterPipelines_[pipelineIndex].Get());
    for (uint32_t index = 0; index < 18; ++index) { commandList_->SetGraphicsRootDescriptorTable(index, kInputs[index]); }
    commandList_->SetGraphicsRootConstantBufferView(18, parameterBuffer_->GetGPUVirtualAddress());
    commandList_->SetGraphicsRoot32BitConstant(19, filterStep, 0);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); commandList_->DrawInstanced(3, 1, 0, 0);
    for (uint32_t index = 0; index < targetCount; ++index) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[indices[index]].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
}
D3D12_GPU_DESCRIPTOR_HANDLE DxrReflectionRenderer::Draw(const DxrReflectionInputs& inputs) {
    if (textures_[8] && (textures_[8]->GetDesc().Width != SceneRenderResolution::GetWidth() || textures_[8]->GetDesc().Height != SceneRenderResolution::GetHeight())) { ResetResources(); }

    hasValidFrame_ = false; hasUsedHistory_ = false;
    traceTimer_.ResetSample(); filterTimer_.ResetSample(); compositeTimer_.ResetSample();
    auto* lights = LightManager::GetInstance();
    if (!settings_.isEnabled || !inputs.scene || !inputs.scene->HasValidScene() || !inputs.camera || !lights->IsInitialized()
        || inputs.colorSrv.ptr == 0 || inputs.depthSrv.ptr == 0 || inputs.surfaceSrv.ptr == 0 || inputs.environmentSrv.ptr == 0
        || inputs.materialSrv.ptr == 0 || !inputs.depthTexture || !inputs.surfaceTexture || !inputs.environmentTexture || !inputs.materialTexture) {
        ResetHistory(); return inputs.colorSrv;
    }
    for (auto* texture : {inputs.depthTexture, inputs.surfaceTexture, inputs.environmentTexture, inputs.materialTexture}) {
        auto description = texture->GetDesc();
        if (description.Width != SceneRenderResolution::GetWidth() || description.Height != SceneRenderResolution::GetHeight()) { ResetHistory(); return inputs.colorSrv; }
    }
    if (mode_ == DxrLightingMode::LocalShadow && (!inputs.localLightTexture || inputs.localLightSrv.ptr == 0
        || (inputs.scene->GetLocalShadowParameters().pointMask | inputs.scene->GetLocalShadowParameters().spotMask) == 0)) {
        ResetHistory(); return inputs.colorSrv;
    }
    if (mode_ == DxrLightingMode::LocalShadow) {
        auto description = inputs.localLightTexture->GetDesc();
        if (description.Width != SceneRenderResolution::GetWidth() || description.Height != SceneRenderResolution::GetHeight()) { ResetHistory(); return inputs.colorSrv; }
    }
    if (mode_ == DxrLightingMode::DiffuseIndirect) {
        if (!inputs.indirectTexture || inputs.indirectSrv.ptr == 0) { ResetHistory(); return inputs.colorSrv; }
        auto description = inputs.indirectTexture->GetDesc();
        if (description.Width != SceneRenderResolution::GetWidth() || description.Height != SceneRenderResolution::GetHeight()) { ResetHistory(); return inputs.colorSrv; }
    }
    Parameters parameters = {}; parameters.indirectSampling.y = settings_.indirectDistanceFadeRatio;
    if (settings_.shouldUseLowDiscrepancySampling) { parameters.indirectSampling.x = 1; }
    if (mode_ == DxrLightingMode::Reflection && settings_.shouldTraceMultipleReflections) {
        parameters.indirectSampling.z = static_cast<float>(settings_.maxReflectionBounces);
    }
    parameters.localShadows = inputs.scene->GetLocalShadowParameters();
    parameters.inverseViewProjection = MatrixMath::Inverse(inputs.camera->GetViewProjectionMatrix());
    parameters.view = inputs.camera->GetViewMatrix();
    if (!IsFiniteReflectionMatrix(parameters.inverseViewProjection) || !IsFiniteReflectionMatrix(parameters.view)) { ResetHistory(); return inputs.colorSrv; }
    uint64_t lightingHash = GetReflectionLightingHash(*lights);
    lightingHash = HashLightingBytes(lightingHash, &parameters.localShadows, sizeof(parameters.localShadows));
    uint64_t skyLightingHash = 0;
    if (lights->IsSkyLightingEnabled()) { skyLightingHash = lightingHash; }
    if (previousSkyLightingHash_ != skyLightingHash) { ResetHistory(); }
    if (historyCamera_ != inputs.camera || historyScene_ != inputs.scene || cameraHistoryId_ != inputs.camera->GetMotionHistoryId()
        || previousSceneRevision_ != inputs.sceneRevision) { ResetHistory(); }
    if (previousGeometryRevision_ != inputs.scene->GetReflectionSceneRevision() || previousLightingHash_ != lightingHash) {
        parameters.historyValidation.x = 1;
    }
    if (settings_.shouldUseTextureMipmaps) { parameters.historyValidation.w = 1; }
    if (settings_.shouldUseTemporalHistory) { parameters.historyValidation.y = 1; }
    if (inputs.motionVectorSrv.ptr != 0 && inputs.reprojectionSrv.ptr != 0 && inputs.previousReprojectionSrv.ptr != 0) { parameters.historyValidation.z = 1; }
    parameters.previousViewProjection = previousViewProjection_; parameters.previousView = previousView_;
    auto world = inputs.camera->GetWorldMatrix(); parameters.cameraPosition = {world.m[3][0], world.m[3][1], world.m[3][2], 1};
    parameters.controls = {settings_.maxDistance, settings_.normalBias, settings_.rayBias, settings_.maxRoughness};
    parameters.temporal = {0, settings_.historyWeight, 0, 0};
    if (hasHistory_ && settings_.shouldUseTemporalHistory) {
        hasUsedHistory_ = true; parameters.temporal.x = 1;
        auto jitter = inputs.camera->GetProjectionJitter();
        parameters.temporal.z = (jitter.x - previousJitter_.x) * 0.5f; parameters.temporal.w = (jitter.y - previousJitter_.y) * -0.5f;
    }
    parameters.options = {static_cast<float>(settings_.sampleCount), 0, 0, 0};
    frameIndex_ = (frameIndex_ + 1) & 0x00ffffffu;
    if (settings_.shouldUseTemporalHistory) { parameters.options.y = static_cast<float>(frameIndex_); }
    if (settings_.shouldTraceSunShadows) { parameters.options.z = 1; }
    if (inputs.motionVectorSrv.ptr != 0) { parameters.options.w = 1; }
    parameters.composition = {settings_.strength, 0, 0, 0}; if (settings_.isDebugVisible) { parameters.composition.y = 1; }
    if (mode_ == DxrLightingMode::DiffuseIndirect) { parameters.composition.z = 1; parameters.composition.w = settings_.maxRadiance; }
    if (mode_ == DxrLightingMode::LocalShadow) { parameters.composition.z = 2; }
    try {
        if (!isReady_) {
            auto* dx = DirectXCommon::GetInstance();
            RequireReflection(dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&device_)), "Reflection device unavailable");
            RequireReflection(dx->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList_)), "Reflection command list unavailable");
            CreateResources(); CreatePipelines(*inputs.scene); isReady_ = true;
        }
        uint64_t tableSizeBytes = 128 + inputs.scene->GetStatistics().hitRecordCount * kHitStrideBytes;
        if (shaderTable_->GetDesc().Width < tableSizeBytes) { shaderTable_ = CreateReflectionUpload(tableSizeBytes); }
        void* tableData = nullptr;
        RequireReflection(shaderTable_->Map(0, nullptr, &tableData), "Reflection hit table mapping failed");
        const void* rayId = rayProperties_->GetShaderIdentifier(L"ReflectionRayGeneration");
        const void* missId = rayProperties_->GetShaderIdentifier(L"ReflectionMiss");
        const void* hitId = rayProperties_->GetShaderIdentifier(L"ReflectionHitGroup");
        if (!rayId || !missId || !hitId) { shaderTable_->Unmap(0, nullptr); throw std::runtime_error("Reflection shader identifier missing"); }
        std::memset(tableData, 0, static_cast<size_t>(shaderTable_->GetDesc().Width));
        std::memcpy(tableData, rayId, 32); std::memcpy(static_cast<uint8_t*>(tableData) + 64, missId, 32);
        inputs.scene->WriteHitRecords(static_cast<uint8_t*>(tableData) + 128, kHitStrideBytes, hitId); shaderTable_->Unmap(0, nullptr);
        void* parameterData = nullptr; RequireReflection(parameterBuffer_->Map(0, nullptr, &parameterData), "Reflection parameters mapping failed");
        std::memcpy(parameterData, &parameters, sizeof(parameters)); parameterBuffer_->Unmap(0, nullptr);
    } catch (const std::exception& error) {
        ResetResources(); settings_.isEnabled = false; status_ = error.what(); Logger::Error(status_); return inputs.colorSrv;
    }
    ID3D12Resource* resources[] = {inputs.depthTexture, inputs.surfaceTexture, inputs.environmentTexture, inputs.materialTexture};
    if (inputs.localLightTexture && inputs.localLightSrv.ptr != 0) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inputs.localLightTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    for (auto* resource : resources) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    const uint32_t kRayTargets[] = {0, 1, 9};
    for (uint32_t index : kRayTargets) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[index].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList_->ResourceBarrier(1, &barrier);
    }
    inputs.scene->BeginMaterialRead(true); auto* srvManager = SrvManager::GetInstance(); srvManager->PreDraw();
    commandList_->SetComputeRootSignature(rayRoot_.Get()); commandList_->SetPipelineState1(rayPipeline_.Get());
    commandList_->SetComputeRootShaderResourceView(0, inputs.scene->GetSceneGpuAddress());
    const D3D12_GPU_DESCRIPTOR_HANDLE kRayInputs[] = {inputs.depthSrv, inputs.surfaceSrv, inputs.environmentSrv, inputs.materialSrv,
        Object3dManager::GetInstance()->GetEnvironmentTexture(), srvManager->GetGPUDescriptorHandle(uavIndices_[0]), srvManager->GetGPUDescriptorHandle(uavIndices_[1])};
    for (uint32_t index = 0; index < 7; ++index) { commandList_->SetComputeRootDescriptorTable(index + 1, kRayInputs[index]); }
    auto localLightSrv = inputs.surfaceSrv; if (inputs.localLightSrv.ptr != 0) { localLightSrv = inputs.localLightSrv; }
    commandList_->SetComputeRootDescriptorTable(13, localLightSrv);
    commandList_->SetComputeRootDescriptorTable(14, srvManager->GetGPUDescriptorHandle(uavIndices_[2]));
    commandList_->SetComputeRootConstantBufferView(8, parameterBuffer_->GetGPUVirtualAddress());
    commandList_->SetComputeRootConstantBufferView(9, lights->GetDirectionalGpuAddress()); commandList_->SetComputeRootConstantBufferView(10, lights->GetAmbientGpuAddress());
    commandList_->SetComputeRootConstantBufferView(11, lights->GetPointLightsGpuAddress()); commandList_->SetComputeRootConstantBufferView(12, lights->GetSpotLightsGpuAddress());
    uint64_t address = shaderTable_->GetGPUVirtualAddress(); D3D12_DISPATCH_RAYS_DESC dispatch = {};
    dispatch.RayGenerationShaderRecord = {address, 32}; dispatch.MissShaderTable = {address + 64, 64, 64};
    dispatch.HitGroupTable = {address + 128, inputs.scene->GetStatistics().hitRecordCount * kHitStrideBytes, kHitStrideBytes};
    dispatch.Width = SceneRenderResolution::GetWidth() / 2; dispatch.Height = SceneRenderResolution::GetHeight() / 2; dispatch.Depth = 1;
    traceTimer_.Begin(); commandList_->DispatchRays(&dispatch); traceTimer_.End(); inputs.scene->EndMaterialRead(true);
    if (inputs.localLightTexture && inputs.localLightSrv.ptr != 0) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inputs.localLightTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    for (auto* resource : resources) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    for (uint32_t index : kRayTargets) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[index].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    filteredIndex_ = 0; filterTimer_.Begin();
    if (settings_.shouldUseTemporalHistory) { RenderFilter(0, 0, historyWriteIndex_, inputs, 1); filteredIndex_ = historyWriteIndex_; }
    for (uint32_t index = 0; index < settings_.spatialPassCount; ++index) {
        uint32_t targetIndex = 6 + index % 2; RenderFilter(1, filteredIndex_, targetIndex, inputs, 1u << index); filteredIndex_ = targetIndex;
    }
    filterTimer_.End(); compositeTimer_.Begin(); RenderFilter(2, filteredIndex_, 8, inputs, 1); compositeTimer_.End();
    hasHistory_ = settings_.shouldUseTemporalHistory; if (hasHistory_) { historyWriteIndex_ = 5 - historyWriteIndex_; }
    previousViewProjection_ = inputs.camera->GetViewProjectionMatrix(); previousView_ = parameters.view;
    previousJitter_ = inputs.camera->GetProjectionJitter(); historyCamera_ = inputs.camera; historyScene_ = inputs.scene;
    cameraHistoryId_ = inputs.camera->GetMotionHistoryId(); previousGeometryRevision_ = inputs.scene->GetReflectionSceneRevision();
    previousSceneRevision_ = inputs.sceneRevision; previousLightingHash_ = lightingHash;
    previousSkyLightingHash_ = skyLightingHash;
    hasValidFrame_ = true; status_ = "RT reflections ready (half resolution)";
    if (mode_ == DxrLightingMode::DiffuseIndirect) { status_ = "RT indirect light ready (half resolution)"; }
    if (mode_ == DxrLightingMode::LocalShadow) { status_ = "RT local shadows ready (half resolution)"; }
    return srvManager->GetGPUDescriptorHandle(srvIndices_[8]);
}
ID3D12Resource* DxrReflectionRenderer::GetFilteredTexture() const {
    if (!hasValidFrame_) { return nullptr; }
    return textures_[filteredIndex_].Get();
}
ID3D12Resource* DxrReflectionRenderer::GetHistoryStatisticsTexture() const {
    if (!hasValidFrame_ || !hasHistory_) { return nullptr; }
    return textures_[15 - historyWriteIndex_].Get();
}
void DxrReflectionRenderer::ReadCompleted() { traceTimer_.ReadCompleted(); filterTimer_.ReadCompleted(); compositeTimer_.ReadCompleted(); }
void DxrReflectionRenderer::DrawImGui() {
#ifdef USE_IMGUI
    const char* windowTitle = "RT Reflections";
    if (mode_ == DxrLightingMode::DiffuseIndirect) { windowTitle = "RT Global Illumination"; }
    if (ImGui::Begin(windowTitle)) {
        ImGui::TextUnformatted(status_.c_str()); auto settings = settings_;
        bool hasChanged = ImGui::Checkbox("Enabled", &settings.isEnabled);
        hasChanged |= ImGui::Checkbox("Lighting contribution only", &settings.isDebugVisible);
        hasChanged |= ImGui::Checkbox("Temporal history", &settings.shouldUseTemporalHistory);
        hasChanged |= ImGui::Checkbox("Sun shadows at ray hits", &settings.shouldTraceSunShadows);
        hasChanged |= ImGui::Checkbox("Ray texture mipmaps", &settings.shouldUseTextureMipmaps);
        if (mode_ == DxrLightingMode::DiffuseIndirect) {
            hasChanged |= ImGui::Checkbox("Low discrepancy sampling", &settings.shouldUseLowDiscrepancySampling);
            hasChanged |= ImGui::SliderFloat("Distance fade ratio", &settings.indirectDistanceFadeRatio, 0, 1);
        }
        if (mode_ == DxrLightingMode::Reflection) {
            hasChanged |= ImGui::Checkbox("Multiple reflections", &settings.shouldTraceMultipleReflections);
            int bounces = static_cast<int>(settings.maxReflectionBounces);
            hasChanged |= ImGui::SliderInt("Maximum reflection bounces", &bounces, 1, 2);
            settings.maxReflectionBounces = static_cast<uint32_t>(bounces);
        }
        int samples = static_cast<int>(settings.sampleCount); int spatialPasses = static_cast<int>(settings.spatialPassCount);
        hasChanged |= ImGui::SliderInt("Samples", &samples, 1, 16); settings.sampleCount = static_cast<uint32_t>(samples);
        hasChanged |= ImGui::SliderInt("Spatial passes", &spatialPasses, 0, 3); settings.spatialPassCount = static_cast<uint32_t>(spatialPasses);
        hasChanged |= ImGui::SliderFloat("History weight", &settings.historyWeight, 0, 0.95f);
        hasChanged |= ImGui::SliderFloat("Strength", &settings.strength, 0, 1);
        if (mode_ == DxrLightingMode::Reflection) { hasChanged |= ImGui::SliderFloat("Maximum roughness", &settings.maxRoughness, 0, 1); }
        else { hasChanged |= ImGui::SliderFloat("Maximum radiance", &settings.maxRadiance, 0.1f, 100); }
        hasChanged |= ImGui::SliderFloat("Distance", &settings.maxDistance, 1, 10000);
        if (hasChanged) { SetSettings(settings); }
        if (ImGui::Button("Reset lighting history")) { ResetHistory(); }
        ImGui::Text("Trace %.3f / filter %.3f / composite %.3f ms", GetTraceGpuTimeMs(), GetFilterGpuTimeMs(), GetCompositeGpuTimeMs());
        ImGui::Text("Lighting targets %.2f MiB", allocationBytes_ / 1048576.0);
    }
    ImGui::End();
#endif
}
#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json DxrReflectionRenderer::GetDevelopmentState() const {
    return {{"status", status_}, {"isEnabled", settings_.isEnabled}, {"isDebugVisible", settings_.isDebugVisible},
        {"shouldUseTemporalHistory", settings_.shouldUseTemporalHistory}, {"shouldTraceSunShadows", settings_.shouldTraceSunShadows},
        {"shouldUseTextureMipmaps", settings_.shouldUseTextureMipmaps},
        {"shouldTraceMultipleReflections", settings_.shouldTraceMultipleReflections}, {"maxReflectionBounces", settings_.maxReflectionBounces},
        {"shouldUseLowDiscrepancySampling", settings_.shouldUseLowDiscrepancySampling}, {"indirectDistanceFadeRatio", settings_.indirectDistanceFadeRatio},
        {"sampleCount", settings_.sampleCount}, {"spatialPassCount", settings_.spatialPassCount}, {"strength", settings_.strength},
        {"maxDistance", settings_.maxDistance}, {"maxRoughness", settings_.maxRoughness}, {"historyWeight", settings_.historyWeight}, {"maxRadiance", settings_.maxRadiance},
        {"traceGpuMs", GetTraceGpuTimeMs()}, {"filterGpuMs", GetFilterGpuTimeMs()}, {"compositeGpuMs", GetCompositeGpuTimeMs()},
        {"allocationBytes", allocationBytes_}, {"hasValidFrame", hasValidFrame_}, {"hasUsedHistory", hasUsedHistory_}};
}
nlohmann::json DxrReflectionRenderer::GetDevelopmentControls() const {
    auto controls = nlohmann::json::array({
        {{"key", "status"}, {"label", "状態"}, {"type", "metric"}},
        {{"key", "isEnabled"}, {"label", "RT反射"}, {"type", "bool"}},
        {{"key", "isDebugVisible"}, {"label", "反射だけ表示"}, {"type", "bool"}},
        {{"key", "shouldUseTemporalHistory"}, {"label", "時間蓄積"}, {"type", "bool"}},
        {{"key", "shouldTraceSunShadows"}, {"label", "反射先の太陽光遮蔽"}, {"type", "bool"}},
        {{"key", "sampleCount"}, {"label", "サンプル数"}, {"type", "number"}, {"min", 1}, {"max", 16}, {"step", 1}},
        {{"key", "spatialPassCount"}, {"label", "空間フィルター回数"}, {"type", "number"}, {"min", 0}, {"max", 3}, {"step", 1}},
        {{"key", "strength"}, {"label", "強度"}, {"type", "number"}, {"min", 0}, {"max", 1}, {"step", 0.05}},
        {{"key", "maxDistance"}, {"label", "距離"}, {"type", "number"}, {"min", 1}, {"max", 10000}, {"step", 10}},
        {{"key", "maxRoughness"}, {"label", "最大粗さ"}, {"type", "number"}, {"min", 0}, {"max", 1}, {"step", 0.05}},
        {{"key", "historyWeight"}, {"label", "履歴の重み"}, {"type", "number"}, {"min", 0}, {"max", 0.95}, {"step", 0.05}},
        {{"key", "traceGpuMs"}, {"label", "光線探索 (ms)"}, {"type", "metric"}},
        {{"key", "filterGpuMs"}, {"label", "ノイズ除去 (ms)"}, {"type", "metric"}},
        {{"key", "compositeGpuMs"}, {"label", "合成 (ms)"}, {"type", "metric"}},
        {{"key", "allocationBytes"}, {"label", "反射画像実割当 (bytes)"}, {"type", "metric"}}
    });
    if (mode_ == DxrLightingMode::DiffuseIndirect) {
        controls[1]["label"] = "RT間接光"; controls[2]["label"] = "間接光だけ表示";
        controls[4]["label"] = "命中先の太陽光遮蔽";
        controls[9] = {{"key", "maxRadiance"}, {"label", "最大放射輝度"}, {"type", "number"}, {"min", 0.1}, {"max", 100}, {"step", 0.1}};
        controls[14]["label"] = "間接光画像実割当 (bytes)";
    }
    controls.push_back({{"key", "shouldUseTextureMipmaps"}, {"label", "命中先のMipフィルター"}, {"type", "bool"}});
    if (mode_ == DxrLightingMode::DiffuseIndirect) {
        controls.push_back({{"key", "shouldUseLowDiscrepancySampling"}, {"label", "均等なサンプル配分"}, {"type", "bool"}});
        controls.push_back({{"key", "indirectDistanceFadeRatio"}, {"label", "探索距離の環境光への移行割合"}, {"type", "number"}, {"min", 0}, {"max", 1}, {"step", 0.05}});
    }
    if (mode_ == DxrLightingMode::Reflection) {
        controls.push_back({{"key", "shouldTraceMultipleReflections"}, {"label", "多重反射"}, {"type", "bool"}});
        controls.push_back({{"key", "maxReflectionBounces"}, {"label", "最大反射回数"}, {"type", "number"}, {"min", 1}, {"max", 2}, {"step", 1}});
    }
    return controls;
}
bool DxrReflectionRenderer::SetDevelopmentBool(const std::string& key, bool isEnabled) {
    auto settings = settings_;
    if (key == "isEnabled") { settings.isEnabled = isEnabled; }
    else if (key == "isDebugVisible") { settings.isDebugVisible = isEnabled; }
    else if (key == "shouldUseTemporalHistory") { settings.shouldUseTemporalHistory = isEnabled; }
    else if (key == "shouldTraceMultipleReflections") { settings.shouldTraceMultipleReflections = isEnabled; }
    else if (key == "shouldUseLowDiscrepancySampling") { settings.shouldUseLowDiscrepancySampling = isEnabled; }
    else if (key == "shouldUseTextureMipmaps") { settings.shouldUseTextureMipmaps = isEnabled; }
    else if (key == "shouldTraceSunShadows") { settings.shouldTraceSunShadows = isEnabled; }
    else { return false; }
    return SetSettings(settings);
}
bool DxrReflectionRenderer::SetDevelopmentNumber(const std::string& key, double value) {
    if (!std::isfinite(value)) { return false; }
    auto settings = settings_;
    if (key == "sampleCount" || key == "spatialPassCount") {
        if (value < 0 || value > 16 || std::floor(value) != value) { return false; }
        if (key == "sampleCount") { settings.sampleCount = static_cast<uint32_t>(value); }
        else { settings.spatialPassCount = static_cast<uint32_t>(value); }
    }
    else if (key == "maxReflectionBounces") {
        if (value < 1 || value > 2 || std::floor(value) != value) { return false; }
        settings.maxReflectionBounces = static_cast<uint32_t>(value);
    }
    else if (key == "strength") { settings.strength = static_cast<float>(value); }
    else if (key == "maxDistance") { settings.maxDistance = static_cast<float>(value); }
    else if (key == "maxRoughness") { settings.maxRoughness = static_cast<float>(value); }
    else if (key == "historyWeight") { settings.historyWeight = static_cast<float>(value); }
    else if (key == "indirectDistanceFadeRatio") { settings.indirectDistanceFadeRatio = static_cast<float>(value); }
    else if (key == "maxRadiance") { settings.maxRadiance = static_cast<float>(value); }
    else { return false; }
    return SetSettings(settings);
}
#endif
