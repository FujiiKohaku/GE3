#include "Engine/Renderer/SceneRenderResolution.h"
#include "ScreenSpaceGlobalIllumination.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include <cassert>
#include <cmath>

ScreenSpaceGlobalIllumination::~ScreenSpaceGlobalIllumination()
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
    for (uint32_t index : srvIndices_) {
        if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); }
    }
}

void ScreenSpaceGlobalIllumination::CreateTarget(uint32_t index, uint32_t width, uint32_t height)
{
    auto* device = DirectXCommon::GetInstance()->GetDevice();
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width;
    description.Height = height;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    description.SampleDesc.Count = 1;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index]));
    assert(SUCCEEDED(result));
    rtvHandles_[index] = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandles_[index].ptr += index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(textures_[index].Get(), nullptr, rtvHandles_[index]);
    auto* srvManager = SrvManager::GetInstance();
    if (srvIndices_[index] == UINT_MAX) { srvIndices_[index] = srvManager->Allocate(); }
    srvManager->CreateSRVforTexture2D(srvIndices_[index], textures_[index].Get(), description.Format, 1);
    srvHandles_[index] = srvManager->GetGPUDescriptorHandle(srvIndices_[index]);
}

void ScreenSpaceGlobalIllumination::Initialize()
{
    auto* dxCommon = DirectXCommon::GetInstance();
    auto* device = dxCommon->GetDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heap {};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.NumDescriptors = kTargetCount;
    HRESULT result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvHeap_));
    assert(SUCCEEDED(result));
    for (uint32_t index = 0; index < kTargetCount; ++index) {
        uint32_t width = SceneRenderResolution::GetWidth() / 2;
        uint32_t height = SceneRenderResolution::GetHeight() / 2;
        if (index == 8) { width = SceneRenderResolution::GetWidth(); height = SceneRenderResolution::GetHeight(); }
        CreateTarget(index, width, height);
    }
    D3D12_DESCRIPTOR_RANGE ranges[10] {};
    D3D12_ROOT_PARAMETER roots[11] {};
    for (uint32_t index = 0; index < 10; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].NumDescriptors = 1;
        ranges[index].BaseShaderRegister = index;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        roots[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        roots[index].DescriptorTable = {1, &ranges[index]};
    }
    roots[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    roots[10].Descriptor.ShaderRegister = 0;
    roots[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature {};
    signature.NumParameters = _countof(roots);
    signature.pParameters = roots;
    signature.NumStaticSamplers = 1;
    signature.pStaticSamplers = &sampler;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errors;
    result = D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    assert(SUCCEEDED(result));
    result = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
    const wchar_t* kPaths[] = {L"resources/Shaders/GlobalIllumination/Trace.PS.hlsl",
        L"resources/Shaders/GlobalIllumination/Temporal.PS.hlsl",
        L"resources/Shaders/GlobalIllumination/BlurHorizontal.PS.hlsl",
        L"resources/Shaders/GlobalIllumination/BlurVertical.PS.hlsl",
        L"resources/Shaders/GlobalIllumination/Composite.PS.hlsl"};
    auto vertex = dxCommon->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    for (uint32_t index = 0; index < kPassCount; ++index) {
        auto pixel = dxCommon->LoadCompiledShader(kPaths[index]);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline {};
        pipeline.pRootSignature = rootSignature_.Get();
        pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
        pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
        pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pipeline.SampleMask = UINT_MAX;
        pipeline.SampleDesc.Count = 1;
        pipeline.NumRenderTargets = 1;
        pipeline.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (index <= 1) {
            pipeline.NumRenderTargets = 2;
            pipeline.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            pipeline.BlendState.IndependentBlendEnable = TRUE;
            pipeline.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        result = device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pipelines_[index]));
        assert(SUCCEEDED(result));
    }
    static_assert(sizeof(Parameters) <= 512);
    parameterResource_ = dxCommon->CreateBufferResource(512);
    parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
    timer_.Initialize();
    isInitialized_ = true;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().RegisterSource<ScreenSpaceGlobalIllumination>(this, "ssgi", "間接光 SSGI", false,
        &ScreenSpaceGlobalIllumination::GetDevelopmentState, &ScreenSpaceGlobalIllumination::GetDevelopmentControls,
        &ScreenSpaceGlobalIllumination::SetDevelopmentBool, &ScreenSpaceGlobalIllumination::SetDevelopmentNumber,
        &ScreenSpaceGlobalIllumination::ExecuteDevelopmentCommand);
#endif
}

