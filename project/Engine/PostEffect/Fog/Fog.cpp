#include "Engine/PostEffect/Fog/Fog.h"

#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include <cassert>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Fog::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreatePipelineState();
    CreateConstantBuffer();
    CreateDepthResource();
    CreateDepthDSV();
    CreateDepthSRV();
    CreateDebugReadbackResource();

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void Fog::Update()
{
    if (fogData_ == nullptr) {
        return;
    }

    if (fogData_->endDistance <= fogData_->startDistance) {
        fogData_->endDistance = fogData_->startDistance + 0.01f;
    }

    if (fogData_->startDistance < 0.0f) {
        fogData_->startDistance = 0.0f;
    }

    if (fogData_->curve < 0.01f) {
        fogData_->curve = 0.01f;
    }

    if (fogData_->maxFog < 0.0f) {
        fogData_->maxFog = 0.0f;
    }

    if (fogData_->maxFog > 1.0f) {
        fogData_->maxFog = 1.0f;
    }

    if (fogData_->nearClip < 0.001f) {
        fogData_->nearClip = 0.001f;
    }

    if (fogData_->farClip <= fogData_->nearClip) {
        fogData_->farClip = fogData_->nearClip + 0.001f;
    }

    if (fogData_->fovY < 0.001f) {
        fogData_->fovY = 0.001f;
    }

    if (fogData_->aspectRatio < 0.001f) {
        fogData_->aspectRatio = 0.001f;
    }
}

void Fog::Apply(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    if (fogData_ == nullptr) {
        return;
    }

    if (fogData_->isEnabled == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    CopyDebugDepth();

    commandList->SetGraphicsRootDescriptorTable(0, sceneColorHandle);
    commandList->SetGraphicsRootDescriptorTable(1, depthSRVHandleGPU_);
    commandList->SetGraphicsRootConstantBufferView(
        2,
        constantBuffer_->GetGPUVirtualAddress());

    commandList->DrawInstanced(3, 1, 0, 0);
}

void Fog::DrawImGui()
{
#ifdef USE_IMGUI
    if (fogData_ == nullptr) {
        return;
    }

    UpdateDebugInfo();

    ImGui::Begin("Fog");

    bool isEnabled = false;
    if (fogData_->isEnabled != 0) {
        isEnabled = true;
    }

    if (ImGui::Checkbox("Enable Fog", &isEnabled)) {
        if (isEnabled) {
            fogData_->isEnabled = 1;
        } else {
            fogData_->isEnabled = 0;
        }
    }

    ImGui::ColorEdit3("Fog Color", &fogData_->color.x);
    ImGui::DragFloat("Fog Start", &fogData_->startDistance, 0.1f, 0.0f, 3000.0f);
    ImGui::DragFloat("Fog End", &fogData_->endDistance, 0.1f, 0.01f, 3000.0f);
    ImGui::DragFloat("Fog Curve", &fogData_->curve, 0.01f, 0.01f, 8.0f);
    ImGui::DragFloat("Max Fog", &fogData_->maxFog, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Debug Center Pixel", &isDebugReadbackEnabled_);
    if (isDebugReadbackEnabled_) {
        ImGui::Text("Depth: %.6f", debugInfo_.depth);
        ImGui::Text("LinearDepth: %.3f", debugInfo_.linearDepth);
        ImGui::Text("ViewDistance: %.3f", debugInfo_.viewDistance);
        ImGui::Text("FogFactor: %.3f", debugInfo_.fogFactor);
    }

    ImGui::End();
#endif
}

void Fog::SetClipRange(float nearClip, float farClip)
{
    if (fogData_ == nullptr) {
        return;
    }

    if (nearClip < 0.001f) {
        nearClip = 0.001f;
    }

    if (farClip <= nearClip) {
        farClip = nearClip + 0.001f;
    }

    fogData_->nearClip = nearClip;
    fogData_->farClip = farClip;
}

void Fog::SetCameraInfo(float nearClip, float farClip, float fovY, float aspectRatio)
{
    SetClipRange(nearClip, farClip);

    if (fogData_ == nullptr) {
        return;
    }

    if (fovY < 0.001f) {
        fovY = 0.001f;
    }

    if (aspectRatio < 0.001f) {
        aspectRatio = 0.001f;
    }

    fogData_->fovY = fovY;
    fogData_->aspectRatio = aspectRatio;
}

void Fog::PreDrawDepth()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    if (depthResourceState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = depthResource_.Get();
        barrier.Transition.StateBefore = depthResourceState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &barrier);

        depthResourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    commandList->ClearDepthStencilView(
        depthDSVHandle_,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
}

void Fog::PostDrawDepth()
{
    if (depthResourceState_ == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthResource_.Get();
    barrier.Transition.StateBefore = depthResourceState_;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    depthResourceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

D3D12_CPU_DESCRIPTOR_HANDLE Fog::GetDepthDSVHandle() const
{
    return depthDSVHandle_;
}

D3D12_GPU_DESCRIPTOR_HANDLE Fog::GetDepthSRVHandle() const
{
    return depthSRVHandleGPU_;
}

void Fog::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE descriptorRanges[2] = {};

    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[1].BaseShaderRegister = 1;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(result));

    result = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
}

void Fog::CreatePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/Fog.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/Fog.PS.hlsl", L"ps_6_0");

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
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
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT result = device->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(result));
}

void Fog::CreateConstantBuffer()
{
    constantBuffer_ = dxCommon_->CreateBufferResource(sizeof(FogData));
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));

    fogData_->isEnabled = 1;
    fogData_->startDistance = 120.0f;
    fogData_->endDistance = 180.0f;
    fogData_->curve = 1.2f;
    fogData_->color = { 0.65f, 0.75f, 0.85f };
    fogData_->maxFog = 1.0f;
    fogData_->nearClip = 0.1f;
    fogData_->farClip = 1000.0f;
    fogData_->fovY = 0.45f;
    fogData_->aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
}

