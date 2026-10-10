#include "AutoExposureRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include <cmath>
#include <stdexcept>
namespace {
void RequireExposure(HRESULT result) {
    if (FAILED(result)) { throw std::runtime_error("Auto exposure resource/pipeline creation failed"); }
}
}
AutoExposureRenderer::~AutoExposureRenderer() {
    for (uint32_t index : descriptorIndices_) { if (index != UINT_MAX) { SrvManager::GetInstance()->Free(index); } }
}
bool AutoExposureRenderer::SetSettings(const AutoExposureSettings& settings) {
    const float kValues[] = {settings.minExposure, settings.maxExposure, settings.middleGray,
        settings.lowPercentile, settings.highPercentile, settings.brightenSpeedPerSecond, settings.darkenSpeedPerSecond};
    for (float value : kValues) { if (!std::isfinite(value)) { return false; } }
    if (settings.minExposure < 0.0001f || settings.maxExposure > 128 || settings.maxExposure < settings.minExposure
        || settings.middleGray < 0.01f || settings.middleGray > 1 || settings.lowPercentile < 0
        || settings.highPercentile > 1 || settings.highPercentile <= settings.lowPercentile
        || settings.brightenSpeedPerSecond <= 0 || settings.brightenSpeedPerSecond > 20
        || settings.darkenSpeedPerSecond <= 0 || settings.darkenSpeedPerSecond > 20) { return false; }
    if (settings.isEnabled != settings_.isEnabled || settings.minExposure != settings_.minExposure
        || settings.maxExposure != settings_.maxExposure || settings.middleGray != settings_.middleGray
        || settings.lowPercentile != settings_.lowPercentile || settings.highPercentile != settings_.highPercentile) { ResetHistory(); }
    settings_ = settings;
    return true;
}
void AutoExposureRenderer::Initialize() {
    auto* dx = DirectXCommon::GetInstance(); auto* device = dx->GetDevice();
    D3D12_DESCRIPTOR_RANGE ranges[3] = {};
    for (uint32_t index = 0; index < 3; ++index) {
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].NumDescriptors = 1; ranges[index].BaseShaderRegister = index;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[2].BaseShaderRegister = 1;
    D3D12_ROOT_PARAMETER parameters[5] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &ranges[0]};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1, &ranges[1]};
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; parameters[2].Descriptor.ShaderRegister = 0;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[3].DescriptorTable = {1, &ranges[2]};
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; parameters[4].Descriptor.ShaderRegister = 0;
    D3D12_ROOT_SIGNATURE_DESC description = {}; description.NumParameters = 5; description.pParameters = parameters;
    Microsoft::WRL::ComPtr<ID3DBlob> signature; Microsoft::WRL::ComPtr<ID3DBlob> errors;
    RequireExposure(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors));
    RequireExposure(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)));
    const wchar_t* kShaderPaths[] = {L"resources/Shaders/PostEffect/Exposure/Clear.CS.hlsl",
        L"resources/Shaders/PostEffect/Exposure/Histogram.CS.hlsl", L"resources/Shaders/PostEffect/Exposure/Adapt.CS.hlsl"};
    for (uint32_t index = 0; index < 3; ++index) {
        auto shader = dx->LoadCompiledShader(kShaderPaths[index]);
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {}; pipeline.pRootSignature = rootSignature_.Get();
        pipeline.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
        RequireExposure(device->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&pipelines_[index])));
    }
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto buffer = CD3DX12_RESOURCE_DESC::Buffer(256 * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    RequireExposure(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&histogram_)));
    allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &buffer).SizeInBytes;
    auto* srv = SrvManager::GetInstance();
    for (uint32_t index = 0; index < 2; ++index) {
        auto texture = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        RequireExposure(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&textures_[index])));
        allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &texture).SizeInBytes;
        descriptorIndices_[index * 2] = srv->Allocate(); descriptorIndices_[index * 2 + 1] = srv->Allocate();
        srv->CreateSRVforTexture2D(descriptorIndices_[index * 2], textures_[index].Get(), DXGI_FORMAT_R32_FLOAT, 1);
        D3D12_UNORDERED_ACCESS_VIEW_DESC view = {}; view.Format = DXGI_FORMAT_R32_FLOAT; view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(textures_[index].Get(), nullptr, &view, srv->GetCPUDescriptorHandle(descriptorIndices_[index * 2 + 1]));
    }
    constantsResource_ = dx->CreateBufferResource(256);
    RequireExposure(constantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&constants_)));
    auto uploadDescription = constantsResource_->GetDesc();
    allocationBytes_ += device->GetResourceAllocationInfo(0, 1, &uploadDescription).SizeInBytes;
    timer_.Initialize(); isReady_ = true;
}
void AutoExposureRenderer::Generate(ID3D12Resource* sceneTexture, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, float deltaSeconds) {
    if (!settings_.isEnabled || sceneTexture == nullptr || sceneSrv.ptr == 0) { ResetHistory(); return; }
    if (!isReady_) { Initialize(); }
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) { deltaSeconds = 0; }
    if (deltaSeconds > 1) { deltaSeconds = 1; }
    auto description = sceneTexture->GetDesc();
    constants_->luminanceRange = {-12, 16, float(description.Width), float(description.Height)};
    constants_->exposureRange = {settings_.minExposure, settings_.maxExposure, settings_.middleGray, 0};
    constants_->adaptation = {deltaSeconds, settings_.brightenSpeedPerSecond, settings_.darkenSpeedPerSecond, 0};
    if (hasHistory_) { constants_->adaptation.w = 1; }
    constants_->percentiles = {settings_.lowPercentile, settings_.highPercentile, 0, 0};
    uint32_t writeIndex = 1 - currentIndex_;
    auto* dx = DirectXCommon::GetInstance(); auto* command = dx->GetCommandList(); auto* srv = SrvManager::GetInstance();
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(sceneTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[currentIndex_].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(textures_[writeIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    command->ResourceBarrier(3, barriers); srv->PreDraw(); timer_.Begin();
    command->SetComputeRootSignature(rootSignature_.Get());
    command->SetComputeRootDescriptorTable(0, sceneSrv);
    command->SetComputeRootDescriptorTable(1, srv->GetGPUDescriptorHandle(descriptorIndices_[currentIndex_ * 2]));
    command->SetComputeRootUnorderedAccessView(2, histogram_->GetGPUVirtualAddress());
    command->SetComputeRootDescriptorTable(3, srv->GetGPUDescriptorHandle(descriptorIndices_[writeIndex * 2 + 1]));
    command->SetComputeRootConstantBufferView(4, constantsResource_->GetGPUVirtualAddress());
    command->SetPipelineState(pipelines_[0].Get()); command->Dispatch(1, 1, 1);
    auto histogramBarrier = CD3DX12_RESOURCE_BARRIER::UAV(histogram_.Get()); command->ResourceBarrier(1, &histogramBarrier);
    command->SetPipelineState(pipelines_[1].Get()); command->Dispatch(UINT((description.Width + 15) / 16), (description.Height + 15) / 16, 1);
    command->ResourceBarrier(1, &histogramBarrier);
    command->SetPipelineState(pipelines_[2].Get()); command->Dispatch(1, 1, 1); timer_.End();
    for (auto& barrier : barriers) { std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); }
    command->ResourceBarrier(3, barriers); currentIndex_ = writeIndex; hasHistory_ = true;
}
ID3D12Resource* AutoExposureRenderer::GetExposureTexture() const {
    if (!isReady_ || !settings_.isEnabled || !hasHistory_) { return nullptr; }
    return textures_[currentIndex_].Get();
}
D3D12_GPU_DESCRIPTOR_HANDLE AutoExposureRenderer::GetExposureSrv() const {
    if (GetExposureTexture() == nullptr) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(descriptorIndices_[currentIndex_ * 2]);
}