bool ScreenSpaceGlobalIllumination::SetSettings(const ScreenSpaceGlobalIlluminationSettings& settings)
{
    if (!std::isfinite(settings.strength) || settings.strength < 0 || settings.strength > 2 ||
        !std::isfinite(settings.maxDistance) || settings.maxDistance < 0.5f || settings.maxDistance > 100 ||
        !std::isfinite(settings.thickness) || settings.thickness < 0.01f || settings.thickness > 5 ||
        !std::isfinite(settings.historyWeight) || settings.historyWeight < 0 || settings.historyWeight > 0.95f ||
        !std::isfinite(settings.maxRadiance) || settings.maxRadiance < 0.1f || settings.maxRadiance > 32 ||
        settings.rayCount < 1 || settings.rayCount > 16 || settings.stepCount < 4 || settings.stepCount > 64 ||
        static_cast<uint32_t>(settings.debugMode) > 2) { return false; }
    if (settings.isEnabled != settings_.isEnabled || settings.strength != settings_.strength ||
        settings.maxDistance != settings_.maxDistance || settings.thickness != settings_.thickness ||
        settings.historyWeight != settings_.historyWeight || settings.maxRadiance != settings_.maxRadiance ||
        settings.rayCount != settings_.rayCount || settings.stepCount != settings_.stepCount ||
        settings.debugMode != settings_.debugMode || settings.shouldUseTemporalHistory != settings_.shouldUseTemporalHistory ||
        settings.shouldBlur != settings_.shouldBlur || settings.shouldUseHierarchicalDepth != settings_.shouldUseHierarchicalDepth) {
        ResetHistory();
        ++settingsRevision_;
    }
    settings_ = settings;
    return true;
}

void ScreenSpaceGlobalIllumination::SetEnabled(bool isEnabled)
{
    auto settings = settings_;
    settings.isEnabled = isEnabled;
    SetSettings(settings);
}

void ScreenSpaceGlobalIllumination::ResetHistory()
{
    hasHistory_ = false;
    frameIndex_ = 0;
}

