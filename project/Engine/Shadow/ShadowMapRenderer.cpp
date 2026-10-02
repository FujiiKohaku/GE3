#include "ShadowMapRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <stdexcept>

namespace {
void CheckShadow(HRESULT result)
{
    if (FAILED(result)) { throw std::runtime_error("Shadow map resource/pipeline creation failed"); }
}
}

ShadowMapRenderer::~ShadowMapRenderer()
{
    if (srvIndex_ != 0xffffffffu) { SrvManager::GetInstance()->Free(srvIndex_); }
}

void ShadowMapRenderer::Initialize(DirectXCommon* dx, uint32_t resolution, ID3D12Resource* sharedDepth, uint32_t arraySlice)
{
    resolution_ = std::clamp(resolution, 256u, 4096u);
    dx_ = dx;
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = resolution_; desc.Height = resolution_;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear {};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    if (sharedDepth != nullptr) {
        depth_ = sharedDepth;
        depthSubresource_ = arraySlice;
    } else {
        CheckShadow(dx_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(&depth_)));
    }
    depth_->SetName(L"ShadowMap::Depth");
    dsvHeap_ = dx_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv {};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    if (sharedDepth != nullptr) {
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsv.Texture2DArray.FirstArraySlice = arraySlice;
        dsv.Texture2DArray.ArraySize = 1;
    }
    dx_->GetDevice()->CreateDepthStencilView(depth_.Get(), &dsv, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
    if (sharedDepth == nullptr) {
        srvIndex_ = SrvManager::GetInstance()->Allocate();
        SrvManager::GetInstance()->CreateSRVforTexture2D(srvIndex_, depth_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
    }
    constantsBuffer_ = dx_->CreateBufferResource(sizeof(ShadowConstants));
    CheckShadow(constantsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constants_)));
    *constants_ = {};

    D3D12_ROOT_PARAMETER parameters[3] {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.Num32BitValues = 16;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[1].Descriptor.ShaderRegister = 1;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants.ShaderRegister = 2;
    parameters[2].Constants.Num32BitValues = 4;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC signature {};
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    signature.NumParameters = 3; signature.pParameters = parameters;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    CheckShadow(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
    CheckShadow(dx_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_)));
    auto vertexShader = dx_->LoadCompiledShader(L"resources/Shaders/ShadowMap/Depth.VS.hlsl");
    D3D12_INPUT_ELEMENT_DESC position {};
    position.SemanticName = "POSITION";
    position.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso {};
    pso.pRootSignature = root_.Get();
    pso.InputLayout = { &position, 1 };
    pso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.DepthBias = 500;
    pso.RasterizerState.SlopeScaledDepthBias = 1.0f;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    CheckShadow(dx_->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_)));
    auto jellyfishShader = dx_->LoadCompiledShader(L"resources/Shaders/ShadowMap/JellyfishDepth.VS.hlsl");
    pso.VS = { jellyfishShader->GetBufferPointer(), jellyfishShader->GetBufferSize() };
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    CheckShadow(dx_->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&jellyfishPipeline_)));
}

void ShadowMapRenderer::Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings)
{
    shadowPassComplete_ = false;
    lightDirection_ = direction;
    ShadowSettings effective = settings;
    effective.resolution = resolution_;
    camera_.Update(camera, direction, effective);
    constants_->lightViewProjection = camera_.GetViewProjection();
    constants_->parameters = { 1.0f / resolution_, settings.depthBias, settings.normalBias, settings.strength };
    constants_->options = { 0.0f, settings.pcfRadius, 0.0f, 0.0f };
    if (settings.enabled) { constants_->options.x = 1.0f; }
}

void ShadowMapRenderer::BeginShadowPass()
{
    shadowPassComplete_ = false;
    auto* cmd = dx_->GetCommandList();
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(depth_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, depthSubresource_);
    cmd->ResourceBarrier(1, &barrier);
    auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    D3D12_VIEWPORT viewport { 0.0f, 0.0f, static_cast<float>(resolution_), static_cast<float>(resolution_), 0.0f, 1.0f };
    D3D12_RECT scissor { 0, 0, static_cast<LONG>(resolution_), static_cast<LONG>(resolution_) };
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);
    cmd->SetGraphicsRootSignature(root_.Get());
    cmd->SetPipelineState(pipeline_.Get());
    cmd->SetGraphicsRootConstantBufferView(1, constantsBuffer_->GetGPUVirtualAddress());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ShadowMapRenderer::BindObject(const Matrix4x4& world)
{
    dx_->GetCommandList()->SetPipelineState(pipeline_.Get());
    dx_->GetCommandList()->SetGraphicsRoot32BitConstants(0, 16, &world, 0);
}

void ShadowMapRenderer::BindJellyfishObject(const Matrix4x4& world, const Vector4& animation)
{
    auto* commandList = dx_->GetCommandList();
    commandList->SetPipelineState(jellyfishPipeline_.Get());
    commandList->SetGraphicsRoot32BitConstants(0, 16, &world, 0);
    commandList->SetGraphicsRoot32BitConstants(2, 4, &animation, 0);
}

void ShadowMapRenderer::EndShadowPass()
{
    shadowPassComplete_ = true;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(depth_.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, depthSubresource_);
    dx_->GetCommandList()->ResourceBarrier(1, &barrier);
}

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapRenderer::GetSrv() const
{
    if (srvIndex_ == 0xffffffffu) { return {}; }
    return SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
}

void ShadowMapRenderer::UpdatePerspective(const Vector3& position, const Vector3& direction, float distance, float fovY)
{
    shadowPassComplete_ = false;
    lightDirection_ = direction;
    camera_.UpdatePerspective(position, direction, distance, fovY);
    constants_->lightViewProjection = camera_.GetViewProjection();
    constants_->parameters = { 1.0f / resolution_, 0.00005f, 0.03f, 1.0f };
    constants_->options = { 1.0f, 1.0f, 0.0f, 0.0f };
}
