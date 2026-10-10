#include "Engine/Renderer/SceneRenderResolution.h"
#include "Engine/PostEffect/Fog/FogRenderer.h"

#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include <cassert>

void FogRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreatePipelineState();
    CreateDepthResource();
    CreateDepthDSV();
    CreateDepthSRV();

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

void FogRenderer::PreDrawDepth()
{
    shouldUseParticleDepth_ = false;
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    TransitionDepthResource(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList->ClearDepthStencilView(
        depthDSVHandle_,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
}

void FogRenderer::PostDrawDepth()
{
    TransitionDepthResource(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void FogRenderer::PrepareDepthForParticleDraw()
{
    TransitionDepthResource(D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void FogRenderer::Apply(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
    D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView)
{
    if (fogConstantBufferView == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    auto viewport = viewport_; auto scissor = scissorRect_;
    if (shouldUseParticleDepth_) {
        viewport.Width = float(WinApp::kClientWidth); viewport.Height = float(WinApp::kClientHeight);
        scissor.right = WinApp::kClientWidth; scissor.bottom = WinApp::kClientHeight;
    }
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootDescriptorTable(0, sceneColorHandle);
    auto depthSrv = depthSRVHandleGPU_;
    if (shouldUseParticleDepth_) { depthSrv = GetParticleDepthSrv(); }
    commandList->SetGraphicsRootDescriptorTable(1, depthSrv);
    commandList->SetGraphicsRootConstantBufferView(2, fogConstantBufferView);

    commandList->DrawInstanced(3, 1, 0, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE FogRenderer::GetDepthDSVHandle() const
{
    return depthDSVHandle_;
}

D3D12_GPU_DESCRIPTOR_HANDLE FogRenderer::GetDepthSRVHandle() const
{
    return depthSRVHandleGPU_;
}

void FogRenderer::CreateRootSignature()
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

void FogRenderer::CreatePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fog/Fog.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fog/DistanceFog.PS.hlsl");

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
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT result = device->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(result));
}

void FogRenderer::CreateDepthResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC depthResourceDesc = {};
    depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthResourceDesc.Width = sceneWidth_;
    depthResourceDesc.Height = sceneHeight_;
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

void FogRenderer::CreateDepthDSV()
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

void FogRenderer::CreateDepthSRV()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
    depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSRVDesc.Texture2D.MipLevels = 1;

    if (depthSRVIndex_ == UINT_MAX) { depthSRVIndex_ = SrvManager::GetInstance()->Allocate(); }
    depthSRVHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(depthSRVIndex_);
    depthSRVHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(depthSRVIndex_);

    device->CreateShaderResourceView(
        depthResource_.Get(),
        &depthSRVDesc,
        depthSRVHandleCPU_);
}

void FogRenderer::TransitionDepthResource(D3D12_RESOURCE_STATES nextState)
{
    if (depthResourceState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthResource_.Get();
    barrier.Transition.StateBefore = depthResourceState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    depthResourceState_ = nextState;
}

FogRenderer::~FogRenderer() {
    if (depthSRVIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(depthSRVIndex_); }
    if (particleDepthSrvIndex_ != UINT_MAX) { SrvManager::GetInstance()->Free(particleDepthSrvIndex_); }
}
void FogRenderer::ResizeSceneTargets() {
    if (sceneWidth_ == SceneRenderResolution::GetWidth() && sceneHeight_ == SceneRenderResolution::GetHeight()) { return; }
    sceneWidth_ = SceneRenderResolution::GetWidth(); sceneHeight_ = SceneRenderResolution::GetHeight();
    CreateDepthResource(); depthResourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    CreateDepthDSV(); CreateDepthSRV();
    viewport_.Width = float(sceneWidth_); viewport_.Height = float(sceneHeight_);
    scissorRect_.right = sceneWidth_; scissorRect_.bottom = sceneHeight_;
}
D3D12_CPU_DESCRIPTOR_HANDLE FogRenderer::GetParticleDepthDsv() const {
    if (sceneWidth_ == WinApp::kClientWidth) { return depthDSVHandle_; }
    return particleDepthHeap_->GetCPUDescriptorHandleForHeapStart();
}
D3D12_GPU_DESCRIPTOR_HANDLE FogRenderer::GetParticleDepthSrv() const {
    if (sceneWidth_ == WinApp::kClientWidth) { return depthSRVHandleGPU_; }
    if (particleDepthSrvIndex_ == UINT_MAX) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(particleDepthSrvIndex_);
}
ID3D12Resource* FogRenderer::GetParticleDepthTexture() const {
    if (sceneWidth_ == WinApp::kClientWidth) { return depthResource_.Get(); }
    return particleDepth_.Get();
}
void FogRenderer::ResolveParticleDepth(Vector2 jitterUv) {
    shouldUseParticleDepth_ = true;
    if (sceneWidth_ == WinApp::kClientWidth) { PrepareDepthForParticleDraw(); return; }
    auto* device = dxCommon_->GetDevice(); auto* command = dxCommon_->GetCommandList();
    if (!particleDepth_) {
        auto description = depthResource_->GetDesc(); description.Width = WinApp::kClientWidth; description.Height = WinApp::kClientHeight;
        D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_CLEAR_VALUE clear = {}; clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; clear.DepthStencil.Depth = 1;
        HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&particleDepth_)); assert(SUCCEEDED(result));
        particleDepthHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {}; dsv.Format = clear.Format; dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(particleDepth_.Get(), &dsv, particleDepthHeap_->GetCPUDescriptorHandleForHeapStart());
        particleDepthSrvIndex_ = SrvManager::GetInstance()->Allocate();
        SrvManager::GetInstance()->CreateSRVforTexture2D(particleDepthSrvIndex_, particleDepth_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
        depthResolveConstants_ = dxCommon_->CreateBufferResource(256);
        auto vertex = dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
        auto pixel = dxCommon_->LoadCompiledShader(L"resources/Shaders/SuperResolution/DepthResolve.PS.hlsl");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {}; pipeline.pRootSignature = rootSignature_.Get();
        pipeline.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()}; pipeline.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
        pipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pipeline.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; pipeline.SampleDesc.Count = 1; pipeline.SampleMask = UINT_MAX;
        pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        result = device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&depthResolvePipeline_)); assert(SUCCEEDED(result));
    }
    if (particleDepthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(particleDepth_.Get(), particleDepthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command->ResourceBarrier(1, &barrier); particleDepthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    Vector4* constants = nullptr; depthResolveConstants_->Map(0, nullptr, reinterpret_cast<void**>(&constants));
    *constants = {jitterUv.x, jitterUv.y, 0, 0}; depthResolveConstants_->Unmap(0, nullptr);
    auto dsv = GetParticleDepthDsv(); command->OMSetRenderTargets(0, nullptr, false, &dsv);
    D3D12_VIEWPORT viewport = {0, 0, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0, 1};
    D3D12_RECT scissor = {0, 0, WinApp::kClientWidth, WinApp::kClientHeight};
    command->RSSetViewports(1, &viewport); command->RSSetScissorRects(1, &scissor);
    command->SetGraphicsRootSignature(rootSignature_.Get()); command->SetPipelineState(depthResolvePipeline_.Get());
    command->SetGraphicsRootDescriptorTable(0, depthSRVHandleGPU_); command->SetGraphicsRootDescriptorTable(1, depthSRVHandleGPU_);
    command->SetGraphicsRootConstantBufferView(2, depthResolveConstants_->GetGPUVirtualAddress());
    command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); command->DrawInstanced(3, 1, 0, 0);
}
void FogRenderer::FinishParticleDepth() {
    if (sceneWidth_ == WinApp::kClientWidth) { PostDrawDepth(); return; }
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(particleDepth_.Get(), particleDepthState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier); particleDepthState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}