void ScreenSpaceGlobalIllumination::Render(uint32_t passIndex, uint32_t targetIndex, uint32_t sourceIndex,
    const ScreenSpaceGlobalIlluminationInputs& inputs)
{
    auto* list = DirectXCommon::GetInstance()->GetCommandList();
    uint32_t targetCount = 1;
    if (passIndex <= 1) { targetCount = 2; }
    D3D12_RESOURCE_BARRIER barriers[2] {};
    D3D12_CPU_DESCRIPTOR_HANDLE targets[2] {};
    for (uint32_t index = 0; index < targetCount; ++index) {
        barriers[index].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[index].Transition.pResource = textures_[targetIndex + index].Get();
        barriers[index].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[index].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[index].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        targets[index] = rtvHandles_[targetIndex + index];
    }
    list->ResourceBarrier(targetCount, barriers);
    auto description = textures_[targetIndex]->GetDesc();
    D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(description.Width), static_cast<float>(description.Height), 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(description.Width), static_cast<LONG>(description.Height)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &scissor);
    list->OMSetRenderTargets(targetCount, targets, false, nullptr);
    list->SetGraphicsRootSignature(rootSignature_.Get());
    list->SetPipelineState(pipelines_[passIndex].Get());
    uint32_t historyReadIndex = 2;
    if (historyWriteIndex_ == 2) { historyReadIndex = 4; }
    D3D12_GPU_DESCRIPTOR_HANDLE motion = inputs.motionVectorSrv;
    if (motion.ptr == 0) { motion = inputs.normalSrv; }
    D3D12_GPU_DESCRIPTOR_HANDLE pyramid = inputs.depthPyramidSrv;
    if (pyramid.ptr == 0) { pyramid = inputs.normalSrv; }
    const D3D12_GPU_DESCRIPTOR_HANDLE kHandles[] = {inputs.colorSrv, inputs.depthSrv, inputs.normalSrv,
        inputs.materialSrv, motion, pyramid, srvHandles_[sourceIndex], srvHandles_[1],
        srvHandles_[historyReadIndex], srvHandles_[historyReadIndex + 1]};
    for (uint32_t index = 0; index < _countof(kHandles); ++index) {
        list->SetGraphicsRootDescriptorTable(index, kHandles[index]);
    }
    list->SetGraphicsRootConstantBufferView(10, parameterResource_->GetGPUVirtualAddress());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    for (uint32_t index = 0; index < targetCount; ++index) {
        barriers[index].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[index].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    list->ResourceBarrier(targetCount, barriers);
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceGlobalIllumination::Draw(const ScreenSpaceGlobalIlluminationInputs& inputs)
{
    timer_.ResetSample();
    if (!isInitialized_ || !settings_.isEnabled || inputs.camera == nullptr || inputs.colorSrv.ptr == 0 ||
        inputs.depthSrv.ptr == 0 || inputs.normalSrv.ptr == 0 || inputs.materialSrv.ptr == 0 ||
        (settings_.strength == 0 && settings_.debugMode == ScreenSpaceGlobalIlluminationDebugMode::None)) {
        ResetHistory();
        return inputs.colorSrv;
    }
    bool shouldResolveHistory = settings_.shouldUseTemporalHistory && inputs.motionVectorSrv.ptr != 0;
    if (!shouldResolveHistory || historyCamera_ != inputs.camera ||
        cameraHistoryId_ != inputs.camera->GetMotionHistoryId() || sceneRevision_ != inputs.sceneRevision) { ResetHistory(); }
    parameterData_->projection = inputs.camera->GetProjectionMatrix();
    parameterData_->inverseProjection = MatrixMath::Inverse(parameterData_->projection);
    parameterData_->view = inputs.camera->GetViewMatrix();
    parameterData_->currentToPreviousView = MatrixMath::MakeIdentity4x4();
    parameterData_->previousProjection = parameterData_->projection;
    parameterData_->tracing = {settings_.maxDistance, settings_.thickness, settings_.maxRadiance, 0};
    if (settings_.shouldUseHierarchicalDepth && inputs.depthPyramidSrv.ptr != 0) { parameterData_->tracing.w = 1; }
    parameterData_->sampling = {static_cast<float>(settings_.rayCount), static_cast<float>(settings_.stepCount), 0, 0};
    if (shouldResolveHistory) { parameterData_->sampling.z = static_cast<float>(frameIndex_ % 8); }
    parameterData_->temporal = {0, settings_.historyWeight, 0, 0};
    if (hasHistory_) {
        parameterData_->temporal.x = 1;
        parameterData_->currentToPreviousView = MatrixMath::Multiply(MatrixMath::Inverse(parameterData_->view), previousView_);
        parameterData_->previousProjection = previousProjection_;
        Vector2 jitterNdc = inputs.camera->GetProjectionJitter();
        parameterData_->temporal.z = (jitterNdc.x - previousJitterNdc_.x) * 0.5f;
        parameterData_->temporal.w = (jitterNdc.y - previousJitterNdc_.y) * -0.5f;
    }
    parameterData_->composition = {settings_.strength, static_cast<float>(settings_.debugMode), 0, 0};
    timer_.Begin();
    Render(0, 0, 0, inputs);
    uint32_t sourceIndex = 0;
    if (shouldResolveHistory) {
        Render(1, historyWriteIndex_, sourceIndex, inputs);
        sourceIndex = historyWriteIndex_;
    }
    if (settings_.shouldBlur && settings_.debugMode != ScreenSpaceGlobalIlluminationDebugMode::RawIndirectLight) {
        Render(2, 6, sourceIndex, inputs);
        Render(3, 7, 6, inputs);
        sourceIndex = 7;
    }
    if (settings_.debugMode == ScreenSpaceGlobalIlluminationDebugMode::RawIndirectLight) { sourceIndex = 0; }
    Render(4, 8, sourceIndex, inputs);
    timer_.End();
    if (shouldResolveHistory) {
        hasHistory_ = true;
        historyWriteIndex_ = 6 - historyWriteIndex_;
        ++frameIndex_;
        historyCamera_ = inputs.camera;
        cameraHistoryId_ = inputs.camera->GetMotionHistoryId();
        sceneRevision_ = inputs.sceneRevision;
        previousView_ = parameterData_->view;
        previousProjection_ = parameterData_->projection;
        previousJitterNdc_ = inputs.camera->GetProjectionJitter();
    }
    return srvHandles_[8];
}

void ScreenSpaceGlobalIllumination::DrawImGui()
{
#ifdef USE_IMGUI
    auto settings = settings_;
    if (ImGui::Begin("Screen space global illumination")) {
        ImGui::Checkbox("Enabled", &settings.isEnabled);
        ImGui::Checkbox("Temporal history", &settings.shouldUseTemporalHistory);
        ImGui::Checkbox("Edge-aware blur", &settings.shouldBlur);
        ImGui::Checkbox("Hierarchical depth", &settings.shouldUseHierarchicalDepth);
        ImGui::SliderFloat("Strength", &settings.strength, 0, 2);
        ImGui::SliderFloat("Distance", &settings.maxDistance, 0.5f, 100);
        ImGui::SliderFloat("Thickness", &settings.thickness, 0.01f, 5);
        ImGui::SliderFloat("History weight", &settings.historyWeight, 0, 0.95f);
        ImGui::SliderFloat("Radiance limit", &settings.maxRadiance, 0.1f, 32);
        int rayCount = static_cast<int>(settings.rayCount);
        int stepCount = static_cast<int>(settings.stepCount);
        ImGui::SliderInt("Rays", &rayCount, 1, 16);
        ImGui::SliderInt("Steps", &stepCount, 4, 64);
        settings.rayCount = static_cast<uint32_t>(rayCount);
        settings.stepCount = static_cast<uint32_t>(stepCount);
        const char* kModes[] = {"Composite", "Indirect light", "Raw indirect light"};
        int mode = static_cast<int>(settings.debugMode);
        ImGui::Combo("View", &mode, kModes, _countof(kModes));
        settings.debugMode = static_cast<ScreenSpaceGlobalIlluminationDebugMode>(mode);
        if (ImGui::Button("Reset SSGI history")) { ResetHistory(); }
        ImGui::Text("SSGI GPU: %.3f ms", GetGpuTimeMs());
    }
    ImGui::End();
    SetSettings(settings);
#endif
}

#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json ScreenSpaceGlobalIllumination::GetDevelopmentState() const
{
    return {{"enabled", settings_.isEnabled}, {"temporal", settings_.shouldUseTemporalHistory},
        {"blur", settings_.shouldBlur}, {"hierarchicalDepth", settings_.shouldUseHierarchicalDepth},
        {"strength", settings_.strength}, {"distance", settings_.maxDistance}, {"thickness", settings_.thickness},
        {"historyWeight", settings_.historyWeight}, {"radianceLimit", settings_.maxRadiance},
        {"rayCount", settings_.rayCount}, {"stepCount", settings_.stepCount},
        {"view", static_cast<uint32_t>(settings_.debugMode)}, {"gpuMs", GetGpuTimeMs()}};
}
nlohmann::json ScreenSpaceGlobalIllumination::GetDevelopmentControls() const
{
    return nlohmann::json::array({
        {{"key", "enabled"}, {"label", "SSGIを有効"}, {"type", "bool"}},
        {{"key", "temporal"}, {"label", "履歴で安定化"}, {"type", "bool"}},
        {{"key", "blur"}, {"label", "境界を保つぼかし"}, {"type", "bool"}},
        {{"key", "hierarchicalDepth"}, {"label", "共有Hi-Zを使用"}, {"type", "bool"}},
        {{"key", "strength"}, {"label", "間接光の強度"}, {"type", "number"}, {"minimum", 0}, {"maximum", 2}, {"step", 0.01}},
        {{"key", "distance"}, {"label", "探索距離"}, {"type", "number"}, {"minimum", 0.5}, {"maximum", 100}, {"step", 0.1}},
        {{"key", "thickness"}, {"label", "深度の許容厚さ"}, {"type", "number"}, {"minimum", 0.01}, {"maximum", 5}, {"step", 0.01}},
        {{"key", "historyWeight"}, {"label", "履歴の割合"}, {"type", "number"}, {"minimum", 0}, {"maximum", 0.95}, {"step", 0.01}},
        {{"key", "radianceLimit"}, {"label", "拾う光の上限"}, {"type", "number"}, {"minimum", 0.1}, {"maximum", 32}, {"step", 0.1}},
        {{"key", "rayCount"}, {"label", "探索方向数"}, {"type", "number"}, {"minimum", 1}, {"maximum", 16}, {"step", 1}},
        {{"key", "stepCount"}, {"label", "探索ステップ数"}, {"type", "number"}, {"minimum", 4}, {"maximum", 64}, {"step", 1}},
        {{"key", "view"}, {"label", "表示"}, {"type", "select"}, {"options", nlohmann::json::array({
            {{"value", 0}, {"label", "合成"}}, {{"value", 1}, {"label", "間接光のみ"}}, {{"value", 2}, {"label", "探索結果のみ"}}})}},
        {{"key", "gpuMs"}, {"label", "SSGI GPU時間 ms"}, {"type", "number"}, {"readOnly", true}},
        {{"key", "resetHistory"}, {"label", "履歴をリセット"}, {"type", "button"}}
    });
}
bool ScreenSpaceGlobalIllumination::SetDevelopmentBool(const std::string& key, bool isEnabled)
{
    auto settings = settings_;
    if (key == "enabled") { settings.isEnabled = isEnabled; }
    else if (key == "temporal") { settings.shouldUseTemporalHistory = isEnabled; }
    else if (key == "blur") { settings.shouldBlur = isEnabled; }
    else if (key == "hierarchicalDepth") { settings.shouldUseHierarchicalDepth = isEnabled; }
    else { return false; }
    return SetSettings(settings);
}
bool ScreenSpaceGlobalIllumination::SetDevelopmentNumber(const std::string& key, double value)
{
    if (!std::isfinite(value)) { return false; }
    auto settings = settings_;
    if (key == "strength") { settings.strength = static_cast<float>(value); }
    else if (key == "distance") { settings.maxDistance = static_cast<float>(value); }
    else if (key == "thickness") { settings.thickness = static_cast<float>(value); }
    else if (key == "historyWeight") { settings.historyWeight = static_cast<float>(value); }
    else if (key == "radianceLimit") { settings.maxRadiance = static_cast<float>(value); }
    else if (key == "rayCount" || key == "stepCount" || key == "view") {
        if (value < 0 || value > 64 || std::floor(value) != value) { return false; }
        if (key == "rayCount") { settings.rayCount = static_cast<uint32_t>(value); }
        else if (key == "stepCount") { settings.stepCount = static_cast<uint32_t>(value); }
        else { settings.debugMode = static_cast<ScreenSpaceGlobalIlluminationDebugMode>(static_cast<uint32_t>(value)); }
    }
    else { return false; }
    return SetSettings(settings);
}
bool ScreenSpaceGlobalIllumination::ExecuteDevelopmentCommand(const std::string& key)
{
    if (key != "resetHistory") { return false; }
    ResetHistory();
    return true;
}
#endif

void ScreenSpaceGlobalIllumination::ResizeSceneTargets() {
    if (textures_[8]->GetDesc().Width == SceneRenderResolution::GetWidth() && textures_[8]->GetDesc().Height == SceneRenderResolution::GetHeight()) { return; }
    ResetHistory();
    for (uint32_t index = 0; index < kTargetCount; ++index) {
        uint32_t width = SceneRenderResolution::GetWidth() / 2;
        uint32_t height = SceneRenderResolution::GetHeight() / 2;
        if (index == 8) { width = SceneRenderResolution::GetWidth(); height = SceneRenderResolution::GetHeight(); }
        CreateTarget(index, width, height);
    }
}
