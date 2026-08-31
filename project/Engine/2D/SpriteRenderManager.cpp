#include "SpriteRenderManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/StringUtility/StringUtility.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/math/SpriteStruct.h"
#include <cassert>
#include <cctype>
#include <filesystem>

void SpriteRenderManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreateFrameParameterBuffer();
    startTime_ = std::chrono::steady_clock::now();
    previousFrameTime_ = startTime_;

    SpriteGraphicsPipelineDesc defaultPipelineDesc;
    GetOrCreateGraphicsPipeline(defaultPipelineDesc);
}

void SpriteRenderManager::PreDraw()
{
    assert(dxCommon_);
    UpdateFrameParameters();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetGraphicsRootConstantBufferView(
        4,
        frameParameterResource_->GetGPUVirtualAddress());
    currentPipelineState_ = nullptr;
}

void SpriteRenderManager::BindPipeline(const SpriteGraphicsPipelineDesc& desc)
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState =
        GetOrCreateGraphicsPipeline(desc);

    if (currentPipelineState_ == pipelineState.Get()) {
        return;
    }

    dxCommon_->GetCommandList()->SetPipelineState(pipelineState.Get());
    currentPipelineState_ = pipelineState.Get();
}

SpriteRenderManager::~SpriteRenderManager()
{
    if (frameParameterResource_ && frameParameterData_) {
        frameParameterResource_->Unmap(0, nullptr);
        frameParameterData_ = nullptr;
    }
}

void SpriteRenderManager::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameters[5] = {};

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

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].Descriptor.ShaderRegister = 2;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 3;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
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

Microsoft::WRL::ComPtr<ID3D12PipelineState> SpriteRenderManager::GetOrCreateGraphicsPipeline(
    const SpriteGraphicsPipelineDesc& desc)
{
    const std::string cacheKey = MakePipelineCacheKey(desc);
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>::iterator iterator =
        pipelineStateCache_.find(cacheKey);
    if (iterator != pipelineStateCache_.end()) {
        return iterator->second;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = CreateGraphicsPipeline(desc);
    pipelineStateCache_[cacheKey] = pipelineState;
    return pipelineState;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> SpriteRenderManager::CreateGraphicsPipeline(
    const SpriteGraphicsPipelineDesc& desc)
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
    rasterizerDesc.CullMode = desc.cullMode;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = FALSE;
    if (desc.depthTest) {
        depthStencilDesc.DepthEnable = TRUE;
    }
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    if (desc.depthWrite) {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    }
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    const std::string vertexShaderPath = NormalizeShaderPath(desc.vertexShaderPath);
    const std::string pixelShaderPath = NormalizeShaderPath(desc.pixelShaderPath);
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(StringUtility::ConvertString(vertexShaderPath));
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(StringUtility::ConvertString(pixelShaderPath));
    assert(vertexShaderBlob && pixelShaderBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineStateDesc.BlendState = CreateBlendDesc(desc.blendMode);
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));
    return pipelineState;
}

std::string SpriteRenderManager::MakePipelineCacheKey(const SpriteGraphicsPipelineDesc& desc) const
{
    std::string cacheKey = NormalizeShaderPath(desc.vertexShaderPath);
    cacheKey += "|";
    cacheKey += NormalizeShaderPath(desc.pixelShaderPath);
    cacheKey += "|Blend:";
    cacheKey += std::to_string(static_cast<int>(desc.blendMode));
    cacheKey += "|DepthTest:";
    if (desc.depthTest) {
        cacheKey += "1";
    } else {
        cacheKey += "0";
    }
    cacheKey += "|DepthWrite:";
    if (desc.depthWrite) {
        cacheKey += "1";
    } else {
        cacheKey += "0";
    }
    cacheKey += "|Cull:";
    cacheKey += std::to_string(static_cast<int>(desc.cullMode));
    return cacheKey;
}

std::string SpriteRenderManager::NormalizeShaderPath(const std::string& shaderPath) const
{
    std::string normalizedPath = std::filesystem::path(shaderPath).lexically_normal().generic_string();
    for (char& character : normalizedPath) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return normalizedPath;
}

void SpriteRenderManager::CreateFrameParameterBuffer()
{
    frameParameterResource_ = dxCommon_->CreateBufferResource(sizeof(SpriteFrameParameters));
    frameParameterResource_->SetName(L"SpriteRenderManager::FrameParameters");
    frameParameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&frameParameterData_));
    frameParameterData_->elapsedTime = 0.0f;
    frameParameterData_->deltaTime = 0.0f;
    frameParameterData_->screenSize = {
        static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
        static_cast<float>(WinApp::GetInstance()->GetClientHeight())
    };
}

void SpriteRenderManager::UpdateFrameParameters()
{
    const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
    frameParameterData_->elapsedTime =
        std::chrono::duration<float>(currentTime - startTime_).count();
    frameParameterData_->deltaTime =
        std::chrono::duration<float>(currentTime - previousFrameTime_).count();
    frameParameterData_->screenSize = {
        static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
        static_cast<float>(WinApp::GetInstance()->GetClientHeight())
    };
    previousFrameTime_ = currentTime;
}
