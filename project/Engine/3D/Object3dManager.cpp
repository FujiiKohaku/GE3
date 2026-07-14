#include "Object3dManager.h"
// 実体をここで作�E　staticだとクラス冁E��宣言しただけでは実体ができなぁE��ら、ここで作る
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
    // DirectX共通部刁E��受け取り、保孁E
    dxCommon_ = dxCommon;

    // ルートシグネチャを作�E
    CreateRootSignature();

    // グラフィチE��スパイプラインを作�E
    CreateGraphicsPipeline();


    TextureManager::GetInstance()->LoadTexture("resources/Textures/skybox.dds");

    defaultEnvironmentTextureHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("resources/Textures/skybox.dds");
}
#pragma endregion
#pragma region
void Object3dManager::PreDraw()
{
    auto* commandList = dxCommon_->GetCommandList();

    // プリミティブ形状�E�三角形リスト！E
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // RootSignature 設宁E
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    //  ブレンドモードに応じぁEPSO を適用
    commandList->SetPipelineState(pipelineStates[currentBlendMode].Get());
}
#pragma endregion
#pragma region
void Object3dManager::SetNormalPSO()
{
    auto* commandList = dxCommon_->GetCommandList();

    commandList->SetPipelineState(pipelineStates[currentBlendMode].Get());
}

void Object3dManager::SetGlowPSO()
{
    auto* commandList = dxCommon_->GetCommandList();

    commandList->SetPipelineState(glowPipelineStates[currentBlendMode].Get());
}
#pragma endregion
#pragma region
void Object3dManager::CreateRootSignature()
{
    HRESULT hr;

    // ====== RootParameterの設宁E======
    D3D12_ROOT_PARAMETER rootParameters[9] = {};

    // [0] Material�E�ピクセルシェーダ用�E�E
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Transform�E�頂点シェーダ用�E�E
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // [2] Texture�E�ERV: チE��スチャ用�E�E
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    // [3] DirectionalLight�E�ライト情報�E�E
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;
    // [4] Camera�E�視点惁E���E�E
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 2; // b2
    // [5] PointLight
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 3; //  b3
    // [6] SpotLight
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 4; //  b4
    // [7] AmbientLight
    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].Descriptor.ShaderRegister = 5; // b5

    D3D12_DESCRIPTOR_RANGE descriptorRangeEnvironment[1] = {};
    descriptorRangeEnvironment[0].BaseShaderRegister = 1;
    descriptorRangeEnvironment[0].NumDescriptors = 1;
    descriptorRangeEnvironment[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeEnvironment[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].DescriptorTable.pDescriptorRanges = descriptorRangeEnvironment;
    rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnvironment);
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

    // ====== シリアライズ & 作�E ======
    hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    // 実際にルートシグネチャ作�E
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}
#pragma endregion
#pragma region
void Object3dManager::CreateGraphicsPipeline()
{

    // ====== 入力レイアウチE======
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

    // ====== チE�EススチE��シル設宁E======
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ====== シェーダーのコンパイル ======
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Object3D/Object3d.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Object3D/Object3d.PS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> glowPixelShaderBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Object3D/Glow.PS.hlsl"); // グロウ
    assert(vertexShaderBlob && pixelShaderBlob && glowPixelShaderBlob);

    // ====== PSO設宁E======
    D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc {};
    baseDesc.pRootSignature = rootSignature.Get();
    baseDesc.InputLayout = inputLayoutDesc;
    // baseDescPSはFor斁E�E中に移動しました
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

        // ===== 通常PSO =====
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;

            desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

            desc.BlendState = CreateBlendDesc(static_cast<BlendMode>(i));

            dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates[i]));
        }

        // ===== Glow PSO =====
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC glowDesc = baseDesc;

            glowDesc.PS = { glowPixelShaderBlob->GetBufferPointer(), glowPixelShaderBlob->GetBufferSize() };

            glowDesc.BlendState = CreateBlendDesc(static_cast<BlendMode>(i));

            dxCommon_->GetDevice()->CreateGraphicsPipelineState(&glowDesc, IID_PPV_ARGS(&glowPipelineStates[i]));
        }
    }
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
