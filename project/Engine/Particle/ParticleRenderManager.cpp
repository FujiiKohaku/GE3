#include "ParticleRenderManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/StringUtility/StringUtility.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <format>
#pragma region
void ParticleRenderManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreateDefaultGraphicsPipelines();
}
#pragma endregion
#pragma region
void ParticleRenderManager::PreDraw(int blendMode)
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    int validBlendMode = blendMode;
    if (validBlendMode < 0 || validBlendMode >= kCountOfBlendMode) {
        validBlendMode = kBlendModeAdd;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[validBlendMode].Get());
}

void ParticleRenderManager::PreDraw()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
}
#pragma endregion
#pragma region
void ParticleRenderManager::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE particleRange {};
    particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleRange.NumDescriptors = 1;
    particleRange.BaseShaderRegister = 0;
    particleRange.RegisterSpace = 0;
    particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE textureRange {};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 1;
    textureRange.RegisterSpace = 0;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] = {};

    // [0] Material : PS b0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Particle : VS t0
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &particleRange;

    // [2] Texture : PS t1
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;

    // [3] PerView : VS b0
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    // [4] Fog : PS b1
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 1;

    // [5] Effect render parameters : b2
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[5].Descriptor.ShaderRegister = 2;

    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
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
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
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
#pragma endregion
#pragma region
void ParticleRenderManager::CreateDefaultGraphicsPipelines()
{
    GraphicsPipelineDesc desc {};
    desc.effectName = "DefaultParticle";
    desc.vertexShaderPath = "resources/Shaders/Effects/Common/Particle.VS.hlsl";
    desc.pixelShaderPath = "resources/Shaders/Effects/Common/Particle.PS.hlsl";
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.cullMode = D3D12_CULL_MODE_NONE;

    for (int blendModeIndex = 0; blendModeIndex < kCountOfBlendMode; blendModeIndex++) {
        desc.blendMode = static_cast<BlendMode>(blendModeIndex);
        pipelineStates_[blendModeIndex] = CreateGraphicsPipeline(desc);
    }
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> ParticleRenderManager::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc)
{
    const std::string cacheKey = MakePipelineCacheKey(desc);
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>::iterator pipelineIterator =
        pipelineStateCache_.find(cacheKey);
    if (pipelineIterator != pipelineStateCache_.end()) {
        return pipelineIterator->second;
    }

#ifdef _DEBUG
    const std::chrono::steady_clock::time_point beginTime =
        std::chrono::steady_clock::now();
#endif

    D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[3] = {};

    inputElementDescriptions[0].SemanticName = "POSITION";
    inputElementDescriptions[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescriptions[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescriptions[1].SemanticName = "TEXCOORD";
    inputElementDescriptions[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescriptions[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescriptions[2].SemanticName = "NORMAL";
    inputElementDescriptions[2].SemanticIndex = 0;
    inputElementDescriptions[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescriptions[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    if (desc.usesVertexInput) {
        inputLayoutDesc.pInputElementDescs = inputElementDescriptions;
        inputLayoutDesc.NumElements = _countof(inputElementDescriptions);
    }

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = desc.cullMode;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
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

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = LoadCompiledShaderWithLog(
        desc.effectName,
        "VS",
        desc.vertexShaderPath);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = LoadCompiledShaderWithLog(
        desc.effectName,
        "PS",
        desc.pixelShaderPath);
    assert(vertexShaderBlob && pixelShaderBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc {};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.BlendState = CreateBlendDesc(desc.blendMode);

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));

#ifdef _DEBUG
    Logger::Log(std::format(
        "[EffectPSO] {} Render Graphics: {} ms",
        desc.effectName,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - beginTime).count()));
#endif

    pipelineStateCache_[cacheKey] = pipelineState;
    return pipelineState;
}

Microsoft::WRL::ComPtr<IDxcBlob> ParticleRenderManager::LoadCompiledShaderWithLog(
    const std::string& effectName,
    const std::string& shaderStage,
    const std::string& shaderPath)
{
    const std::filesystem::path shaderFilePath(shaderPath);
    const std::filesystem::path fullShaderPath = std::filesystem::absolute(shaderFilePath);

    if (!std::filesystem::is_regular_file(shaderFilePath)) {
        Logger::Error(
            "Particle render shader file is missing. Effect:" + effectName +
            " Stage:" + shaderStage +
            " Path:" + fullShaderPath.generic_string());
        assert(false);
        return nullptr;
    }

    Logger::Log(
        "Load particle render shader. Effect:" + effectName +
        " Stage:" + shaderStage +
        " Path:" + fullShaderPath.generic_string());

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob =
        dxCommon_->LoadCompiledShader(StringUtility::ConvertString(shaderPath));

    if (!shaderBlob) {
        Logger::Error(
            "Particle render shader load failed. Effect:" + effectName +
            " Stage:" + shaderStage +
            " Path:" + fullShaderPath.generic_string());
    }

    return shaderBlob;
}

std::string ParticleRenderManager::MakePipelineCacheKey(const GraphicsPipelineDesc& desc) const
{
    std::string cacheKey;
    cacheKey += desc.vertexShaderPath;
    cacheKey += "|";
    cacheKey += desc.pixelShaderPath;
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
    cacheKey += "|VertexInput:";
    if (desc.usesVertexInput) {
        cacheKey += "1";
    } else {
        cacheKey += "0";
    }
    cacheKey += "|Cull:";
    cacheKey += std::to_string(static_cast<int>(desc.cullMode));

    return cacheKey;
}
#pragma endregion
