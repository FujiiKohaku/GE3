#include "Engine/Renderer/SceneRenderResolution.h"
#include "DxrShadowRenderer.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/WinApp/WinApp.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
struct ShadowParameters {
    Matrix4x4 inverseViewProjection;
    Vector3 lightDirection;
    uint32_t sampleCount;
    float normalBias;
    float rayBias;
    float sunAngularRadiusRadians;
    float maxRayDistance;
    uint32_t sampleFrameIndex;
};
void RequireShadowResult(HRESULT result, const char* message) {
    if (FAILED(result)) { throw std::runtime_error(message); }
}
Microsoft::WRL::ComPtr<ID3D12Resource> CreateShadowUpload(ID3D12Device* device, uint64_t sizeBytes) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    auto description = CD3DX12_RESOURCE_DESC::Buffer((sizeBytes + 255) / 256 * 256);
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    RequireShadowResult(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)), "RT shadow upload allocation failed");
    return resource;
}
void CreateShadowRoot(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& description,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& root) {
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    RequireShadowResult(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "RT shadow root serialization failed");
    RequireShadowResult(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "RT shadow root creation failed");
}
}
DxrShadowRenderer::~DxrShadowRenderer() {
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
    ResetResources();
}
void DxrShadowRenderer::ResetResources() {
    for (uint32_t index : {maskUavIndex_, maskSrvIndex_, colorSrvIndex_}) {
        if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); }
    }
    maskUavIndex_ = maskSrvIndex_ = colorSrvIndex_ = UINT_MAX;
    mask_.Reset(); color_.Reset(); rtvHeap_.Reset(); parameters_.Reset(); shaderTable_.Reset();
    traceRoot_.Reset(); compositeRoot_.Reset(); tracePipeline_.Reset(); compositePipeline_.Reset();
    materialRoot_.Reset();
    isReady_ = false; textureAllocationBytes_ = 0;
}
void DxrShadowRenderer::Initialize() {
    traceTimer_.Initialize(); compositeTimer_.Initialize();
    denoiser_.Initialize();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().RegisterSource<DxrShadowRenderer>(this, "rtShadows", "RTシャドウ", false,
        &DxrShadowRenderer::GetDevelopmentState, &DxrShadowRenderer::GetDevelopmentControls,
        &DxrShadowRenderer::SetDevelopmentBool, &DxrShadowRenderer::SetDevelopmentNumber, nullptr);
#endif
}
bool DxrShadowRenderer::SetSettings(const DxrShadowSettings& settings) {
    if (settings.maxHistoryFrames < 1 || settings.maxHistoryFrames > 64 || settings.spatialPassCount > 3
        || settings.sampleCount < 1 || settings.sampleCount > 64 || !std::isfinite(settings.sunAngularRadiusRadians)
        || settings.sunAngularRadiusRadians < 0 || settings.sunAngularRadiusRadians > 0.2f
        || !std::isfinite(settings.normalBias) || settings.normalBias < 0 || settings.normalBias > 10
        || !std::isfinite(settings.rayBias) || settings.rayBias <= 0 || settings.rayBias > 10
        || !std::isfinite(settings.maxRayDistance) || settings.maxRayDistance <= settings.rayBias || settings.maxRayDistance > 1000000) { return false; }
    if (settings.isEnabled != settings_.isEnabled || settings.isDenoisingEnabled != settings_.isDenoisingEnabled
        || settings.shouldUseTemporalHistory != settings_.shouldUseTemporalHistory || settings.maxHistoryFrames != settings_.maxHistoryFrames
        || settings.spatialPassCount != settings_.spatialPassCount || settings.sampleCount != settings_.sampleCount
        || settings.sunAngularRadiusRadians != settings_.sunAngularRadiusRadians || settings.normalBias != settings_.normalBias
        || settings.rayBias != settings_.rayBias || settings.maxRayDistance != settings_.maxRayDistance) { denoiser_.ResetHistory(); }
    settings_ = settings;
    return true;
}
void DxrShadowRenderer::CreateResources() {
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate(3)) { throw std::runtime_error("RT shadow descriptors exhausted"); }
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto maskDescription = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, SceneRenderResolution::GetWidth(),
        SceneRenderResolution::GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    RequireShadowResult(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &maskDescription,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&mask_)), "RT shadow mask creation failed");
    auto colorDescription = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, SceneRenderResolution::GetWidth(),
        SceneRenderResolution::GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    RequireShadowResult(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &colorDescription,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&color_)), "RT shadow color creation failed");
    textureAllocationBytes_ = device_->GetResourceAllocationInfo(0, 1, &maskDescription).SizeInBytes
        + device_->GetResourceAllocationInfo(0, 1, &colorDescription).SizeInBytes;
    maskUavIndex_ = srvManager->Allocate(); maskSrvIndex_ = srvManager->Allocate(); colorSrvIndex_ = srvManager->Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = DXGI_FORMAT_R32_FLOAT; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(mask_.Get(), nullptr, &uav, srvManager->GetCPUDescriptorHandle(maskUavIndex_));
    srvManager->CreateSRVforTexture2D(maskSrvIndex_, mask_.Get(), maskDescription.Format, 1);
    srvManager->CreateSRVforTexture2D(colorSrvIndex_, color_.Get(), colorDescription.Format, 1);
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescription = {};
    rtvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; rtvDescription.NumDescriptors = 1;
    RequireShadowResult(device_->CreateDescriptorHeap(&rtvDescription, IID_PPV_ARGS(&rtvHeap_)), "RT shadow RTV creation failed");
    device_->CreateRenderTargetView(color_.Get(), nullptr, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
    parameters_ = CreateShadowUpload(device_.Get(), 256);
    shaderTable_ = CreateShadowUpload(device_.Get(), 256);
}
void DxrShadowRenderer::CreatePipelines(const DxrRenderer& scene) {
    D3D12_DESCRIPTOR_RANGE ranges[4] = {};
    D3D12_ROOT_PARAMETER parameters[6] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[0].Descriptor.ShaderRegister = 0;
    for (uint32_t index = 0; index < 4; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].BaseShaderRegister = index + 1;
        ranges[index].NumDescriptors = 1;
        if (index == 3) { ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[index].BaseShaderRegister = 0; }
        parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index + 1].DescriptorTable = {1, &ranges[index]};
    }
    parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    D3D12_ROOT_SIGNATURE_DESC rootDescription = {6, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    CreateShadowRoot(device_.Get(), rootDescription, traceRoot_);
    auto library = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/Shadow.LIB.hlsl");
    D3D12_EXPORT_DESC exports[3] = {{L"ShadowRayGeneration", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"ShadowMiss", nullptr, D3D12_EXPORT_FLAG_NONE}, {L"ShadowAnyHit", nullptr, D3D12_EXPORT_FLAG_NONE}};
    D3D12_DXIL_LIBRARY_DESC libraryDescription = {{library->GetBufferPointer(), library->GetBufferSize()}, 3, exports};
    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"ShadowHitGroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.AnyHitShaderImport = L"ShadowAnyHit";
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {4, 8};
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {1};
    ID3D12RootSignature* traceRoot = traceRoot_.Get();
    materialRoot_ = scene.GetMaterialRootSignature();
    ID3D12RootSignature* materialRoot = materialRoot_.Get();
    D3D12_STATE_SUBOBJECT subobjects[7] = {
        {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libraryDescription},
        {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroup},
        {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig},
        {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig},
        {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &traceRoot},
        {D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &materialRoot},
        {}
    };
    const wchar_t* kMaterialExports[] = {L"ShadowHitGroup"};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {&subobjects[5], 1, kMaterialExports};
    subobjects[6] = {D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association};
    D3D12_STATE_OBJECT_DESC pipelineDescription = {D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE, 7, subobjects};
    RequireShadowResult(device_->CreateStateObject(&pipelineDescription, IID_PPV_ARGS(&tracePipeline_)), "RT shadow pipeline creation failed");
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> properties;
    RequireShadowResult(tracePipeline_.As(&properties), "RT shadow shader identifiers unavailable");
    void* data = nullptr;
    RequireShadowResult(shaderTable_->Map(0, nullptr, &data), "RT shadow shader table mapping failed");
    std::memset(data, 0, 256);
    const wchar_t* kShaderNames[] = {L"ShadowRayGeneration", L"ShadowMiss", L"ShadowHitGroup"};
    for (uint32_t index = 0; index < 3; ++index) {
        const void* identifier = properties->GetShaderIdentifier(kShaderNames[index]);
        if (!identifier) { throw std::runtime_error("RT shadow shader identifier missing"); }
        std::memcpy(static_cast<uint8_t*>(data) + index * 64, identifier, 32);
        if (index == 2) { std::memcpy(hitShaderIdentifier_, identifier, sizeof(hitShaderIdentifier_)); }
    }
    shaderTable_->Unmap(0, nullptr);

    D3D12_ROOT_PARAMETER compositeParameters[4] = {};
    for (uint32_t index = 0; index < 3; ++index) {
        ranges[index].BaseShaderRegister = index;
        compositeParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        compositeParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        compositeParameters[index].DescriptorTable = {1, &ranges[index]};
    }
    compositeParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    compositeParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    compositeParameters[3].Constants.Num32BitValues = 1;
    rootDescription.NumParameters = 4; rootDescription.pParameters = compositeParameters;
    rootDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    CreateShadowRoot(device_.Get(), rootDescription, compositeRoot_);
    auto vertex = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    auto pixel = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/ShadowComposite.PS.hlsl");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC composite = {};
    composite.pRootSignature = compositeRoot_.Get();
    composite.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    composite.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    composite.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    composite.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    composite.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    composite.SampleMask = UINT_MAX; composite.SampleDesc.Count = 1;
    composite.NumRenderTargets = 1; composite.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    composite.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    RequireShadowResult(device_->CreateGraphicsPipelineState(&composite, IID_PPV_ARGS(&compositePipeline_)), "RT shadow composition pipeline creation failed");
}
D3D12_GPU_DESCRIPTOR_HANDLE DxrShadowRenderer::Draw(const DxrShadowInputs& inputs) {
    if (color_ && (color_->GetDesc().Width != SceneRenderResolution::GetWidth() || color_->GetDesc().Height != SceneRenderResolution::GetHeight())) { ResetResources(); }

    hasValidFrame_ = false;
    traceTimer_.ResetSample(); compositeTimer_.ResetSample();
    if (!settings_.isEnabled || !inputs.scene || !inputs.scene->HasValidScene() || !inputs.camera
        || !inputs.depthTexture || !inputs.normalTexture || !inputs.directionalLightTexture
        || inputs.colorSrv.ptr == 0 || inputs.depthSrv.ptr == 0 || inputs.normalSrv.ptr == 0 || inputs.directionalLightSrv.ptr == 0) { denoiser_.ResetHistory(); return inputs.colorSrv; }
    float directionLength = Vector3Length(inputs.lightDirection);
    if (!std::isfinite(directionLength) || directionLength < 0.000001f) { denoiser_.ResetHistory(); return inputs.colorSrv; }
    ShadowParameters parameters = {};
    parameters.inverseViewProjection = MatrixMath::Inverse(inputs.camera->GetViewProjectionMatrix());
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!std::isfinite(parameters.inverseViewProjection.m[row][column])) { denoiser_.ResetHistory(); return inputs.colorSrv; }
        }
    }
    for (ID3D12Resource* resource : {inputs.depthTexture, inputs.normalTexture, inputs.directionalLightTexture}) {
        auto description = resource->GetDesc();
        if (description.Width != SceneRenderResolution::GetWidth() || description.Height != SceneRenderResolution::GetHeight()) { denoiser_.ResetHistory(); return inputs.colorSrv; }
    }
    try {
        if (!isReady_) {
            auto* dxCommon = DirectXCommon::GetInstance();
            RequireShadowResult(dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&device_)), "RT shadow device unavailable");
            RequireShadowResult(dxCommon->GetCommandList()->QueryInterface(IID_PPV_ARGS(&commandList_)), "RT shadow command list unavailable");
            CreateResources(); CreatePipelines(*inputs.scene); isReady_ = true;
        }
        parameters.lightDirection = inputs.lightDirection * (-1.0f / directionLength);
        parameters.sampleCount = settings_.sampleCount;
        parameters.normalBias = settings_.normalBias; parameters.rayBias = settings_.rayBias;
        parameters.sunAngularRadiusRadians = settings_.sunAngularRadiusRadians;
        parameters.maxRayDistance = settings_.maxRayDistance;
        parameters.sampleFrameIndex = 0;
        if (settings_.isDenoisingEnabled && settings_.shouldUseTemporalHistory && settings_.sunAngularRadiusRadians > 0) {
            parameters.sampleFrameIndex = ++sampleFrameIndex_;
            if (parameters.sampleFrameIndex == 0) { parameters.sampleFrameIndex = ++sampleFrameIndex_; }
        }
        uint32_t hitRecordCount = inputs.scene->GetStatistics().hitRecordCount;
        uint64_t tableSizeBytes = 128 + static_cast<uint64_t>(hitRecordCount) * 96;
        if (shaderTable_->GetDesc().Width < tableSizeBytes) {
            // Renderer waits for the previous frame before replacing upload resources.
            auto replacement = CreateShadowUpload(device_.Get(), tableSizeBytes);
            void* previousData = nullptr;
            void* replacementData = nullptr;
            RequireShadowResult(shaderTable_->Map(0, nullptr, &previousData), "Shadow table source mapping failed");
            RequireShadowResult(replacement->Map(0, nullptr, &replacementData), "Shadow table replacement mapping failed");
            std::memcpy(replacementData, previousData, 128);
            shaderTable_->Unmap(0, nullptr); replacement->Unmap(0, nullptr);
            shaderTable_ = replacement;
        }
        void* tableData = nullptr;
        RequireShadowResult(shaderTable_->Map(0, nullptr, &tableData), "Shadow hit table mapping failed");
        inputs.scene->WriteHitRecords(static_cast<uint8_t*>(tableData) + 128, 96, hitShaderIdentifier_);
        shaderTable_->Unmap(0, nullptr);
        void* data = nullptr;
        RequireShadowResult(parameters_->Map(0, nullptr, &data), "RT shadow parameters mapping failed");
        std::memcpy(data, &parameters, sizeof(parameters)); parameters_->Unmap(0, nullptr);
    } catch (const std::exception& error) {
        ResetResources();
        denoiser_.ResetHistory();
        settings_.isEnabled = false; status_ = error.what(); Logger::Error(status_); return inputs.colorSrv;
    }
    ID3D12Resource* resources[] = {inputs.depthTexture, inputs.normalTexture, inputs.directionalLightTexture};
    for (ID3D12Resource* resource : resources) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    auto maskBefore = CD3DX12_RESOURCE_BARRIER::Transition(mask_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList_->ResourceBarrier(1, &maskBefore);
    SrvManager::GetInstance()->PreDraw();
    commandList_->SetComputeRootSignature(traceRoot_.Get());
    commandList_->SetComputeRootShaderResourceView(0, inputs.scene->GetSceneGpuAddress());
    commandList_->SetComputeRootDescriptorTable(1, inputs.depthSrv);
    commandList_->SetComputeRootDescriptorTable(2, inputs.normalSrv);
    commandList_->SetComputeRootDescriptorTable(3, inputs.directionalLightSrv);
    commandList_->SetComputeRootDescriptorTable(4, SrvManager::GetInstance()->GetGPUDescriptorHandle(maskUavIndex_));
    commandList_->SetComputeRootConstantBufferView(5, parameters_->GetGPUVirtualAddress());
    commandList_->SetPipelineState1(tracePipeline_.Get());
    inputs.scene->BeginMaterialRead();
    uint64_t address = shaderTable_->GetGPUVirtualAddress();
    D3D12_DISPATCH_RAYS_DESC dispatch = {};
    dispatch.RayGenerationShaderRecord = {address, 32};
    dispatch.MissShaderTable = {address + 64, 64, 64};
    dispatch.HitGroupTable = {address + 128, static_cast<uint64_t>(inputs.scene->GetStatistics().hitRecordCount) * 96, 96};
    dispatch.Width = SceneRenderResolution::GetWidth(); dispatch.Height = SceneRenderResolution::GetHeight(); dispatch.Depth = 1;
    traceTimer_.Begin(); commandList_->DispatchRays(&dispatch); traceTimer_.End();
    inputs.scene->EndMaterialRead();
    for (ID3D12Resource* resource : resources) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(1, &barrier);
    }
    auto maskAfter = CD3DX12_RESOURCE_BARRIER::Transition(mask_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &maskAfter);
    auto visibilitySrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(maskSrvIndex_);
    if (settings_.isDenoisingEnabled && settings_.sunAngularRadiusRadians > 0) {
        ShadowDenoiserInputs denoiseInputs;
        denoiseInputs.camera = inputs.camera; denoiseInputs.rawMaskSrv = visibilitySrv;
        denoiseInputs.depthSrv = inputs.depthSrv; denoiseInputs.normalSrv = inputs.normalSrv;
        denoiseInputs.directionalLightSrv = inputs.directionalLightSrv;
        denoiseInputs.motionVectorSrv = inputs.motionVectorSrv;
        denoiseInputs.reprojectionSrv = inputs.reprojectionSrv; denoiseInputs.previousReprojectionSrv = inputs.previousReprojectionSrv;
        denoiseInputs.sceneRevision = inputs.sceneRevision;
        denoiseInputs.casterRevision = inputs.scene->GetShadowSceneRevision();
        denoiseInputs.lightDirection = parameters.lightDirection;
        denoiseInputs.shouldUseTemporalHistory = settings_.shouldUseTemporalHistory;
        denoiseInputs.maxHistoryFrames = settings_.maxHistoryFrames; denoiseInputs.spatialPassCount = settings_.spatialPassCount;
        visibilitySrv = denoiser_.Draw(denoiseInputs);
    } else { denoiser_.ResetHistory(); }
    auto colorBefore = CD3DX12_RESOURCE_BARRIER::Transition(color_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &colorBefore);
    auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtv, false, nullptr);
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(SceneRenderResolution::GetWidth()), static_cast<float>(SceneRenderResolution::GetHeight()), 0, 1};
    D3D12_RECT scissor = {0, 0, SceneRenderResolution::GetWidth(), SceneRenderResolution::GetHeight()};
    commandList_->RSSetViewports(1, &viewport); commandList_->RSSetScissorRects(1, &scissor);
    commandList_->SetGraphicsRootSignature(compositeRoot_.Get()); commandList_->SetPipelineState(compositePipeline_.Get());
    commandList_->SetGraphicsRootDescriptorTable(0, inputs.colorSrv);
    commandList_->SetGraphicsRootDescriptorTable(1, inputs.directionalLightSrv);
    commandList_->SetGraphicsRootDescriptorTable(2, visibilitySrv);
    commandList_->SetGraphicsRoot32BitConstant(3, static_cast<uint32_t>(settings_.isDebugVisible), 0);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    compositeTimer_.Begin(); commandList_->DrawInstanced(3, 1, 0, 0); compositeTimer_.End();
    auto colorAfter = CD3DX12_RESOURCE_BARRIER::Transition(color_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &colorAfter);
    hasValidFrame_ = true; status_ = "RT sun shadows ready";
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(colorSrvIndex_);
}
void DxrShadowRenderer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("RT Shadows")) {
        ImGui::TextUnformatted(status_.c_str());
        DxrShadowSettings settings = settings_;
        ImGui::Checkbox("Enabled (requires DXR)", &settings.isEnabled);
        ImGui::Checkbox("Show visibility mask", &settings.isDebugVisible);
        ImGui::Checkbox("Denoise soft shadows", &settings.isDenoisingEnabled);
        ImGui::Checkbox("Temporal accumulation", &settings.shouldUseTemporalHistory);
        int maxHistoryFrames = static_cast<int>(settings.maxHistoryFrames);
        ImGui::SliderInt("History frames", &maxHistoryFrames, 1, 64); settings.maxHistoryFrames = static_cast<uint32_t>(maxHistoryFrames);
        int spatialPassCount = static_cast<int>(settings.spatialPassCount);
        ImGui::SliderInt("Spatial passes", &spatialPassCount, 0, 3); settings.spatialPassCount = static_cast<uint32_t>(spatialPassCount);
        int sampleCount = static_cast<int>(settings.sampleCount);
        ImGui::SliderInt("Samples", &sampleCount, 1, 64); settings.sampleCount = static_cast<uint32_t>(sampleCount);
        ImGui::SliderFloat("Sun angular radius (radians)", &settings.sunAngularRadiusRadians, 0, 0.1f, "%.5f");
        ImGui::SliderFloat("Normal bias (world units)", &settings.normalBias, 0, 0.5f);
        ImGui::SliderFloat("Ray bias (world units)", &settings.rayBias, 0.0001f, 0.1f, "%.4f");
        SetSettings(settings);
        ImGui::Text("Trace %.3f ms / denoise %.3f ms / composite %.3f ms", GetTraceGpuTimeMs(), GetDenoiseGpuTimeMs(), GetCompositeGpuTimeMs());
        ImGui::Text("Denoise allocation %.2f MiB / history accepted %d", GetDenoiseAllocationBytes() / 1048576.0, static_cast<int>(HasUsedShadowHistory()));
        ImGui::Text("Mask + color allocation %.2f MiB", textureAllocationBytes_ / 1048576.0);
        ImGui::TextWrapped("Directional light only. Depth/normal guided filtering. History resets on caster, camera-cut, scene, sun or sampling changes.");
    }
    ImGui::End();
