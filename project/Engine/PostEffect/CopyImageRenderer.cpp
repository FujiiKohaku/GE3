#include "CopyImageRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/TextureManager/TextureManager.h"
#include <cassert>

void CopyImageRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreatePostEffectParameterResource();

    maskTextureHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/noise0.png"); // 繝槭せ繧ｯ繝・け繧ｹ繝√Ε縺ｨ縺励※縲・resources/noise.png"繧剃ｽｿ逕ｨ縺励※縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶る←螳懷､画峩縺励※縺上□縺輔＞縲・

    pipelineStates_[PostEffectType::Copy] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Fullscreen.PS.hlsl"); // Copy逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲∝腰邏斐↓繝・け繧ｹ繝√Ε繧呈緒逕ｻ縺吶ｋ縺縺代・繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・

    pipelineStates_[PostEffectType::GrayScale] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/GrayScale.PS.hlsl"); // GrayScale逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲√ユ繧ｯ繧ｹ繝√Ε繧偵げ繝ｬ繝ｼ繧ｹ繧ｱ繝ｼ繝ｫ縺ｧ謠冗判縺吶ｋ繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・

    pipelineStates_[PostEffectType::Vignette] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Vignette.PS.hlsl"); // Vignette逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲√ユ繧ｯ繧ｹ繝√Ε縺ｫ繝薙ロ繝・ヨ蜉ｹ譫懊ｒ驕ｩ逕ｨ縺励※謠冗判縺吶ｋ繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・

    pipelineStates_[PostEffectType::DepthOfField] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/DepthOfField.PS.hlsl");
    pipelineStates_[PostEffectType::MotionBlur] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/MotionBlur.PS.hlsl");
    pipelineStates_[PostEffectType::ChromaticAberration] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/ChromaticAberration.PS.hlsl");
    pipelineStates_[PostEffectType::LensDistortion] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LensDistortion.PS.hlsl");
    pipelineStates_[PostEffectType::FilmGrain] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/FilmGrain.PS.hlsl");
    pipelineStates_[PostEffectType::LensDirt] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LensDirt.PS.hlsl");
    pipelineStates_[PostEffectType::CameraShake] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/CameraShake.PS.hlsl");
    pipelineStates_[PostEffectType::BokehShape] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/BokehShape.PS.hlsl");
    pipelineStates_[PostEffectType::Fisheye] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Fisheye.PS.hlsl");

    pipelineStates_[PostEffectType::Pixelate] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Pixelate.PS.hlsl");

    pipelineStates_[PostEffectType::ColorAdjust] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/ColorAdjust.PS.hlsl");

    pipelineStates_[PostEffectType::smoothing] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/BoxFilter.PS.hlsl"); // smoothing逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲√ユ繧ｯ繧ｹ繝√Ε縺ｫ蜊倡ｴ斐↑繝懊ャ繧ｯ繧ｹ繝輔ぅ繝ｫ繧ｿ繧帝←逕ｨ縺励※謠冗判縺吶ｋ繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・

    pipelineStates_[PostEffectType::GaussianFilter] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/GaussianFilter.PS.hlsl"); // GaussianFilter逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲√ユ繧ｯ繧ｹ繝√Ε縺ｫ繧ｬ繧ｦ繧ｷ繧｢繝ｳ繝輔ぅ繝ｫ繧ｿ繧帝←逕ｨ縺励※謠冗判縺吶ｋ繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・
    pipelineStates_[PostEffectType::LuminanceBasedOutline] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LuminanceBasedOutline.PS.hlsl"); // LuminanceBasedOutline逕ｨ縺ｮ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｯ縲√ユ繧ｯ繧ｹ繝√Ε縺ｮ霈晏ｺｦ縺ｫ蝓ｺ縺･縺・※霈ｪ驛ｭ繧呈緒逕ｻ縺吶ｋ繧ゅ・繧堤畑諢上＠縺ｦ縺・ｋ縺ｨ莉ｮ螳壹＠縺ｦ縺・∪縺吶・
    pipelineStates_[PostEffectType::Bloom] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Fullscreen.PS.hlsl");
    pipelineStates_[PostEffectType::LensFlare] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LensFlare.PS.hlsl");
    pipelineStates_[PostEffectType::Glare] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Glare.PS.hlsl");
    pipelineStates_[PostEffectType::LightShafts] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LightShafts.PS.hlsl");
    pipelineStates_[PostEffectType::VolumetricLight] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/VolumetricLight.PS.hlsl");
    pipelineStates_[PostEffectType::AnamorphicFlare] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/AnamorphicFlare.PS.hlsl");
    pipelineStates_[PostEffectType::Halo] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Halo.PS.hlsl");
    pipelineStates_[PostEffectType::LightStreak] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/LightStreak.PS.hlsl");
    pipelineStates_[PostEffectType::NeonGlow] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/NeonGlow.PS.hlsl");
    pipelineStates_[PostEffectType::GhostImage] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/GhostImage.PS.hlsl");

    pipelineStates_[PostEffectType::DepthOutline] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/DepthBasedOutline.PS.hlsl");
    pipelineStates_[PostEffectType::Outline] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/DepthBasedOutline.PS.hlsl");

    pipelineStates_[PostEffectType::RadialBlur] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/RadialBlur.PS.hlsl");
    pipelineStates_[PostEffectType::FocusLine] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/FocusLine.PS.hlsl");

    pipelineStates_[PostEffectType::Dissolve] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Dissolve.PS.hlsl");

    pipelineStates_[PostEffectType::Random] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Random.PS.hlsl");
    pipelineStates_[PostEffectType::Paint] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/Paint.PS.hlsl");
    pipelineStates_[PostEffectType::GlassCrack] = CreateGraphicsPipeline(L"resources/Shaders/PostEffect/GlassCrack.PS.hlsl");

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

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->LoadCompiledShader(pixelShaderPath);

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

