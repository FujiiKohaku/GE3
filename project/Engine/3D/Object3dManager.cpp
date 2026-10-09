#include "Object3dManager.h"
#include "Object3dRootParameter.h"
#include "Engine/Light/LightManager.h"
#include <cassert>
#include <filesystem>
#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/Shadow/LocalShadowRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Raytracing/DxrRenderer.h"

namespace {
constexpr const char* kDefaultObject3dPixelShader =
    "resources/Shaders/Object3D/Unlit/Render.PS.hlsl";

std::string MakePipelineKey(const std::string& pixelShaderPath, BlendMode blendMode,
    bool transparent, bool transparentDepthWrite, const std::string& vertexShaderPath)
{
    std::string key = vertexShaderPath + "#" + pixelShaderPath + "#" +
        std::to_string(static_cast<int>(blendMode));
    if (transparent) {
        if (transparentDepthWrite) {
            return key + "#transparent-depth-write";
        }
        return key + "#transparent";
    }
    return key + "#default";
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


    disabledShadowConstants_ = dxCommon_->CreateBufferResource(sizeof(ShadowConstants));
    ShadowConstants* disabled = nullptr;
    disabledShadowConstants_->Map(0, nullptr, reinterpret_cast<void**>(&disabled));
    *disabled = {};
    disabled->lightViewProjection = MatrixMath::MakeIdentity4x4();
    disabledLocalShadowConstants_ = dxCommon_->CreateBufferResource(sizeof(LocalShadowConstants));
    LocalShadowConstants* disabledLocal = nullptr;
    disabledLocalShadowConstants_->Map(0, nullptr, reinterpret_cast<void**>(&disabledLocal));
    *disabledLocal = {};
    disabledRtLocalShadowConstants_ = dxCommon_->CreateBufferResource(sizeof(DxrLocalShadowParameters));
    DxrLocalShadowParameters* disabledRtLocal = nullptr;
    disabledRtLocalShadowConstants_->Map(0, nullptr, reinterpret_cast<void**>(&disabledRtLocal));
    *disabledRtLocal = {};
    nullLocalShadowSrv_ = SrvManager::GetInstance()->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC localNullDesc {};
    localNullDesc.Format = DXGI_FORMAT_R32_FLOAT;
    localNullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    localNullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    localNullDesc.Texture2DArray.ArraySize = 8; localNullDesc.Texture2DArray.MipLevels = 1;
    dxCommon_->GetDevice()->CreateShaderResourceView(nullptr, &localNullDesc, SrvManager::GetInstance()->GetCPUDescriptorHandle(nullLocalShadowSrv_));
    nullShadowSrv_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVforTexture2D(nullShadowSrv_, nullptr, DXGI_FORMAT_R32_FLOAT, 1);

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

void Object3dManager::ReloadMaterialPipelines()
{
    dxCommon_->WaitForGPU();
    materialPipelineCache_.clear();
}

void Object3dManager::SetGlowPSO()
{
    auto* commandList = dxCommon_->GetCommandList();

    commandList->SetPipelineState(glowPipelineStates[currentBlendMode].Get());
}

void Object3dManager::BindPipeline(const std::string& pixelShaderPath, bool transparent,
    bool transparentDepthWrite, const std::string& vertexShaderPath)
{
    BlendMode blendMode = static_cast<BlendMode>(currentBlendMode);
    if (transparent) {
        blendMode = kBlendModeNormal;
    }
    const std::string key = MakePipelineKey(
        pixelShaderPath, blendMode, transparent, transparentDepthWrite, vertexShaderPath);
    auto found = materialPipelineCache_.find(key);
    if (found == materialPipelineCache_.end()) {
        auto pipeline = CreateMaterialPipeline(
            pixelShaderPath, blendMode, transparent, transparentDepthWrite, vertexShaderPath);
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
        kLocalShadowTextureRootIndex + 1] = {};


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
    D3D12_DESCRIPTOR_RANGE normalRange {};
    normalRange.BaseShaderRegister = 3;
    normalRange.NumDescriptors = 1;
    normalRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    normalRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    auto& normalParameter = rootParameters[RootParameterIndex(Object3dRootParameter::NormalTexture)];
    normalParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    normalParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    normalParameter.DescriptorTable = { 1, &normalRange };


    auto& vertexShaderParameters = rootParameters[kVertexShaderParametersRootIndex];
    vertexShaderParameters.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    vertexShaderParameters.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    vertexShaderParameters.Constants.ShaderRegister = 1;
    vertexShaderParameters.Constants.Num32BitValues = 4;
    auto& rtLocalSettings = rootParameters[RootParameterIndex(Object3dRootParameter::RtLocalShadowSettings)];
    rtLocalSettings.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rtLocalSettings.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; rtLocalSettings.Descriptor.ShaderRegister = 9;
    auto& rtLocalReceiver = rootParameters[RootParameterIndex(Object3dRootParameter::RtLocalShadowReceiver)];
    rtLocalReceiver.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rtLocalReceiver.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; rtLocalReceiver.Constants = {10, 0, 1};
    auto& shadowConstants = rootParameters[kShadowConstantsRootIndex];
    shadowConstants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    shadowConstants.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    shadowConstants.Descriptor.ShaderRegister = 6;
    D3D12_DESCRIPTOR_RANGE shadowRange {};
    shadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange.BaseShaderRegister = 2;
    shadowRange.NumDescriptors = 1;
    auto& shadowTexture = rootParameters[kShadowTextureRootIndex];
    shadowTexture.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    shadowTexture.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    shadowTexture.DescriptorTable = { 1, &shadowRange };
    auto& receiver = rootParameters[kShadowReceiverRootIndex];
    receiver.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    receiver.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    receiver.Constants.ShaderRegister = 7;
    receiver.Constants.Num32BitValues = 1;
    auto& localConstants = rootParameters[kLocalShadowConstantsRootIndex];
    localConstants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    localConstants.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    localConstants.Descriptor.ShaderRegister = 8;
    D3D12_DESCRIPTOR_RANGE localRange {};
    localRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    localRange.BaseShaderRegister = 4; localRange.NumDescriptors = 1;
    localRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    auto& localTexture = rootParameters[kLocalShadowTextureRootIndex];
    localTexture.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    localTexture.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    localTexture.DescriptorTable = { 1, &localRange };
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

    D3D12_STATIC_SAMPLER_DESC samplers[2] { staticSampler, staticSampler };
    samplers[1].ShaderRegister = 1;
    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    // ====== RootSignatureDesc設宁E======
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = samplers;
    desc.NumStaticSamplers = 2;


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
    baseDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    baseDesc.NumRenderTargets = 4;
    baseDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    baseDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    baseDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
    baseDesc.DepthStencilState = depthStencilDesc;
    baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    baseDesc.SampleDesc.Count = 1;
    baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // ブレンド設定（とりあえずなしで初期化！E
    baseDesc.BlendState = CreateBlendDesc(kBlendModeNone);
    baseDesc.BlendState.IndependentBlendEnable = TRUE;
    baseDesc.BlendState.RenderTarget[2] = baseDesc.BlendState.RenderTarget[0];
    baseDesc.BlendState.RenderTarget[2].RenderTargetWriteMask = 0;
    baseDesc.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;

    for (int i = 0; i < kCountOfBlendMode; i++) {

        // ===== Glow PSO =====
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC glowDesc = baseDesc;

            glowDesc.PS = { glowPixelShaderBlob->GetBufferPointer(), glowPixelShaderBlob->GetBufferSize() };

            glowDesc.BlendState = CreateBlendDesc(static_cast<BlendMode>(i));
            glowDesc.BlendState.IndependentBlendEnable = TRUE;
            glowDesc.BlendState.RenderTarget[2].RenderTargetWriteMask = 0;
            glowDesc.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;

            dxCommon_->GetDevice()->CreateGraphicsPipelineState(&glowDesc, IID_PPV_ARGS(&glowPipelineStates[i]));
        }
    }
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Object3dManager::CreateMaterialPipeline(
    const std::string& pixelShaderPath, BlendMode blendMode, bool transparent,
    bool transparentDepthWrite, const std::string& vertexShaderPath)
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

    if (transparent && !transparentDepthWrite) {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    }

    const auto vertexShader = dxCommon_->LoadCompiledShader(
        std::filesystem::path(vertexShaderPath).wstring());
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
    desc.BlendState.IndependentBlendEnable = TRUE;
    desc.BlendState.RenderTarget[2] = desc.BlendState.RenderTarget[0];
    desc.BlendState.RenderTarget[3] = desc.BlendState.RenderTarget[0];
    desc.BlendState.RenderTarget[3].BlendEnable = FALSE;
    // Normal/depth metadata must never use the color target's alpha blending.
    desc.BlendState.IndependentBlendEnable = TRUE;
    desc.BlendState.RenderTarget[1] = desc.BlendState.RenderTarget[0];
    desc.BlendState.RenderTarget[1].BlendEnable = FALSE;
    if (transparent) {
        // Write unblended normals only when this surface also writes depth.
        desc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
        desc.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;
        if (transparentDepthWrite) {
            desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            desc.BlendState.RenderTarget[3].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
    }
    desc.NumRenderTargets = 4;
    desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (pixelShaderPath.find("/Raytracing/") != std::string::npos) {
        desc.NumRenderTargets = 5;
        desc.RTVFormats[4] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        if (pixelShaderPath.find("/Reflections") != std::string::npos || pixelShaderPath.find("/LocalShadows") != std::string::npos) {
            desc.NumRenderTargets = 7;
            desc.RTVFormats[5] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.RTVFormats[6] = DXGI_FORMAT_R32G32B32A32_FLOAT;
            for (uint32_t index = 5; index < 7; ++index) {
                desc.BlendState.RenderTarget[index].BlendEnable = FALSE;
                desc.BlendState.RenderTarget[index].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            }
        }
        if (pixelShaderPath.find("/LocalShadows") != std::string::npos) {
            desc.NumRenderTargets = 8; desc.RTVFormats[7] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.BlendState.RenderTarget[7].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        desc.BlendState.RenderTarget[4].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
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

Object3dManager::~Object3dManager()
{
    if (nullLocalShadowSrv_ != 0xffffffffu) { SrvManager::GetInstance()->Free(nullLocalShadowSrv_); }
    if (nullShadowSrv_ != 0xffffffffu) { SrvManager::GetInstance()->Free(nullShadowSrv_); }
}

D3D12_GPU_VIRTUAL_ADDRESS Object3dManager::GetRtLocalShadowConstantsAddress() const {
    auto* scene = DxrRenderer::GetActive();
    if (scene && scene->GetLocalShadowConstantsAddress() != 0) { return scene->GetLocalShadowConstantsAddress(); }
    return disabledRtLocalShadowConstants_->GetGPUVirtualAddress();
}
void Object3dManager::BindShadowResources(bool receiveShadow,
    uint32_t constantsIndex, uint32_t textureIndex, uint32_t receiverIndex)
{
    auto* cmd = dxCommon_->GetCommandList();
    D3D12_GPU_VIRTUAL_ADDRESS address = disabledShadowConstants_->GetGPUVirtualAddress();
    auto srv = SrvManager::GetInstance()->GetGPUDescriptorHandle(nullShadowSrv_);
    if (shadowRenderer_ != nullptr) {
        address = shadowRenderer_->GetConstantsAddress();
        srv = shadowRenderer_->GetSrv();
    }
    cmd->SetGraphicsRootConstantBufferView(constantsIndex, address);
    cmd->SetGraphicsRootDescriptorTable(textureIndex, srv);
    uint32_t receiver = 0;
    if (receiveShadow) { receiver = 1; }
    cmd->SetGraphicsRoot32BitConstant(receiverIndex, receiver, 0);
    if (receiverIndex == kShadowReceiverRootIndex) {
        auto localAddress = disabledLocalShadowConstants_->GetGPUVirtualAddress();
        auto localSrv = SrvManager::GetInstance()->GetGPUDescriptorHandle(nullLocalShadowSrv_);
        if (localShadowRenderer_ != nullptr && localShadowRenderer_->HasValidFrame()) {
            localAddress = localShadowRenderer_->GetConstantsAddress();
            localSrv = localShadowRenderer_->GetSrv();
        }
        cmd->SetGraphicsRootConstantBufferView(kLocalShadowConstantsRootIndex, localAddress);
        cmd->SetGraphicsRootDescriptorTable(kLocalShadowTextureRootIndex, localSrv);
    }
}
