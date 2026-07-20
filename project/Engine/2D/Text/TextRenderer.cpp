#include "TextRenderer.h"

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/blend/BlendUtil.h"
#include "Engine/StringUtility/StringUtility.h"
#include <cassert>

std::unique_ptr<TextRenderer> TextRenderer::instance_ = nullptr;

TextRenderer* TextRenderer::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<TextRenderer>(ConstructorKey());
    }
    return instance_.get();
}

void TextRenderer::Finalize()
{
    instance_.reset();
}

TextRenderer::TextRenderer(ConstructorKey)
{
}

void TextRenderer::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreateGraphicsPipeline();
}

void TextRenderer::PreDraw()
{
    assert(dxCommon_);
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
}

void TextRenderer::Draw(
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    uint32_t indexCount,
    D3D12_GPU_VIRTUAL_ADDRESS transformAddress,
    D3D12_GPU_VIRTUAL_ADDRESS appearanceAddress,
    const std::string& textureKey)
{
    if (indexCount == 0) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->SetGraphicsRootConstantBufferView(0, transformAddress);
    commandList->SetGraphicsRootConstantBufferView(1, appearanceAddress);
    commandList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvHandleGPU(textureKey));
    commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void TextRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    D3D12_DESCRIPTOR_RANGE textureRange = {};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 0;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &sampler;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(result)) {
        if (errorBlob) {
            Logger::Error(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    result = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
}

void TextRenderer::CreateGraphicsPipeline()
{
    D3D12_INPUT_ELEMENT_DESC inputElements[2] = {};
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0;
    inputElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElements[0].AlignedByteOffset = 0;
    inputElements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    inputLayoutDesc.pInputElementDescs = inputElements;
    inputLayoutDesc.NumElements = _countof(inputElements);

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.StencilEnable = FALSE;

    const std::wstring vertexShaderPath =
        L"resources/Shaders/Text/Standard/Render.VS.hlsl";
    const std::wstring pixelShaderPath =
        L"resources/Shaders/Text/Standard/Render.PS.hlsl";
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader =
        dxCommon_->LoadCompiledShader(vertexShaderPath);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =
        dxCommon_->LoadCompiledShader(pixelShaderPath);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };
    pipelineDesc.BlendState = CreateBlendDesc(kBlendModeNormal);
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(result));
}
