#include "SkinningObject3dManager.h"

// cpp
std::unique_ptr<SkinningObject3dManager> SkinningObject3dManager::instance_ = nullptr;

SkinningObject3dManager::SkinningObject3dManager(ConstructorKey) { }

SkinningObject3dManager* SkinningObject3dManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<SkinningObject3dManager>(ConstructorKey {});
    }
    return instance_.get();
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

    CreateComputeRootSignature();
    CreateComputePipeline();
}
#pragma endregion

#pragma region 描画準備処理
void SkinningObject3dManager::PreDraw()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetPipelineState(pipelineStates[currentBlendMode].Get());
}

#pragma endregion

#pragma region ルートシグネチャ作成
void SkinningObject3dManager::CreateRootSignature()
{
    HRESULT hr;

    D3D12_ROOT_PARAMETER rootParameters[9] = {};

    // [0] Material（PS : b0）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Transform（VS : b0）
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // [2] Texture（PS : t0）
    D3D12_DESCRIPTOR_RANGE textureRange {};
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // [3] DirectionalLight（PS : b1）
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // [4] Camera（PS : b2）
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 2;

    // [5] PointLight（PS : b3）
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 3;

    // [6] SpotLight（PS : b4）
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 4;

    // [7] MatrixPalette（VS : t0）
    D3D12_DESCRIPTOR_RANGE matrixPaletteRange {};
    matrixPaletteRange.BaseShaderRegister = 0;
    matrixPaletteRange.NumDescriptors = 1;
    matrixPaletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    matrixPaletteRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[7].DescriptorTable.pDescriptorRanges = &matrixPaletteRange;
    rootParameters[7].DescriptorTable.NumDescriptorRanges = 1;
    // [8] EnvironmentTexture（PS : t1）
    D3D12_DESCRIPTOR_RANGE environmentTextureRange {};
    environmentTextureRange.BaseShaderRegister = 1;
    environmentTextureRange.NumDescriptors = 1;
    environmentTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentTextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].DescriptorTable.pDescriptorRanges = &environmentTextureRange;
    rootParameters[8].DescriptorTable.NumDescriptorRanges = 1;
    // ===============================
    // Sampler
    // ===============================
    D3D12_STATIC_SAMPLER_DESC staticSampler {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ===============================
    // RootSignatureDesc
    // ===============================
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    // Serialize
    hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(
                reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    // Create
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
    HRESULT hr;

    std::array<D3D12_INPUT_ELEMENT_DESC, 5> inputElementDescs {};

    // ---------------------------------
    // Stream 0 : VertexData
    // ---------------------------------
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].InputSlot = 0;
    inputElementDescs[0].AlignedByteOffset = 0;
    inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[0].InstanceDataStepRate = 0;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].InputSlot = 0;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[1].InstanceDataStepRate = 0;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].InputSlot = 0;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[2].InstanceDataStepRate = 0;

    // ---------------------------------
    // Stream 1 : VertexInfluence
    // ---------------------------------
    inputElementDescs[3].SemanticName = "WEIGHT";
    inputElementDescs[3].SemanticIndex = 0;
    inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[3].InputSlot = 1; //
    inputElementDescs[3].AlignedByteOffset = 0;
    inputElementDescs[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[3].InstanceDataStepRate = 0;

    inputElementDescs[4].SemanticName = "INDEX";
    inputElementDescs[4].SemanticIndex = 0;
    inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
    inputElementDescs[4].InputSlot = 1; //
    inputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[4].InstanceDataStepRate = 0;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs.data();
    inputLayoutDesc.NumElements = static_cast<UINT>(inputElementDescs.size());

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

void SkinningObject3dManager::CreateComputeRootSignature()
{
    HRESULT hr;

D3D12_ROOT_PARAMETER rootParameters[5] = {};

    // t0 : MatrixPalette
    D3D12_DESCRIPTOR_RANGE matrixPaletteRange = {};
    matrixPaletteRange.BaseShaderRegister = 0;
    matrixPaletteRange.NumDescriptors = 1;
    matrixPaletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    matrixPaletteRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &matrixPaletteRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // t1 : InputVertices
    D3D12_DESCRIPTOR_RANGE inputVertexRange = {};
    inputVertexRange.BaseShaderRegister = 1;
    inputVertexRange.NumDescriptors = 1;
    inputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    inputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &inputVertexRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // t2 : Influences
    D3D12_DESCRIPTOR_RANGE influenceRange = {};
    influenceRange.BaseShaderRegister = 2;
    influenceRange.NumDescriptors = 1;
    influenceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    influenceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &influenceRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // u0 : OutputVertices
    D3D12_DESCRIPTOR_RANGE outputVertexRange = {};
    outputVertexRange.BaseShaderRegister = 0;
    outputVertexRange.NumDescriptors = 1;
    outputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &outputVertexRange;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

    // b0 : SkinningInformation
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 0;


    ///////////////////////////
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&computeRootSignature_));
    assert(SUCCEEDED(hr));
}

void SkinningObject3dManager::CreateComputePipeline()
{
    HRESULT hr;

    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Skinning.CS.hlsl", L"cs_6_0");
    assert(computeShaderBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = computeRootSignature_.Get();
    desc.CS.pShaderBytecode = computeShaderBlob->GetBufferPointer();
    desc.CS.BytecodeLength = computeShaderBlob->GetBufferSize();

    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&desc,IID_PPV_ARGS(&computePipelineState_));assert(SUCCEEDED(hr));
}



void SkinningObject3dManager::Finalize()
{
    instance_.reset();
}