void CopyImageRenderer::Draw(
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle)
{
    ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    std::unordered_map<PostEffectType, Microsoft::WRL::ComPtr<ID3D12PipelineState>>::iterator iterator = pipelineStates_.find(currentPostEffectType_);
    if (iterator == pipelineStates_.end()) {
        iterator = pipelineStates_.find(PostEffectType::Copy);
    }
    assert(iterator != pipelineStates_.end());
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = iterator->second;

    commandList->SetPipelineState(pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_GPU_DESCRIPTOR_HANDLE secondTextureHandle = depthTextureHandle;

    if (currentPostEffectType_ == PostEffectType::Dissolve) {
        secondTextureHandle = maskTextureHandle_;
    }

    commandList->SetGraphicsRootDescriptorTable(0, textureHandle);
    commandList->SetGraphicsRootDescriptorTable(1, secondTextureHandle);

    commandList->SetGraphicsRootConstantBufferView(
        2,
        postEffectParameterResource_->GetGPUVirtualAddress());

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
    // radialBlur縺ｮ繝代Λ繝｡繝ｼ繧ｿ縺ｮ蛻晄悄蛟､繧りｨｭ螳・
    postEffectParameterData_->radialBlurCenter = { 0.5f, 0.5f };
    postEffectParameterData_->radialBlurSampleCount = 32;
    postEffectParameterData_->radialBlurWidth = 0.08f;
    // Dissolve繧ｨ繝輔ぉ繧ｯ繝医・繝代Λ繝｡繝ｼ繧ｿ縺ｮ蛻晄悄蛟､繧りｨｭ螳・
    postEffectParameterData_->dissolveThreshold = 0.5f;
    postEffectParameterData_->dissolveEdgeWidth = 0.05f;
    postEffectParameterData_->dissolveEdgeStrength = 2.0f;
    postEffectParameterData_->dissolvePadding = 0.0f;
    postEffectParameterData_->boostKickStrength = 0.0f;
    postEffectParameterData_->pixelSize = 8.0f;
    postEffectParameterData_->colorBrightness = 0.03f;
    postEffectParameterData_->colorContrast = 1.15f;
    postEffectParameterData_->colorSaturation = 1.25f;
    postEffectParameterData_->padding0 = 0.0f;
    postEffectParameterData_->padding1 = 0.0f;
    postEffectParameterData_->padding2 = 0.0f;
    postEffectParameterData_->focusDepth = 0.99f;
    postEffectParameterData_->focusRange = 0.01f;
    postEffectParameterData_->depthOfFieldRadius = 8.0f;
    postEffectParameterData_->motionBlurStrength = 0.025f;
    postEffectParameterData_->motionBlurDirection = { 1.0f, 0.2f };
    postEffectParameterData_->motionBlurSampleCount = 12;
    postEffectParameterData_->chromaticAberrationStrength = 0.008f;
    postEffectParameterData_->lensDistortionStrength = 0.2f;
    postEffectParameterData_->filmGrainStrength = 0.08f;
    postEffectParameterData_->lensDirtStrength = 1.0f;
    postEffectParameterData_->cameraShakeStrength = 0.008f;
    postEffectParameterData_->bokehRadius = 10.0f;
    postEffectParameterData_->bokehSides = 6;
    postEffectParameterData_->fisheyeStrength = 1.2f;
    postEffectParameterData_->animationEnabled = 1;
    postEffectParameterData_->lightThreshold = 0.65f;
    postEffectParameterData_->lightStrength = 1.0f;
    postEffectParameterData_->lightRadius = 0.2f;
    postEffectParameterData_->lightAngle = 0.0f;
}
CopyImageRenderer::PostEffectParameter&CopyImageRenderer::GetPostEffectParameter()
{
    return *postEffectParameterData_;
}
void CopyImageRenderer::SetMaskTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
    maskTextureHandle_ = handle;
}
