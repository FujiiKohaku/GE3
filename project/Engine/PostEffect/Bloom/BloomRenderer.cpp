#include "Engine/PostEffect/Bloom/BloomRenderer.h"

#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void BloomRenderer::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreatePipelineStates();
    CreateConstantBuffer();
    SetDefaultParameter();

    uint32_t bloomWidth = WinApp::kClientWidth / kBloomDownSample;
    if (bloomWidth == 0) {
        bloomWidth = 1;
    }

    uint32_t bloomHeight = WinApp::kClientHeight / kBloomDownSample;
    if (bloomHeight == 0) {
        bloomHeight = 1;
    }

    for (uint32_t index = 0; index < BloomRenderTargetCount; ++index) {
        renderTargets_[index].Initialize(
            dxCommon_,
            kFirstBloomRTVIndex + index,
            bloomWidth,
            bloomHeight,
            format_);
    }

    fullScreenViewport_.Width = static_cast<float>(WinApp::kClientWidth);
    fullScreenViewport_.Height = static_cast<float>(WinApp::kClientHeight);
    fullScreenViewport_.TopLeftX = 0.0f;
    fullScreenViewport_.TopLeftY = 0.0f;
    fullScreenViewport_.MinDepth = 0.0f;
    fullScreenViewport_.MaxDepth = 1.0f;

    fullScreenScissorRect_.left = 0;
    fullScreenScissorRect_.top = 0;
    fullScreenScissorRect_.right = WinApp::kClientWidth;
    fullScreenScissorRect_.bottom = WinApp::kClientHeight;
}

void BloomRenderer::Update()
{
    if (parameterData_ == nullptr) {
        return;
    }

    if (parameterData_->threshold < 0.0f) {
        parameterData_->threshold = 0.0f;
    }

    if (parameterData_->blurRadius < 0) {
        parameterData_->blurRadius = 0;
    }

    if (parameterData_->blurRadius > 32) {
        parameterData_->blurRadius = 32;
    }

    if (parameterData_->blurSigma < 0.01f) {
        parameterData_->blurSigma = 0.01f;
    }

    if (parameterData_->intensity < 0.0f) {
        parameterData_->intensity = 0.0f;
    }
}

void BloomRenderer::DrawImGui()
{
#ifdef USE_IMGUI
    if (parameterData_ == nullptr) {
        return;
    }

    ImGui::Begin("Bloom");

    bool isEnabled = false;
    if (parameterData_->isEnabled != 0) {
        isEnabled = true;
    }

    if (ImGui::Checkbox("Enable", &isEnabled)) {
        if (isEnabled) {
            parameterData_->isEnabled = 1;
        } else {
            parameterData_->isEnabled = 0;
        }
    }

    ImGui::SliderFloat("Threshold", &parameterData_->threshold, 0.0f, 5.0f);
    ImGui::SliderInt("Blur Radius", &parameterData_->blurRadius, 0, 32);
    ImGui::SliderFloat("Blur Sigma", &parameterData_->blurSigma, 0.01f, 20.0f);
    ImGui::SliderFloat("Intensity", &parameterData_->intensity, 0.0f, 5.0f);

    ImGui::End();
#endif
}

void BloomRenderer::Generate(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    if (parameterData_ == nullptr) {
        return;
    }

    ApplyBrightPass(sceneColorHandle);

    ApplyBlur(
        renderTargets_[BloomRenderTargetBrightPass].GetSrvHandleGPU(),
        renderTargets_[BloomRenderTargetBlurX],
        0);

    ApplyBlur(
        renderTargets_[BloomRenderTargetBlurX].GetSrvHandleGPU(),
        renderTargets_[BloomRenderTargetBlurY],
        1);
}

void BloomRenderer::Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    if (parameterData_ == nullptr) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &fullScreenViewport_);
    commandList->RSSetScissorRects(1, &fullScreenScissorRect_);

    ApplyComposite(
        sceneColorHandle,
        renderTargets_[BloomRenderTargetBlurY].GetSrvHandleGPU());
}

bool BloomRenderer::IsEnabled() const
{
    if (parameterData_ == nullptr) {
        return false;
    }

    if (parameterData_->isEnabled == 0) {
        return false;
    }

    return true;
}

