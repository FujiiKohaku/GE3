#include "ScreenSpaceReflection.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/WinApp/WinApp.h"
#include <cassert>
ScreenSpaceReflection::~ScreenSpaceReflection() {
    for (uint32_t index : srvIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
    if (depthSrvIndex_ != UINT_MAX) {
        SrvManager::GetInstance()->Free(depthSrvIndex_);
        for (uint32_t index : depthMipSrvIndices_) { SrvManager::GetInstance()->Free(index); }
    }
}
void ScreenSpaceReflection::CreateTarget(uint32_t index, uint32_t width, uint32_t height) {
    auto* device = DirectXCommon::GetInstance()->GetDevice();
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width; desc.Height = height; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index]));
    assert(SUCCEEDED(result));
    rtvHandles_[index] = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandles_[index].ptr += index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(textures_[index].Get(), nullptr, rtvHandles_[index]);
    auto* srvManager = SrvManager::GetInstance();
    srvIndices_[index] = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(srvIndices_[index], textures_[index].Get(), desc.Format, 1);
    srvHandles_[index] = srvManager->GetGPUDescriptorHandle(srvIndices_[index]);
}
void ScreenSpaceReflection::Initialize() {
    auto* dxCommon = DirectXCommon::GetInstance(); auto* device = dxCommon->GetDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {}; heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors = kTargetCount + kDepthMipCount;
    HRESULT result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)); assert(SUCCEEDED(result));
    CreateTarget(0, WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    CreateTarget(1, WinApp::kClientWidth, WinApp::kClientHeight);
    for (uint32_t index = 2; index < kTargetCount; ++index) {
        CreateTarget(index, WinApp::kClientWidth / 2, WinApp::kClientHeight / 2);
    }
    D3D12_DESCRIPTOR_RANGE ranges[9] = {}; D3D12_ROOT_PARAMETER roots[10] = {};
    for (uint32_t index = 0; index < 9; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[index].NumDescriptors = 1;
        ranges[index].BaseShaderRegister = index; ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        roots[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        roots[index].DescriptorTable = {1, &ranges[index]};
    }
    roots[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; roots[9].Descriptor.ShaderRegister = 0;
    roots[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler = {}; sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature = {}; signature.NumParameters = 10; signature.pParameters = roots;
    signature.NumStaticSamplers = 1; signature.pStaticSamplers = &sampler;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errors;
    result = D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors); assert(SUCCEEDED(result));
    result = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)); assert(SUCCEEDED(result));
    const wchar_t* kShaderPaths[] = {L"resources/Shaders/Reflection/Trace.PS.hlsl", L"resources/Shaders/Reflection/Composite.PS.hlsl", L"resources/Shaders/Reflection/Temporal.PS.hlsl",
        L"resources/Shaders/Reflection/BlurHorizontal.PS.hlsl", L"resources/Shaders/Reflection/BlurVertical.PS.hlsl"};
    auto vertex = dxCommon->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    for (uint32_t index = 0; index < 5; ++index) {
        auto pixel = dxCommon->LoadCompiledShader(kShaderPaths[index]);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {}; desc.pRootSignature = rootSignature_.Get();
        desc.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()}; desc.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = UINT_MAX; desc.SampleDesc.Count = 1; desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (index == 0 || index == 2) {
            desc.NumRenderTargets = 2;
            desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.BlendState.IndependentBlendEnable = TRUE;
            desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        result = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates_[index])); assert(SUCCEEDED(result));
    }
    parameterResource_ = dxCommon->CreateBufferResource(512);
    parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_)); timer_.Initialize();
    CreateDepthPyramid();
}
void ScreenSpaceReflection::Render(uint32_t index, const ScreenSpaceReflectionInputs& inputs) {
    auto* list = DirectXCommon::GetInstance()->GetCommandList();
    uint32_t targetIndex = index;
    uint32_t metadataIndex = 2;
    uint32_t targetCount = 1;
    if (index == 0) { targetCount = 2; }
    if (index == 2) { targetIndex = historyWriteIndex_; metadataIndex = targetIndex + 1; targetCount = 2; }
    if (index >= 3) { targetIndex = index + 4; }
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    const uint32_t kIndices[] = {targetIndex, metadataIndex};
    D3D12_CPU_DESCRIPTOR_HANDLE targets[] = {rtvHandles_[targetIndex], rtvHandles_[metadataIndex]};
    for (uint32_t target = 0; target < targetCount; ++target) {
        barriers[target].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[target].Transition.pResource = textures_[kIndices[target]].Get();
        barriers[target].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[target].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[target].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    list->ResourceBarrier(targetCount, barriers);
    auto desc = textures_[targetIndex]->GetDesc();
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
    list->RSSetViewports(1, &viewport); list->RSSetScissorRects(1, &scissor);
    list->OMSetRenderTargets(targetCount, targets, false, nullptr);
    list->SetGraphicsRootSignature(rootSignature_.Get()); list->SetPipelineState(pipelineStates_[index].Get());
    list->SetGraphicsRootDescriptorTable(0, inputs.colorSrv); list->SetGraphicsRootDescriptorTable(1, inputs.depthSrv);
    list->SetGraphicsRootDescriptorTable(2, inputs.normalSrv);
    D3D12_GPU_DESCRIPTOR_HANDLE reflectionHandle = inputs.colorSrv;
    if (index == 1 || index >= 3) { reflectionHandle = srvHandles_[reflectionIndex_]; }
    if (index == 2) { reflectionHandle = srvHandles_[0]; }
    list->SetGraphicsRootDescriptorTable(3, reflectionHandle);
    list->SetGraphicsRootDescriptorTable(4, SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_));
    list->SetGraphicsRootDescriptorTable(5, srvHandles_[2]);
    uint32_t readIndex = 3;
    if (historyWriteIndex_ == 3) { readIndex = 5; }
    list->SetGraphicsRootDescriptorTable(6, srvHandles_[readIndex]);
    list->SetGraphicsRootDescriptorTable(7, srvHandles_[readIndex + 1]);
    D3D12_GPU_DESCRIPTOR_HANDLE motionHandle = inputs.normalSrv;
    if (inputs.motionVectorSrv.ptr != 0) { motionHandle = inputs.motionVectorSrv; }
    list->SetGraphicsRootDescriptorTable(8, motionHandle);
    list->SetGraphicsRootConstantBufferView(9, parameterResource_->GetGPUVirtualAddress());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); list->DrawInstanced(3, 1, 0, 0);
    for (uint32_t target = 0; target < targetCount; ++target) {
        barriers[target].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[target].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    list->ResourceBarrier(targetCount, barriers);
}
D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceReflection::Draw(const ScreenSpaceReflectionInputs& inputs) {
    timer_.ResetSample();
    if (!settings_.isEnabled || inputs.camera == nullptr) {
        isDepthPyramidPrepared_ = false; ResetHistory(); return inputs.colorSrv;
    }
    bool shouldResolveHistory = settings_.shouldUseTemporalHistory && inputs.motionVectorSrv.ptr != 0 &&
        static_cast<uint32_t>(settings_.debugMode) <= 1;
    if (!shouldResolveHistory || historyCamera_ != inputs.camera || cameraHistoryId_ != inputs.camera->GetMotionHistoryId() ||
        sceneRevision_ != inputs.sceneRevision || previousSettings_.shouldUseHierarchicalDepth != settings_.shouldUseHierarchicalDepth ||
        previousSettings_.maxDistance != settings_.maxDistance || previousSettings_.thickness != settings_.thickness) {
        ResetHistory();
    }
    parameterData_->projection = inputs.camera->GetProjectionMatrix();
    parameterData_->inverseProjection = MatrixMath::Inverse(parameterData_->projection);
    parameterData_->view = inputs.camera->GetViewMatrix();
    parameterData_->settings = {settings_.maxDistance, settings_.thickness, settings_.strength, 0};
    parameterData_->settings.w = static_cast<float>(settings_.debugMode);
    parameterData_->traversal = {0, 0, 0, 0};
    if (settings_.shouldUseHierarchicalDepth) { parameterData_->traversal.x = 1; }
    parameterData_->currentToPreviousView = MatrixMath::MakeIdentity4x4();
    parameterData_->previousProjection = parameterData_->projection;
    parameterData_->temporal = {0, settings_.historyWeight, 0, 0};
    parameterData_->filter = {settings_.maxBlurRadiusPx, 0, 0, 0};
    bool shouldBlur = settings_.shouldBlurReflection && static_cast<uint32_t>(settings_.debugMode) <= 1;
    if (shouldBlur) { parameterData_->filter.y = 1; }
    if (hasHistory_) {
        Vector2 jitterNdc = inputs.camera->GetProjectionJitter();
        parameterData_->temporal.z = (jitterNdc.x - previousJitterNdc_.x) * 0.5f;
        parameterData_->temporal.w = (jitterNdc.y - previousJitterNdc_.y) * -0.5f;
        parameterData_->currentToPreviousView = MatrixMath::Multiply(MatrixMath::Inverse(parameterData_->view), previousView_);
        parameterData_->previousProjection = previousProjection_;
        parameterData_->temporal.x = 1;
    }
    timer_.Begin();
    if (settings_.shouldUseHierarchicalDepth && !isDepthPyramidPrepared_) { BuildDepthPyramid(inputs); }
    isDepthPyramidPrepared_ = false;
    Render(0, inputs);
    reflectionIndex_ = 0;
    if (shouldResolveHistory) {
        Render(2, inputs);
        reflectionIndex_ = historyWriteIndex_;
    }
    if (shouldBlur) {
        Render(3, inputs); reflectionIndex_ = 7;
        Render(4, inputs); reflectionIndex_ = 8;
    }
    Render(1, inputs); timer_.End();
    if (shouldResolveHistory) {
        hasHistory_ = true;
        historyWriteIndex_ = 8 - historyWriteIndex_;
        previousView_ = parameterData_->view; previousProjection_ = parameterData_->projection;
        previousJitterNdc_ = inputs.camera->GetProjectionJitter();
        historyCamera_ = inputs.camera; cameraHistoryId_ = inputs.camera->GetMotionHistoryId();
        sceneRevision_ = inputs.sceneRevision; previousSettings_ = settings_;
    }
    return srvHandles_[1];
}
D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceReflection::GetDepthPyramidSrv() const
{
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSrvIndex_);
}

