#include "TemporalSuperResolution.h"
#include "Engine/Camera/Camera.h"
#include "Engine/SrvManager/SrvManager.h"
#include <cmath>
#include <stdexcept>
namespace {
void RequireTemporal(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Temporal resolution initialization failed"); }
}
float HaltonTemporal(uint32_t value, uint32_t base) {
    float result = 0; float fraction = 1;
    while (value > 0) { fraction /= float(base); result += fraction * float(value % base); value /= base; }
    return result;
}
bool HasTemporalSize(ID3D12Resource* resource, const TemporalResolutionSettings& settings) {
    if (resource == nullptr) { return false; }
    auto description = resource->GetDesc();
    return description.Width == settings.inputWidth && description.Height == settings.inputHeight
        && description.SampleDesc.Count == 1;
}
}
TemporalSuperResolution::~TemporalSuperResolution() {
    for (uint32_t index : descriptorIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
}
bool TemporalSuperResolution::SetSettings(const TemporalResolutionSettings& settings) {
    if (settings.inputWidth < 16 || settings.inputHeight < 9 || settings.outputWidth > 3840 || settings.outputHeight > 2160
        || settings.outputWidth < settings.inputWidth || settings.outputHeight < settings.inputHeight
        || uint64_t(settings.inputWidth) * settings.outputHeight != uint64_t(settings.inputHeight) * settings.outputWidth
        || !std::isfinite(settings.historyWeight) || settings.historyWeight < 0 || settings.historyWeight > 0.95f
        || !std::isfinite(settings.varianceGamma) || settings.varianceGamma < 0.5f || settings.varianceGamma > 3) { return false; }
    if (settings.outputWidth != settings_.outputWidth || settings.outputHeight != settings_.outputHeight) { shouldRecreateResources_ = true; }
    if (settings.isEnabled != settings_.isEnabled || settings.shouldUseBicubic != settings_.shouldUseBicubic
        || settings.inputWidth != settings_.inputWidth || settings.inputHeight != settings_.inputHeight
        || settings.outputWidth != settings_.outputWidth || settings.outputHeight != settings_.outputHeight
        || settings.historyWeight != settings_.historyWeight || settings.varianceGamma != settings_.varianceGamma) { ResetHistory(); }
    settings_ = settings;
    if (!settings_.isEnabled) { isActive_ = false; jitterPixels_ = {}; timer_.ResetSample(); }
    return true;
}
void TemporalSuperResolution::BeginFrame(const SuperResolutionHistoryInputs& inputs) {
    if (!settings_.isEnabled || !inputs.hasCamera) { isActive_ = false; ResetHistory(); jitterPixels_ = {}; return; }
    if (!isActive_ || inputs.cameraId != historyInputs_.cameraId || inputs.cameraHistoryId != historyInputs_.cameraHistoryId
        || inputs.sceneRevision != historyInputs_.sceneRevision || inputs.radianceRevision != historyInputs_.radianceRevision) {
        ResetHistory(); frameIndex_ = 0;
    }
    historyInputs_ = inputs; isActive_ = true;
    uint32_t sample = frameIndex_++ % 32 + 1;
    jitterPixels_ = {HaltonTemporal(sample, 2) - 0.5f, HaltonTemporal(sample, 3) - 0.5f};
}
Vector2 TemporalSuperResolution::GetProjectionJitterNdc() const {
    if (!isActive_) { return {}; }
    return {2 * jitterPixels_.x / float(settings_.inputWidth), -2 * jitterPixels_.y / float(settings_.inputHeight)};
}
void TemporalSuperResolution::Initialize() {
    auto* dx = DirectXCommon::GetInstance(); auto* device = dx->GetDevice();
    D3D12_DESCRIPTOR_RANGE ranges[8] = {}; D3D12_ROOT_PARAMETER parameters[9] = {};
    for (uint32_t index = 0; index < 8; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[index].BaseShaderRegister = index;
        if (index >= 6) { ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[index].BaseShaderRegister = index - 6; }
        ranges[index].NumDescriptors = 1; ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index].DescriptorTable = {1, &ranges[index]};
    }
    parameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    D3D12_STATIC_SAMPLER_DESC sampler = {}; sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; sampler.AddressV = sampler.AddressU; sampler.AddressW = sampler.AddressU;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    D3D12_ROOT_SIGNATURE_DESC description = {}; description.NumParameters = 9; description.pParameters = parameters;
    description.NumStaticSamplers = 1; description.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> signature; Microsoft::WRL::ComPtr<ID3DBlob> errors;
    RequireTemporal(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors));
    RequireTemporal(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_)));
    auto shader = dx->LoadCompiledShader(L"resources/Shaders/SuperResolution/Temporal.CS.hlsl");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {}; pipeline.pRootSignature = root_.Get();
    pipeline.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
    RequireTemporal(device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&pipeline_)));
    constantsResource_ = dx->CreateBufferResource(512);
    RequireTemporal(constantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&constants_)));
    timer_.Initialize(); isReady_ = true;
}
void TemporalSuperResolution::CreateResources() {
    auto* dx = DirectXCommon::GetInstance(); auto* device = dx->GetDevice(); auto* srv = SrvManager::GetInstance();
    for (auto& index : descriptorIndices_) { if (index != UINT_MAX) { srv->Free(index); index = UINT_MAX; } }
    allocationBytes_ = 0; currentIndex_ = 0; ResetHistory();
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    for (uint32_t index = 0; index < 4; ++index) {
        DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (index >= 2) { format = DXGI_FORMAT_R32G32_FLOAT; }
        auto texture = CD3DX12_RESOURCE_DESC::Tex2D(format, settings_.outputWidth, settings_.outputHeight,
            1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        RequireTemporal(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index])));
        allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &texture).SizeInBytes;
        descriptorIndices_[index * 2] = srv->Allocate(); descriptorIndices_[index * 2 + 1] = srv->Allocate();
        srv->CreateSRVforTexture2D(descriptorIndices_[index * 2], textures_[index].Get(), format, 1);
        D3D12_UNORDERED_ACCESS_VIEW_DESC view = {}; view.Format = format; view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(textures_[index].Get(), nullptr, &view, srv->GetCPUDescriptorHandle(descriptorIndices_[index * 2 + 1]));
    }
    shouldRecreateResources_ = false;
}
D3D12_GPU_DESCRIPTOR_HANDLE TemporalSuperResolution::Evaluate(const TemporalResolutionFrameInputs& inputs) {
    hasUsedHistory_ = false;
    if (!isActive_) { return inputs.scene.colorSrv; }
    if (!HasTemporalSize(inputs.scene.colorTexture, settings_) || !HasTemporalSize(inputs.scene.depthTexture, settings_)
        || !HasTemporalSize(inputs.scene.motionVectorTexture, settings_) || inputs.scene.colorSrv.ptr == 0
        || inputs.depthSrv.ptr == 0 || inputs.motionSrv.ptr == 0 || inputs.camera == nullptr) {
        ResetHistory(); return inputs.scene.colorSrv;
    }
    if (inputs.reprojectionSrv.ptr != 0 && !HasTemporalSize(inputs.reprojectionTexture, settings_)) {
        ResetHistory(); return inputs.scene.colorSrv;
    }
    if (!isReady_) { Initialize(); }
    if (shouldRecreateResources_) { CreateResources(); }
    constants_->inverseViewProjection = MatrixMath::Inverse(inputs.camera->GetViewProjectionMatrix());
    constants_->view = inputs.camera->GetViewMatrix(); constants_->previousViewProjection = previousViewProjection_;
    constants_->previousView = previousView_; auto position = inputs.camera->GetTranslate();
    constants_->cameraPosition = {position.x, position.y, position.z, 1};
    constants_->inputSize = {float(settings_.inputWidth), float(settings_.inputHeight), 1.0f / settings_.inputWidth, 1.0f / settings_.inputHeight};
    constants_->outputSize = {float(settings_.outputWidth), float(settings_.outputHeight), 1.0f / settings_.outputWidth, 1.0f / settings_.outputHeight};
    constants_->controls = {0, settings_.historyWeight, settings_.varianceGamma, 0};
    if (hasHistory_) { constants_->controls.x = 1; hasUsedHistory_ = true; }
    if (inputs.reprojectionSrv.ptr != 0) { constants_->controls.w = 1; }
    constants_->jitter = {jitterPixels_.x / settings_.inputWidth, jitterPixels_.y / settings_.inputHeight, 0, 0};
    if (settings_.shouldUseBicubic) { constants_->jitter.z = 1; }
    auto* dx = DirectXCommon::GetInstance(); auto* command = dx->GetCommandList(); auto* srv = SrvManager::GetInstance();
    uint32_t writeIndex = 1 - currentIndex_;
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[currentIndex_].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[currentIndex_ + 2].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[writeIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[writeIndex + 2].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    command->ResourceBarrier(4, barriers);
    // Borrowed inputs can also be used by graphics; retain their PIXEL state.
    constexpr auto kReadState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    ID3D12Resource* sources[] = {inputs.scene.colorTexture, inputs.scene.depthTexture, inputs.scene.motionVectorTexture, inputs.reprojectionTexture};
    for (auto* source : sources) {
        if (source == nullptr) { continue; }
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, kReadState);
        command->ResourceBarrier(1, &barrier);
    }
    srv->PreDraw(); timer_.Begin(); command->SetComputeRootSignature(root_.Get()); command->SetPipelineState(pipeline_.Get());
    D3D12_GPU_DESCRIPTOR_HANDLE handles[] = {inputs.scene.colorSrv, inputs.depthSrv, inputs.motionSrv,
        inputs.reprojectionSrv, srv->GetGPUDescriptorHandle(descriptorIndices_[currentIndex_ * 2]),
        srv->GetGPUDescriptorHandle(descriptorIndices_[(currentIndex_ + 2) * 2]),
        srv->GetGPUDescriptorHandle(descriptorIndices_[writeIndex * 2 + 1]),
        srv->GetGPUDescriptorHandle(descriptorIndices_[(writeIndex + 2) * 2 + 1])};
    if (handles[3].ptr == 0) { handles[3] = inputs.scene.colorSrv; }
    for (uint32_t index = 0; index < 8; ++index) { command->SetComputeRootDescriptorTable(index, handles[index]); }
    command->SetComputeRootConstantBufferView(8, constantsResource_->GetGPUVirtualAddress());
    command->Dispatch((settings_.outputWidth + 7) / 8, (settings_.outputHeight + 7) / 8, 1); timer_.End();
    for (auto& barrier : barriers) { std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); }
    command->ResourceBarrier(4, barriers);
    for (auto* source : sources) {
        if (source == nullptr) { continue; }
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, kReadState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command->ResourceBarrier(1, &barrier);
    }
    currentIndex_ = writeIndex; hasHistory_ = true;
    previousViewProjection_ = inputs.camera->GetUnjitteredViewProjectionMatrix(); previousView_ = inputs.camera->GetViewMatrix();
    return srv->GetGPUDescriptorHandle(descriptorIndices_[currentIndex_ * 2]);
}
ID3D12Resource* TemporalSuperResolution::GetOutputTexture() const {
    if (!isActive_ || !hasHistory_) { return nullptr; }
    return textures_[currentIndex_].Get();
}