void Fog::CreateDepthResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC depthResourceDesc = {};
    depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthResourceDesc.Width = WinApp::kClientWidth;
    depthResourceDesc.Height = WinApp::kClientHeight;
    depthResourceDesc.DepthOrArraySize = 1;
    depthResourceDesc.MipLevels = 1;
    depthResourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthResourceDesc.SampleDesc.Count = 1;
    depthResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthResource_));
    assert(SUCCEEDED(result));
}

void Fog::CreateDepthDSV()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    depthDSVHandle_ = dxCommon_->GetDSVHandle(2);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device->CreateDepthStencilView(
        depthResource_.Get(),
        &dsvDesc,
        depthDSVHandle_);
}

void Fog::CreateDepthSRV()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
    depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSRVDesc.Texture2D.MipLevels = 1;

    depthSRVIndex_ = SrvManager::GetInstance()->Allocate();
    depthSRVHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSRVIndex_);
    depthSRVHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSRVIndex_);

    device->CreateShaderResourceView(
        depthResource_.Get(),
        &depthSRVDesc,
        depthSRVHandleCPU_);
}

void Fog::CreateDebugReadbackResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC depthResourceDesc = depthResource_->GetDesc();
    device->GetCopyableFootprints(
        &depthResourceDesc,
        0,
        1,
        0,
        &debugReadbackLayout_,
        nullptr,
        nullptr,
        &debugReadbackSize_);

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackResourceDesc = {};
    readbackResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackResourceDesc.Width = debugReadbackSize_;
    readbackResourceDesc.Height = 1;
    readbackResourceDesc.DepthOrArraySize = 1;
    readbackResourceDesc.MipLevels = 1;
    readbackResourceDesc.SampleDesc.Count = 1;
    readbackResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &readbackResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&debugReadbackResource_));
    assert(SUCCEEDED(result));
}

void Fog::CopyDebugDepth()
{
    if (!isDebugReadbackEnabled_) {
        return;
    }

    if (debugReadbackResource_ == nullptr) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    if (depthResourceState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = depthResource_.Get();
        barrier.Transition.StateBefore = depthResourceState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &barrier);
        depthResourceState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }

    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = debugReadbackResource_.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = debugReadbackLayout_;

    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = depthResource_.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;

    commandList->CopyTextureRegion(
        &destination,
        0,
        0,
        0,
        &source,
        nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthResource_.Get();
    barrier.Transition.StateBefore = depthResourceState_;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);
    depthResourceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void Fog::UpdateDebugInfo()
{
    if (!isDebugReadbackEnabled_) {
        return;
    }

    if (debugReadbackResource_ == nullptr) {
        return;
    }

    uint8_t* mappedData = nullptr;
    HRESULT result = debugReadbackResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(result)) {
        return;
    }

    uint32_t centerX = WinApp::kClientWidth / 2;
    uint32_t centerY = WinApp::kClientHeight / 2;
    uint64_t offset = debugReadbackLayout_.Offset;
    offset += static_cast<uint64_t>(debugReadbackLayout_.Footprint.RowPitch) * centerY;
    offset += static_cast<uint64_t>(sizeof(uint32_t)) * centerX;

    uint32_t packedDepth = *reinterpret_cast<uint32_t*>(mappedData + offset);
    uint32_t depthBits = packedDepth & 0x00ffffff;
    float depth = static_cast<float>(depthBits) / 16777215.0f;
    debugReadbackResource_->Unmap(0, nullptr);

    debugInfo_.depth = depth;
    debugInfo_.linearDepth = RestoreViewZ(depth);
    debugInfo_.viewDistance = RestoreCenterViewDistance(depth);
    debugInfo_.fogFactor = CalculateFogFactor(debugInfo_.viewDistance);
}

float Fog::RestoreViewZ(float depth) const
{
    if (fogData_ == nullptr) {
        return 0.0f;
    }

    float nearClip = fogData_->nearClip;
    float farClip = fogData_->farClip;

    if (nearClip < 0.0001f) {
        nearClip = 0.0001f;
    }

    if (farClip <= nearClip) {
        farClip = nearClip + 0.0001f;
    }

    float denominator = farClip - depth * (farClip - nearClip);
    if (denominator < 0.0001f) {
        denominator = 0.0001f;
    }

    return (nearClip * farClip) / denominator;
}

float Fog::RestoreCenterViewDistance(float depth) const
{
    return RestoreViewZ(depth);
}

float Fog::CalculateFogFactor(float viewDistance) const
{
    if (fogData_ == nullptr) {
        return 0.0f;
    }

    float fogRange = fogData_->endDistance - fogData_->startDistance;
    if (fogRange < 0.0001f) {
        fogRange = 0.0001f;
    }

    float distanceFactor = (viewDistance - fogData_->startDistance) / fogRange;
    if (distanceFactor < 0.0f) {
        distanceFactor = 0.0f;
    }

    if (distanceFactor > 1.0f) {
        distanceFactor = 1.0f;
    }

    float curve = fogData_->curve;
    if (curve < 0.01f) {
        curve = 0.01f;
    }

    float maxFog = fogData_->maxFog;
    if (maxFog < 0.0f) {
        maxFog = 0.0f;
    }

    if (maxFog > 1.0f) {
        maxFog = 1.0f;
    }

    float fogFactor = std::pow(distanceFactor, curve) * maxFog;
    if (fogFactor > 1.0f) {
        fogFactor = 1.0f;
    }

    return fogFactor;
}