void ScreenSpaceReflection::PrepareDepthPyramid(const ScreenSpaceReflectionInputs& inputs)
{
    isDepthPyramidPrepared_ = false;
    if (inputs.camera == nullptr || inputs.depthSrv.ptr == 0) { return; }
    parameterData_->projection = inputs.camera->GetProjectionMatrix();
    parameterData_->inverseProjection = MatrixMath::Inverse(parameterData_->projection);
    BuildDepthPyramid(inputs);
    isDepthPyramidPrepared_ = true;
}

void ScreenSpaceReflection::CreateDepthPyramid() {
    auto* dxCommon = DirectXCommon::GetInstance();
    auto* device = dxCommon->GetDevice();
    auto* srvManager = SrvManager::GetInstance();
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = 2048; description.Height = 1024;
    description.DepthOrArraySize = 1; description.MipLevels = kDepthMipCount;
    description.Format = DXGI_FORMAT_R32G32_FLOAT;
    description.SampleDesc.Count = 1;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&depthPyramid_));
    assert(SUCCEEDED(result));
    depthSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(depthSrvIndex_, depthPyramid_.Get(), description.Format, kDepthMipCount);
    for (uint32_t mipIndex = 0; mipIndex < kDepthMipCount; ++mipIndex) {
        depthMipSrvIndices_[mipIndex] = srvManager->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = description.Format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MostDetailedMip = mipIndex; srv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(depthPyramid_.Get(), &srv, srvManager->GetCPUDescriptorHandle(depthMipSrvIndices_[mipIndex]));
        depthRtvHandles_[mipIndex] = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        depthRtvHandles_[mipIndex].ptr += (mipIndex + kTargetCount) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
        rtv.Format = description.Format; rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv.Texture2D.MipSlice = mipIndex;
        device->CreateRenderTargetView(depthPyramid_.Get(), &rtv, depthRtvHandles_[mipIndex]);
    }
    auto vertex = dxCommon->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    const wchar_t* kPaths[] = {L"resources/Shaders/Reflection/DepthBase.PS.hlsl", L"resources/Shaders/Reflection/DepthReduce.PS.hlsl"};
    for (uint32_t index = 0; index < 2; ++index) {
        auto pixel = dxCommon->LoadCompiledShader(kPaths[index]);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
        pipeline.pRootSignature = rootSignature_.Get();
        pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
        pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
        pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pipeline.SampleMask = UINT_MAX; pipeline.SampleDesc.Count = 1;
        pipeline.NumRenderTargets = 1; pipeline.RTVFormats[0] = description.Format;
        pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        result = device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&depthPipelineStates_[index]));
        assert(SUCCEEDED(result));
    }
}
void ScreenSpaceReflection::BuildDepthPyramid(const ScreenSpaceReflectionInputs& inputs) {
    auto* list = DirectXCommon::GetInstance()->GetCommandList();
    auto* srvManager = SrvManager::GetInstance();
    list->SetGraphicsRootSignature(rootSignature_.Get());
    list->SetGraphicsRootDescriptorTable(0, inputs.colorSrv);
    list->SetGraphicsRootDescriptorTable(1, inputs.depthSrv);
    list->SetGraphicsRootDescriptorTable(2, inputs.normalSrv);
    list->SetGraphicsRootDescriptorTable(3, inputs.colorSrv);
    list->SetGraphicsRootConstantBufferView(9, parameterResource_->GetGPUVirtualAddress());
    for (uint32_t mipIndex = 0; mipIndex < kDepthMipCount; ++mipIndex) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = depthPyramid_.Get();
        barrier.Transition.Subresource = mipIndex;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        list->ResourceBarrier(1, &barrier);
        uint32_t sourceIndex = depthSrvIndex_;
        uint32_t pipelineIndex = 0;
        if (mipIndex > 0) {
            sourceIndex = depthMipSrvIndices_[mipIndex - 1]; pipelineIndex = 1;
        }
        list->SetGraphicsRootDescriptorTable(4, srvManager->GetGPUDescriptorHandle(sourceIndex));
        list->SetPipelineState(depthPipelineStates_[pipelineIndex].Get());
        uint32_t width = 2048 >> mipIndex;
        uint32_t height = 1024 >> mipIndex;
        if (height == 0) { height = 1; }
        D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        list->RSSetViewports(1, &viewport); list->RSSetScissorRects(1, &scissor);
        list->OMSetRenderTargets(1, &depthRtvHandles_[mipIndex], false, nullptr);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); list->DrawInstanced(3, 1, 0, 0);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        list->ResourceBarrier(1, &barrier);
    }
}
void ScreenSpaceReflection::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("Screen space reflection")) {
        ImGui::Checkbox("Enabled", &settings_.isEnabled);
        ImGui::Checkbox("Hierarchical depth", &settings_.shouldUseHierarchicalDepth);
        ImGui::Checkbox("Temporal history", &settings_.shouldUseTemporalHistory);
        ImGui::SliderFloat("History weight", &settings_.historyWeight, 0, 0.95f);
        ImGui::Checkbox("Roughness blur", &settings_.shouldBlurReflection);
        ImGui::SliderFloat("Maximum blur radius (px)", &settings_.maxBlurRadiusPx, 0, 32);
        if (ImGui::Button("Reset reflection history")) { ResetHistory(); }
        const char* kDebugModes[] = {"Composite", "Reflection only", "View depth", "Reflection direction", "Hit coordinates", "Hit status"};
        int debugMode = static_cast<int>(settings_.debugMode);
        if (ImGui::Combo("Debug view", &debugMode, kDebugModes, 6)) {
            settings_.debugMode = static_cast<ScreenSpaceReflectionDebugMode>(debugMode);
        }
        ImGui::SliderFloat("Strength", &settings_.strength, 0, 1);
        ImGui::SliderFloat("Distance", &settings_.maxDistance, 10, 300);
        ImGui::SliderFloat("Thickness", &settings_.thickness, 0.1f, 5);
        ImGui::Text("SSR GPU: %.3f ms", GetGpuTimeMs());
    }
    ImGui::End();
#endif
}
