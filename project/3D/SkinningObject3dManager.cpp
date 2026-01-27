#include "SkinningObject3dManager.h"

SkinningObject3dManager* SkinningObject3dManager::instance = nullptr;

SkinningObject3dManager* SkinningObject3dManager::GetInstance()
{

    if (instance == nullptr) {
        instance = new SkinningObject3dManager();
    }
    return instance;
}
#pragma region 初期化処理
void SkinningObject3dManager::Initialize(DirectXCommon* dxCommon)
{
    // DirectX共通部分を受け取り、保存
    dxCommon_ = dxCommon;

    // ルートシグネチャを作成
    CreateRootSignature();

    // グラフィックスパイプラインを作成
    CreateGraphicsPipeline();
}
#pragma endregion

#pragma region 描画準備処理
void SkinningObject3dManager::PreDraw()
{
    auto* commandList = dxCommon_->GetCommandList();

    // プリミティブ形状（三角形リスト）
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // RootSignature 設定
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    //  ブレンドモードに応じた PSO を適用
    commandList->SetPipelineState(pipelineStates[currentBlendMode].Get());
}

#pragma endregion

#pragma region ルートシグネチャ作成
void SkinningObject3dManager::CreateRootSignature()
{
    HRESULT hr;

    // ====== RootParameterの設定 ======
    D3D12_ROOT_PARAMETER rootParameters[7] = {};

    // [0] Material（ピクセルシェーダ用）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Transform（頂点シェーダ用）
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // [2] Texture（SRV: テクスチャ用）
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    // [3] DirectionalLight（ライト情報）
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;
    // [4] Camera（視点情報）
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
    // ====== Sampler設定 ======
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ====== RootSignatureDesc設定 ======
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    // ====== シリアライズ & 作成 ======
    hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    // 実際にルートシグネチャ作成
    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}
#pragma endregion

#pragma region グラフィックスパイプライン作成
void SkinningObject3dManager::CreateGraphicsPipeline()
{
    

    // ====== 入力レイアウト ======
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

    // ====== ラスタライザ設定 ======
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // ====== デプスステンシル設定 ======
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ====== シェーダーのコンパイル ======
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && pixelShaderBlob);

    // ====== PSO設定 ======
    D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc {};
    baseDesc.pRootSignature = rootSignature.Get();
    baseDesc.InputLayout = inputLayoutDesc;
    baseDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    baseDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    baseDesc.RasterizerState = rasterizerDesc;
    baseDesc.DepthStencilState = depthStencilDesc;
    baseDesc.NumRenderTargets = 1;
    baseDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    baseDesc.DepthStencilState = depthStencilDesc;
    baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    baseDesc.SampleDesc.Count = 1;
    baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // ブレンド設定（とりあえずなしで初期化）
    baseDesc.BlendState = CreateBlendDesc(kBlendModeNone);
    // PSOを保存する配列
    pipelineStates[kCountOfBlendMode];

    for (int i = 0; i < kCountOfBlendMode; i++) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc; // 共通設定コピー
        desc.BlendState = CreateBlendDesc(static_cast<BlendMode>(i)); // ブレンドだけ切替
        dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates[i]));
    }
}

#pragma endregion
void SkinningObject3dManager::Finalize()
{
    delete instance;
    instance = nullptr;
}