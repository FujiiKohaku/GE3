#include "ShadowDenoiser.h"
#include "Engine/Camera/Camera.h"
#include "Engine/SrvManager/SrvManager.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
struct DenoiseParameters {
    Matrix4x4 inverseViewProjection;
    Matrix4x4 previousViewProjection;
    Matrix4x4 previousView;
    Matrix4x4 currentView;
    Vector2 jitterDeltaUv;
    uint32_t hasHistory;
    uint32_t hasMotionVectors;
    uint32_t maxHistoryFrames;
    uint32_t hasSceneChanges;
};
void RequireDenoiseResult(HRESULT result, const char* message) {
    if (FAILED(result)) { throw std::runtime_error(message); }
}
}
ShadowDenoiser::~ShadowDenoiser() { ReleaseResources(); }
void ShadowDenoiser::ResetHistory() {
    hasHistory_ = false; hasUsedHistory_ = false; outputIndex_ = UINT_MAX;
    timer_.ResetSample();
}
void ShadowDenoiser::ReleaseResources() {
    for (uint32_t& index : srvIndices_) {
        if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); index = UINT_MAX; }
    }
    for (auto& texture : textures_) { texture.Reset(); }
    rtvHeap_.Reset(); root_.Reset(); temporalPipeline_.Reset(); spatialPipeline_.Reset(); parameters_.Reset();
    allocationBytes_ = 0; isReady_ = false; ResetHistory();
}
void ShadowDenoiser::CreateResources() {
    auto* device = DirectXCommon::GetInstance()->GetDevice();
    auto* srvManager = SrvManager::GetInstance();
    if (!srvManager->CanAllocate(kTargetCount)) { throw std::runtime_error("Shadow denoiser descriptors exhausted"); }
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription = {};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDescription.NumDescriptors = kTargetCount;
    RequireDenoiseResult(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&rtvHeap_)), "Shadow denoiser RTV creation failed");
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    for (uint32_t index = 0; index < kTargetCount; ++index) {
        DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
        if (index < 4) {
            format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        }
        auto description = CD3DX12_RESOURCE_DESC::Tex2D(format, WinApp::kClientWidth, WinApp::kClientHeight,
            1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        RequireDenoiseResult(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index])), "Shadow denoiser texture creation failed");
        allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &description).SizeInBytes;
        srvIndices_[index] = srvManager->Allocate();
        srvManager->CreateSRVforTexture2D(srvIndices_[index], textures_[index].Get(), format, 1);
        rtvHandles_[index] = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandles_[index].ptr += static_cast<SIZE_T>(index) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(textures_[index].Get(), nullptr, rtvHandles_[index]);
    }
    D3D12_DESCRIPTOR_RANGE ranges[7] = {};
    D3D12_ROOT_PARAMETER parameters[9] = {};
    for (uint32_t index = 0; index < 7; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].BaseShaderRegister = index; ranges[index].NumDescriptors = 1;
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[index].DescriptorTable = {1, &ranges[index]};
    }
    parameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[8].Constants = {1, 0, 1};
    D3D12_ROOT_SIGNATURE_DESC signature = {9, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errors;
    RequireDenoiseResult(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "Shadow denoiser root serialization failed");
    RequireDenoiseResult(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_)), "Shadow denoiser root creation failed");
    auto vertex = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    auto temporal = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/ShadowTemporal.PS.hlsl");
    auto spatial = DirectXCommon::GetInstance()->LoadCompiledShader(L"resources/Shaders/Raytracing/ShadowSpatial.PS.hlsl");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = root_.Get(); pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    pipeline.PS = {temporal->GetBufferPointer(), temporal->GetBufferSize()};
    pipeline.BlendState.IndependentBlendEnable = TRUE;
    pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.SampleMask = UINT_MAX; pipeline.SampleDesc.Count = 1;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 2;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; pipeline.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    RequireDenoiseResult(device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&temporalPipeline_)), "Shadow temporal pipeline creation failed");
    pipeline.PS = {spatial->GetBufferPointer(), spatial->GetBufferSize()};
    pipeline.NumRenderTargets = 1; pipeline.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT; pipeline.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
    RequireDenoiseResult(device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&spatialPipeline_)), "Shadow spatial pipeline creation failed");
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    auto buffer = CD3DX12_RESOURCE_DESC::Buffer(512);
    RequireDenoiseResult(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&parameters_)), "Shadow denoiser parameter allocation failed");
    isReady_ = true;
}
void ShadowDenoiser::DrawPass(const ShadowDenoiserInputs& inputs, uint32_t targetIndex,
    D3D12_GPU_DESCRIPTOR_HANDLE signalSrv, uint32_t step, bool isTemporal) {
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    uint32_t targetCount = 1;
    if (isTemporal) { targetCount = 2; }
    for (uint32_t offset = 0; offset < targetCount; ++offset) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[targetIndex + offset].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);
    }
    commandList->OMSetRenderTargets(targetCount, &rtvHandles_[targetIndex], false, nullptr);
    commandList->SetGraphicsRootSignature(root_.Get());
    if (isTemporal) { commandList->SetPipelineState(temporalPipeline_.Get()); }
    else { commandList->SetPipelineState(spatialPipeline_.Get()); }
    commandList->SetGraphicsRootDescriptorTable(0, signalSrv);
    commandList->SetGraphicsRootDescriptorTable(1, inputs.depthSrv);
    commandList->SetGraphicsRootDescriptorTable(2, inputs.normalSrv);
    commandList->SetGraphicsRootDescriptorTable(3, inputs.directionalLightSrv);
    auto motionSrv = inputs.motionVectorSrv;
    if (motionSrv.ptr == 0) { motionSrv = inputs.normalSrv; }
    commandList->SetGraphicsRootDescriptorTable(4, motionSrv);
    uint32_t statisticsIndex = historyIndex_ * 2;
    if (!isTemporal) { statisticsIndex = (1 - historyIndex_) * 2; }
    commandList->SetGraphicsRootDescriptorTable(5, SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices_[statisticsIndex]));
    uint32_t geometryIndex = historyIndex_ * 2 + 1;
    if (!isTemporal) { geometryIndex = (1 - historyIndex_) * 2 + 1; }
    commandList->SetGraphicsRootDescriptorTable(6, SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices_[geometryIndex]));
    commandList->SetGraphicsRootConstantBufferView(7, parameters_->GetGPUVirtualAddress());
    commandList->SetGraphicsRoot32BitConstant(8, step, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    for (uint32_t offset = 0; offset < targetCount; ++offset) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(textures_[targetIndex + offset].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);
    }
}
D3D12_GPU_DESCRIPTOR_HANDLE ShadowDenoiser::Draw(const ShadowDenoiserInputs& inputs) {
    if (!inputs.camera || inputs.rawMaskSrv.ptr == 0 || inputs.depthSrv.ptr == 0
        || inputs.normalSrv.ptr == 0 || inputs.directionalLightSrv.ptr == 0 || inputs.maxHistoryFrames < 1
        || inputs.maxHistoryFrames > 64 || inputs.spatialPassCount > 3) { ResetHistory(); return inputs.rawMaskSrv; }
    DenoiseParameters parameters = {};
    parameters.inverseViewProjection = MatrixMath::Inverse(inputs.camera->GetViewProjectionMatrix());
    parameters.currentView = inputs.camera->GetViewMatrix();
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!std::isfinite(parameters.inverseViewProjection.m[row][column])) { ResetHistory(); return inputs.rawMaskSrv; }
        }
    }
    hasUsedHistory_ = hasHistory_ && inputs.shouldUseTemporalHistory && previousCamera_ == inputs.camera
        && previousCameraHistoryId_ == inputs.camera->GetMotionHistoryId() && previousSceneRevision_ == inputs.sceneRevision;
    bool hasSceneChanges = previousCasterRevision_ != inputs.casterRevision
        || previousLightDirection_.x != inputs.lightDirection.x || previousLightDirection_.y != inputs.lightDirection.y
        || previousLightDirection_.z != inputs.lightDirection.z;
    parameters.hasSceneChanges = static_cast<uint32_t>(hasSceneChanges);
    parameters.hasHistory = static_cast<uint32_t>(hasUsedHistory_);
    parameters.hasMotionVectors = static_cast<uint32_t>(inputs.motionVectorSrv.ptr != 0);
    parameters.previousViewProjection = previousViewProjection_; parameters.previousView = previousView_;
    parameters.maxHistoryFrames = inputs.maxHistoryFrames;
    const Vector2& jitter = inputs.camera->GetProjectionJitter();
    parameters.jitterDeltaUv = {(jitter.x - previousJitter_.x) * 0.5f, (previousJitter_.y - jitter.y) * 0.5f};
    try {
        if (!isReady_) { CreateResources(); }
        void* data = nullptr;
        RequireDenoiseResult(parameters_->Map(0, nullptr, &data), "Shadow denoiser parameters mapping failed");
        std::memcpy(data, &parameters, sizeof(parameters)); parameters_->Unmap(0, nullptr);
    } catch (const std::exception& error) {
        Logger::Error(error.what()); ReleaseResources(); return inputs.rawMaskSrv;
    }
    auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
    SrvManager::GetInstance()->PreDraw();
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight), 0, 1};
    D3D12_RECT scissor = {0, 0, WinApp::kClientWidth, WinApp::kClientHeight};
    commandList->RSSetViewports(1, &viewport); commandList->RSSetScissorRects(1, &scissor);
    uint32_t writeIndex = 1 - historyIndex_;
    timer_.Begin();
    DrawPass(inputs, writeIndex * 2, inputs.rawMaskSrv, 0, true);
    outputIndex_ = writeIndex * 2;
    for (uint32_t pass = 0; pass < inputs.spatialPassCount; ++pass) {
        uint32_t target = 4 + pass % 2;
        DrawPass(inputs, target, SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices_[outputIndex_]), 1u << pass, false);
        outputIndex_ = target;
    }
    timer_.End();
    historyIndex_ = writeIndex; hasHistory_ = inputs.shouldUseTemporalHistory;
    previousCamera_ = inputs.camera; previousCameraHistoryId_ = inputs.camera->GetMotionHistoryId();
    previousSceneRevision_ = inputs.sceneRevision; previousCasterRevision_ = inputs.casterRevision;
    previousViewProjection_ = inputs.camera->GetViewProjectionMatrix(); previousView_ = inputs.camera->GetViewMatrix();
    previousJitter_ = jitter; previousLightDirection_ = inputs.lightDirection;
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndices_[outputIndex_]);
}
ID3D12Resource* ShadowDenoiser::GetOutputTexture() const {
    if (outputIndex_ == UINT_MAX) { return nullptr; }
    return textures_[outputIndex_].Get();
}
ID3D12Resource* ShadowDenoiser::GetHistoryTexture() const {
    if (!hasHistory_) { return nullptr; }
    return textures_[historyIndex_ * 2].Get();
}