void BloomRenderer::CreateRootSignature()
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
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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

void BloomRenderer::CreatePipelineStates()
{
    pipelineStates_[PipelineTypeBrightPass] =
        CreatePipelineState(L"resources/Shaders/PostEffect/Bloom/BloomExtract.PS.hlsl");
    pipelineStates_[PipelineTypeBlur] =
        CreatePipelineState(L"resources/Shaders/PostEffect/Bloom/BloomBlur.PS.hlsl");
    pipelineStates_[PipelineTypeComposite] =
        CreatePipelineState(L"resources/Shaders/PostEffect/Bloom/BloomComposite.PS.hlsl");
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> BloomRenderer::CreatePipelineState(const std::wstring& pixelShaderPath)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(pixelShaderPath, L"ps_6_0");

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

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
    pipelineStateDesc.RTVFormats[0] = format_;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = device->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));

    return pipelineState;
}

void BloomRenderer::CreateConstantBuffer()
{
    parameterResource_ = dxCommon_->CreateBufferResource(sizeof(BloomParameter));
    parameterResource_->SetName(L"BloomRenderer::BloomParameterCB");
    parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
}

void BloomRenderer::SetDefaultParameter()
{
    if (parameterData_ == nullptr) {
        return;
    }

    parameterData_->isEnabled = 1;
    parameterData_->threshold = 0.85f;
    parameterData_->blurRadius = 8;
    parameterData_->blurSigma = 4.0f;
    parameterData_->intensity = 0.75f;
    parameterData_->blurDirection = 0;
    parameterData_->padding0 = 0.0f;
    parameterData_->padding1 = 0.0f;
}

void BloomRenderer::DrawFullScreen(
    PipelineType pipelineType,
    D3D12_GPU_DESCRIPTOR_HANDLE firstTextureHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE secondTextureHandle)
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[pipelineType].Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootDescriptorTable(0, firstTextureHandle);
    commandList->SetGraphicsRootDescriptorTable(1, secondTextureHandle);
    commandList->SetGraphicsRootConstantBufferView(
        2,
        parameterResource_->GetGPUVirtualAddress());

    commandList->DrawInstanced(3, 1, 0, 0);
}

void BloomRenderer::ApplyBrightPass(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    RenderTarget& brightPassRenderTarget = renderTargets_[BloomRenderTargetBrightPass];

    brightPassRenderTarget.BeginRender();
    DrawFullScreen(
        PipelineTypeBrightPass,
        sceneColorHandle,
        sceneColorHandle);
    brightPassRenderTarget.EndRender();
}

void BloomRenderer::ApplyBlur(
    D3D12_GPU_DESCRIPTOR_HANDLE sourceHandle,
    RenderTarget& destination,
    int32_t blurDirection)
{
    parameterData_->blurDirection = blurDirection;

    destination.BeginRender();
    DrawFullScreen(
        PipelineTypeBlur,
        sourceHandle,
        sourceHandle);
    destination.EndRender();
}

void BloomRenderer::ApplyComposite(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE bloomColorHandle)
{
    DrawFullScreen(
        PipelineTypeComposite,
        sceneColorHandle,
        bloomColorHandle);
}

void BloomRenderer::RenderTarget::Initialize(
    DirectXCommon* dxCommon,
    uint32_t rtvIndex,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;
    width_ = width;
    height_ = height;
    format_ = format;

    CreateResource();
    CreateViews(rtvIndex);

    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = static_cast<LONG>(width_);
    scissorRect_.bottom = static_cast<LONG>(height_);
}

void BloomRenderer::RenderTarget::BeginRender()
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
}

void BloomRenderer::RenderTarget::EndRender()
{
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

D3D12_GPU_DESCRIPTOR_HANDLE BloomRenderer::RenderTarget::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

void BloomRenderer::RenderTarget::CreateResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width_;
    resourceDesc.Height = height_;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(result));

    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void BloomRenderer::RenderTarget::CreateViews(uint32_t rtvIndex)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    rtvHandle_ = dxCommon_->GetRTVHandle(rtvIndex);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);

    device->CreateShaderResourceView(
        resource_.Get(),
        &srvDesc,
        srvHandleCPU_);
}

void BloomRenderer::RenderTarget::Transition(D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}