#endif
}
#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json DxrShadowRenderer::GetDevelopmentState() const {
    return {{"status", status_}, {"isEnabled", settings_.isEnabled}, {"isDebugVisible", settings_.isDebugVisible},
        {"isDenoisingEnabled", settings_.isDenoisingEnabled}, {"shouldUseTemporalHistory", settings_.shouldUseTemporalHistory},
        {"maxHistoryFrames", settings_.maxHistoryFrames}, {"spatialPassCount", settings_.spatialPassCount},
        {"denoiseGpuMs", GetDenoiseGpuTimeMs()}, {"denoiseAllocationBytes", GetDenoiseAllocationBytes()}, {"hasUsedHistory", HasUsedShadowHistory()},
        {"sampleCount", settings_.sampleCount}, {"sunAngularRadiusRadians", settings_.sunAngularRadiusRadians},
        {"normalBias", settings_.normalBias}, {"rayBias", settings_.rayBias}, {"maxRayDistance", settings_.maxRayDistance},
        {"traceGpuMs", GetTraceGpuTimeMs()}, {"compositeGpuMs", GetCompositeGpuTimeMs()}, {"textureAllocationBytes", textureAllocationBytes_}};
}
nlohmann::json DxrShadowRenderer::GetDevelopmentControls() const {
    return nlohmann::json::array({
        {{"key", "status"}, {"label", "状態"}, {"type", "metric"}},
        {{"key", "isEnabled"}, {"label", "RT太陽光シャドウを有効（DXRも有効にする）"}, {"type", "bool"}},
        {{"key", "isDebugVisible"}, {"label", "遮蔽マスクを表示"}, {"type", "bool"}},
        {{"key", "isDenoisingEnabled"}, {"label", "柔らかい影のノイズ除去"}, {"type", "bool"}},
        {{"key", "shouldUseTemporalHistory"}, {"label", "時間方向の蓄積"}, {"type", "bool"}},
        {{"key", "maxHistoryFrames"}, {"label", "最大履歴フレーム数"}, {"type", "number"}, {"minimum", 1}, {"maximum", 64}, {"step", 1}},
        {{"key", "spatialPassCount"}, {"label", "空間フィルター回数"}, {"type", "number"}, {"minimum", 0}, {"maximum", 3}, {"step", 1}},
        {{"key", "denoiseGpuMs"}, {"label", "ノイズ除去 (ms)"}, {"type", "metric"}},
        {{"key", "denoiseAllocationBytes"}, {"label", "ノイズ除去画像割当 (bytes)"}, {"type", "metric"}},
        {{"key", "hasUsedHistory"}, {"label", "履歴を使用"}, {"type", "metric"}},
        {{"key", "sampleCount"}, {"label", "サンプル数"}, {"type", "number"}, {"minimum", 1}, {"maximum", 64}, {"step", 1}},
        {{"key", "sunAngularRadiusRadians"}, {"label", "太陽の角半径 (rad)"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.2}, {"step", 0.001}},
        {{"key", "normalBias"}, {"label", "法線バイアス"}, {"type", "number"}, {"minimum", 0}, {"maximum", 10}, {"step", 0.001}},
        {{"key", "rayBias"}, {"label", "光線バイアス"}, {"type", "number"}, {"minimum", 0.0001}, {"maximum", 10}, {"step", 0.001}},
        {{"key", "maxRayDistance"}, {"label", "探索距離"}, {"type", "number"}, {"minimum", 1}, {"maximum", 1000000}, {"step", 100}},
        {{"key", "traceGpuMs"}, {"label", "RT探索 (ms)"}, {"type", "metric"}},
        {{"key", "compositeGpuMs"}, {"label", "合成 (ms)"}, {"type", "metric"}},
        {{"key", "textureAllocationBytes"}, {"label", "マスク＋合成画像割当 (bytes)"}, {"type", "metric"}}
    });
}
bool DxrShadowRenderer::SetDevelopmentBool(const std::string& key, bool isEnabled) {
    DxrShadowSettings settings = settings_;
    if (key == "isEnabled") { settings.isEnabled = isEnabled; }
    else if (key == "isDebugVisible") { settings.isDebugVisible = isEnabled; }
    else if (key == "isDenoisingEnabled") { settings.isDenoisingEnabled = isEnabled; }
    else if (key == "shouldUseTemporalHistory") { settings.shouldUseTemporalHistory = isEnabled; }
    else { return false; }
    return SetSettings(settings);
}
bool DxrShadowRenderer::SetDevelopmentNumber(const std::string& key, double value) {
    if (!std::isfinite(value)) { return false; }
    DxrShadowSettings settings = settings_;
    if (key == "sampleCount") {
        if (value < 1 || value > 64 || value != std::floor(value)) { return false; }
        settings.sampleCount = static_cast<uint32_t>(value);
    } else if (key == "maxHistoryFrames") {
        if (value < 1 || value > 64 || value != std::floor(value)) { return false; }
        settings.maxHistoryFrames = static_cast<uint32_t>(value);
    } else if (key == "spatialPassCount") {
        if (value < 0 || value > 3 || value != std::floor(value)) { return false; }
        settings.spatialPassCount = static_cast<uint32_t>(value);
    } else if (key == "sunAngularRadiusRadians") { settings.sunAngularRadiusRadians = static_cast<float>(value); }
    else if (key == "normalBias") { settings.normalBias = static_cast<float>(value); }
    else if (key == "rayBias") { settings.rayBias = static_cast<float>(value); }
    else if (key == "maxRayDistance") { settings.maxRayDistance = static_cast<float>(value); }
    else { return false; }
    return SetSettings(settings);
}
#endif
