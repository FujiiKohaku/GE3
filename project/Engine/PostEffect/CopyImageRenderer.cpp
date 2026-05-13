#include "CopyImageRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"

#include <cassert>

void CopyImageRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreatePostEffectParameterResource();
    pipelineStates_[PostEffectType::Copy] = CreateGraphicsPipeline(L"resources/shaders/Fullscreen.PS.hlsl"); // Copy用のシェーダーは、単純にテクスチャを描画するだけのものを用意していると仮定しています。

    pipelineStates_[PostEffectType::GrayScale] = CreateGraphicsPipeline(L"resources/shaders/GrayScale.PS.hlsl"); // GrayScale用のシェーダーは、テクスチャをグレースケールで描画するものを用意していると仮定しています。

    pipelineStates_[PostEffectType::Vignette] = CreateGraphicsPipeline(L"resources/shaders/Vignette.PS.hlsl"); // Vignette用のシェーダーは、テクスチャにビネット効果を適用して描画するものを用意していると仮定しています。

    pipelineStates_[PostEffectType::smoothing] = CreateGraphicsPipeline(L"resources/shaders/BoxFilter.PS.hlsl"); // smoothing用のシェーダーは、テクスチャに単純なボックスフィルタを適用して描画するものを用意していると仮定しています。

    pipelineStates_[PostEffectType::GaussianFilter] = CreateGraphicsPipeline(L"resources/shaders/GaussianFilter.PS.hlsl"); // GaussianFilter用のシェーダーは、テクスチャにガウシアンフィルタを適用して描画するものを用意していると仮定しています。
    pipelineStates_[PostEffectType::LuminanceBasedOutline] = CreateGraphicsPipeline(L"resources/shaders/LuminanceBasedOutline.PS.hlsl"); // LuminanceBasedOutline用のシェーダーは、テクスチャの輝度に基づいて輪郭を描画するものを用意していると仮定しています。

    pipelineStates_[PostEffectType::DepthOutline] = CreateGraphicsPipeline(L"resources/shaders/DepthBasedOutline.PS.hlsl");
}

void CopyImageRenderer::CreateRootSignature()
{
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

    D3D12_DESCRIPTOR_RANGE descriptorRange[2] = {};

    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRange[1].BaseShaderRegister = 1;
    descriptorRange[1].NumDescriptors = 1;
    descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter[3] = {};

    rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
    rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter[1].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
    rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;

    rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter[2].Descriptor.ShaderRegister = 0;
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
    rootSignatureDesc.pParameters = rootParameter;
    rootSignatureDesc.NumParameters = 3;
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    assert(SUCCEEDED(hr));

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));

    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> CopyImageRenderer::CreateGraphicsPipeline(const std::wstring& pixelShaderPath)
{
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(pixelShaderPath, L"ps_6_0");

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();

    pipelineStateDesc.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
    pipelineStateDesc.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();

    pipelineStateDesc.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
    pipelineStateDesc.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();

    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.RasterizerState = rasterizerDesc;

    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;

    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = false;

    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;

    HRESULT hr = device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));

    assert(SUCCEEDED(hr));

    return graphicsPipelineState;
}

void CopyImageRenderer::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle)
{
    ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = pipelineStates_[currentPostEffectType_];

    commandList->SetPipelineState(pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, textureHandle);
    commandList->SetGraphicsRootDescriptorTable(1, depthTextureHandle);
    commandList->SetGraphicsRootConstantBufferView(2, postEffectParameterResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(3, 1, 0, 0);
}
void CopyImageRenderer::SetPostEffectType(PostEffectType postEffectType)
{
    currentPostEffectType_ = postEffectType;
}
void CopyImageRenderer::CreatePostEffectParameterResource()
{
    postEffectParameterResource_ = dxCommon_->CreateBufferResource(sizeof(PostEffectParameter));

    postEffectParameterResource_->Map(0,nullptr,reinterpret_cast<void**>(&postEffectParameterData_));

    postEffectParameterData_->grayScaleStrength = 1.0f;
    postEffectParameterData_->vignetteStrength = 1.0f;
    postEffectParameterData_->outlineScale = 1000.0f;
    postEffectParameterData_->time = 0.0f;
}