#include "Object3dManager.h"
#include "Object3dRootParameter.h"
#include "Engine/Light/LightManager.h"
#include <cassert>
#include <filesystem>

namespace {
constexpr const char* kDefaultObject3dPixelShader =
    "resources/Shaders/Object3D/Unlit/Render.PS.hlsl";

std::string MakePipelineKey(const std::string& pixelShaderPath, BlendMode blendMode)
{
    return pixelShaderPath + "#" + std::to_string(static_cast<int>(blendMode));
}
}

std::unique_ptr<Object3dManager> Object3dManager::instance_ = nullptr;

Object3dManager::Object3dManager(ConstructorKey)
{
}
Object3dManager* Object3dManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<Object3dManager>(ConstructorKey());
    }
    return instance_.get();
}
#pragma region
void Object3dManager::Initialize(DirectXCommon* dxCommon)
{
    if (dxCommon_ != nullptr) {
        return;
    }

    dxCommon_ = dxCommon;
    LightManager::GetInstance()->Initialize(dxCommon_);


    CreateRootSignature();


    CreateGraphicsPipeline();


    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");

    defaultEnvironmentTextureHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds");
}
#pragma endregion
#pragma region
void Object3dManager::PreDraw()
{
    auto* commandList = dxCommon_->GetCommandList();


    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // RootSignature 設宁E
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    LightManager::GetInstance()->Bind(commandList);

    BindPipeline(kDefaultObject3dPixelShader);
}
#pragma endregion
#pragma region
void Object3dManager::SetNormalPSO()
{
    BindPipeline(kDefaultObject3dPixelShader);
}

void Object3dManager::SetGlowPSO()
{
    auto* commandList = dxCommon_->GetCommandList();

    commandList->SetPipelineState(glowPipelineStates[currentBlendMode].Get());
}

void Object3dManager::BindPipeline(const std::string& pixelShaderPath)
{
    const BlendMode blendMode = static_cast<BlendMode>(currentBlendMode);
    const std::string key = MakePipelineKey(pixelShaderPath, blendMode);
    auto found = materialPipelineCache_.find(key);
    if (found == materialPipelineCache_.end()) {
        auto pipeline = CreateMaterialPipeline(pixelShaderPath, blendMode);
        found = materialPipelineCache_.emplace(key, std::move(pipeline)).first;
    }
    dxCommon_->GetCommandList()->SetPipelineState(found->second.Get());
}
#pragma endregion
#pragma region
void Object3dManager::CreateRootSignature()
{
    HRESULT hr;

    // ====== RootParameterの設宁E======
    D3D12_ROOT_PARAMETER rootParameters[
        RootParameterIndex(Object3dRootParameter::Count)] = {};


    auto& materialParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::Material)];
    materialParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    materialParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    materialParameter.Descriptor.ShaderRegister = 0;


    auto& transformParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::TransformationMatrix)];
    transformParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    transformParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    transformParameter.Descriptor.ShaderRegister = 0;


    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    auto& textureParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::Texture)];
    textureParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    textureParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    textureParameter.DescriptorTable.pDescriptorRanges = descriptorRange;
    textureParameter.DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);


    auto& directionalLightParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::DirectionalLight)];
    directionalLightParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    directionalLightParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    directionalLightParameter.Descriptor.ShaderRegister = 1;

    auto& cameraParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::Camera)];
    cameraParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cameraParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    cameraParameter.Descriptor.ShaderRegister = 2; // b2
    // [5] PointLight
    auto& pointLightsParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::PointLights)];
    pointLightsParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pointLightsParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    pointLightsParameter.Descriptor.ShaderRegister = 3; //  b3
    // [6] SpotLight
    auto& spotLightsParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::SpotLights)];
    spotLightsParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    spotLightsParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    spotLightsParameter.Descriptor.ShaderRegister = 4; //  b4
    // [7] AmbientLight
    auto& ambientLightParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::AmbientLight)];
    ambientLightParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    ambientLightParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    ambientLightParameter.Descriptor.ShaderRegister = 5; // b5

    D3D12_DESCRIPTOR_RANGE descriptorRangeEnvironment[1] = {};
    descriptorRangeEnvironment[0].BaseShaderRegister = 1;
    descriptorRangeEnvironment[0].NumDescriptors = 1;
    descriptorRangeEnvironment[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeEnvironment[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    auto& environmentParameter = rootParameters[
        RootParameterIndex(Object3dRootParameter::EnvironmentTexture)];
    environmentParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    environmentParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    environmentParameter.DescriptorTable.pDescriptorRanges = descriptorRangeEnvironment;
    environmentParameter.DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnvironment);
    // ====== Sampler設宁E======
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ====== RootSignatureDesc設宁E======
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;


    hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }


    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}
#pragma endregion
#pragma region
void Object3dManager::CreateGraphicsPipeline()
{


    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

    // POSITION
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // TEXCOORD
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // NORMAL
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // ====== ラスタライザ設宁E======
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ====== シェーダーのコンパイル ======
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Object3D/Object3d.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> glowPixelShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Object3D/Glow.PS.hlsl"); // グロウ
    assert(vertexShaderBlob && glowPixelShaderBlob);

    // ====== PSO設宁E======
    D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc {};
    baseDesc.pRootSignature = rootSignature.Get();
    baseDesc.InputLayout = inputLayoutDesc;

    baseDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    baseDesc.RasterizerState = rasterizerDesc;
    baseDesc.DepthStencilState = depthStencilDesc;
    baseDesc.NumRenderTargets = 1;
    baseDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    baseDesc.DepthStencilState = depthStencilDesc;
    baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    baseDesc.SampleDesc.Count = 1;
    baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // ブレンド設定（とりあえずなしで初期化！E
    baseDesc.BlendState = CreateBlendDesc(kBlendModeNone);

    for (int i = 0; i < kCountOfBlendMode; i++) {

        // ===== Glow PSO =====
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC glowDesc = baseDesc;

            glowDesc.PS = { glowPixelShaderBlob->GetBufferPointer(), glowPixelShaderBlob->GetBufferSize() };

            glowDesc.BlendState = CreateBlendDesc(static_cast<BlendMode>(i));

            dxCommon_->GetDevice()->CreateGraphicsPipelineState(&glowDesc, IID_PPV_ARGS(&glowPipelineStates[i]));
        }
    }
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Object3dManager::CreateMaterialPipeline(
    const std::string& pixelShaderPath, BlendMode blendMode)
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    const auto vertexShader = dxCommon_->LoadCompiledShader(
        L"resources/Shaders/Object3D/Object3d.VS.hlsl");
    const auto pixelShader = dxCommon_->LoadCompiledShader(
        std::filesystem::path(pixelShaderPath).wstring());
    assert(vertexShader && pixelShader);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature.Get();
    desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.BlendState = CreateBlendDesc(blendMode);
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&pipeline));
    assert(SUCCEEDED(hr));
    return pipeline;
}
#pragma endregion
void Object3dManager::Finalize()
{
    instance_.reset();
}

D3D12_GPU_DESCRIPTOR_HANDLE Object3dManager::GetEnvironmentTexture()
{
    if (environmentTextureHandle_.ptr != 0) {
        return environmentTextureHandle_;
    }

    return defaultEnvironmentTextureHandle_;
}
void Object3dManager::SetEnvironmentTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
    environmentTextureHandle_ = handle;
}
