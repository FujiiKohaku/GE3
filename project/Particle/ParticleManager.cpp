#include "ParticleManager.h"
#include <cassert>

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;

    CreateRootSignature();
    CreateGraphicsPipeline();
    CreateInstancingBuffer();
    CreateSrvBuffer();
    InitTransforms();
    CreateBoardMesh();
}
void ParticleManager::Update()
{
    UpdateTransforms();
}

void ParticleManager::Draw()
{
    auto* cmd = dxCommon_->GetCommandList();

    // [0] b0（Material CBV: PS）
    cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

    // [1] t0（Instancing StructuredBuffer: VS）
    cmd->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

    // [2] t1（Texture SRV: PS）
    cmd->SetGraphicsRootDescriptorTable(2, srvHandle);

    cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
    cmd->IASetIndexBuffer(&indexBufferView);

    cmd->DrawIndexedInstanced(6, kNumInstance, 0, 0, 0);
}
void ParticleManager::PreDraw()
{
    auto* cmd = dxCommon_->GetCommandList();

    // パーティクル用の PSO と RootSignature を使う！
    cmd->SetPipelineState(pipelineState.Get());
    cmd->SetGraphicsRootSignature(rootSignature.Get());

    // 必要なら IA 設定もここで
}

void ParticleManager::CreateRootSignature()
{

    // ========= SRV (t0) : Instancing Transform =========
    D3D12_DESCRIPTOR_RANGE instancingRange {};
    instancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    instancingRange.NumDescriptors = 1;
    instancingRange.BaseShaderRegister = 0; // t0
    instancingRange.RegisterSpace = 0;
    instancingRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ========= SRV (t1) : Texture =========
    D3D12_DESCRIPTOR_RANGE texRange {};
    texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange.NumDescriptors = 1;
    texRange.BaseShaderRegister = 1; // t1
    texRange.RegisterSpace = 0;
    texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ========= RootParameters =========
    D3D12_ROOT_PARAMETER rootParams[3] = {};

    // [0] Material (b0, PS)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[0].Descriptor.ShaderRegister = 0; // b0

    // [1] Instancing SRV (t0, VS)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &instancingRange;

    // [2] Texture SRV (t1, PS)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &texRange;

    // ========= Sampler =========
    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.ShaderRegister = 0;

    // ========= RootSignatureDesc =========
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;

    // ========= Serialize =========
    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errBlob);

    if (FAILED(hr)) {
        if (errBlob) {
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
        }
        assert(false);
    }

    // ========= Create Signature =========
    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGraphicsPipeline()
{

    HRESULT hr;

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

    // ====== ブレンド設定 ======
    D3D12_BLEND_DESC blendDesc {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

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
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && pixelShaderBlob);

    // ====== PSO設定 ======
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DepthStencilState = depthStencilDesc;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // PSOを生成
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
}

void ParticleManager::CreateInstancingBuffer()
{
    // GPUバッファを作成
    instancingResource = dxCommon_->CreateBufferResource(
        sizeof(TransformationMatrix) * kNumInstance);

    // 初期化
    TransformationMatrix* instanceData = nullptr;
    instancingResource->Map(0, nullptr, (void**)&instanceData);

    for (uint32_t i = 0; i < kNumInstance; i++) {
        instanceData[i].WVP = MatrixMath::MakeIdentity4x4();
        instanceData[i].World = MatrixMath::MakeIdentity4x4();
    }

    instancingResource->Unmap(0, nullptr);
}

void ParticleManager::CreateSrvBuffer()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc {};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.NumElements = kNumInstance;
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);

    auto handleCPU = srvManager_->GetCPUDescriptorHandle(3);
    auto handleGPU = srvManager_->GetGPUDescriptorHandle(3);

    dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, handleCPU);

    instancingSrvHandleGPU_ = handleGPU;
}

void ParticleManager::InitTransforms()
{
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        transforms[i].scale = { 1.0f, 1.0f, 1.0f };
        transforms[i].rotate = { 0.0f, 0.0f, 0.0f };
        transforms[i].translate = { i * 0.1f, i * 0.1f, i * -0.1f };
    }
}
void ParticleManager::UpdateTransforms()
{
    // instancingResource を map
    TransformationMatrix* instanceData = nullptr;
    instancingResource->Map(0, nullptr, (void**)&instanceData);

    Matrix4x4 vp = camera_->GetViewProjectionMatrix();

    for (uint32_t i = 0; i < kNumInstance; ++i) {

        Matrix4x4 world = MatrixMath::MakeAffineMatrix(
            transforms[i].scale,
            transforms[i].rotate,
            transforms[i].translate);

        Matrix4x4 wvp = MatrixMath::Multiply(world, vp);

        instanceData[i].World = world;
        instanceData[i].WVP = wvp;
    }

    instancingResource->Unmap(0, nullptr);
}
void ParticleManager::CreateBoardMesh()
{
    // ===========================
    // ① 頂点データ作成
    // ===========================

    // 左上
    vertices[0].position = { -0.5f, 0.5f, 0, -1.0f };
    vertices[0].texcoord = { 0, 0 };
    vertices[0].normal = { 0, 0, -1 };

    // 右上
    vertices[1].position = { 0.5f, 0.5f, 0, -1.0f };
    vertices[1].texcoord = { 1, 0 };
    vertices[1].normal = { 0, 0, -1 };

    // 右下
    vertices[2].position = { 0.5f, -0.5f, 0, -1.0f };
    vertices[2].texcoord = { 1, 1 };
    vertices[2].normal = { 0, 0, -1 };

    // 左下
    vertices[3].position = { -0.5f, -0.5f, 0, -1.0f };
    vertices[3].texcoord = { 0, 1 };
    vertices[3].normal = { 0, 0, -1 };

    // ===========================
    // ② 頂点バッファ作成
    // ===========================
    vertexResource = dxCommon_->CreateBufferResource(sizeof(vertices));

    VertexData* vbData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vbData));
    memcpy(vbData, vertices, sizeof(vertices));
    vertexResource->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(vertices);
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // ===========================
    // ③ マテリアルCB作成（Root[0]）
    // ===========================
    materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
    Material* matCB = nullptr;
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&matCB));

    // CPU側の struct に値を入れる
    materialData_.color = { 1, 1, 1, 1 }; // 白
    materialData_.enableLighting = 0; // とりあえずライティングなし
    materialData_.padding[0] = 0.0f;
    materialData_.padding[1] = 0.0f;
    materialData_.padding[2] = 0.0f;
    materialData_.uvTransform = MatrixMath::MakeIdentity4x4();

    // CB にコピー
    *matCB = materialData_;
    materialResource->Unmap(0, nullptr);

    // ===========================
    // TransformCB（板ポリ用）
    // ===========================
    transformResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));

    // ===========================
    // ⑤ DirectionalLightCB作成（Root[3]）
    // ===========================
    lightResource = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* lightCB = nullptr;
    lightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightCB));

    lightData_.color = { 1, 1, 1, 1 };
    lightData_.direction = { 0.0f, -1.0f, 0.0f };
    lightData_.intensity = 1.0f;

    *lightCB = lightData_;
    lightResource->Unmap(0, nullptr);

    // ===========================
    // ⑥ テクスチャSRVハンドル取得
    // ===========================
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/uvChecker.png");

    indexResource = dxCommon_->CreateBufferResource(sizeof(indexList));

    uint32_t* ibData = nullptr;
    indexResource->Map(0, nullptr, reinterpret_cast<void**>(&ibData));
    memcpy(ibData, indexList, sizeof(indexList));
    indexResource->Unmap(0, nullptr);

    indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView.SizeInBytes = sizeof(indexList);

    // ------------------------------
    // 板ポリ Transform 更新
    // ------------------------------
    {
        TransformationMatrix* transCB = nullptr;
        transformResource->Map(0, nullptr, (void**)&transCB);

        // ▼ IMGUI で調整できる Transform（例：transformBoard_）
        Matrix4x4 world = MatrixMath::MakeAffineMatrix(
            transformBoard_.scale,
            transformBoard_.rotate,
            transformBoard_.translate);

        // ▼ カメラの ViewProjection（Object3d と同じ）
        Matrix4x4 vp = camera_->GetViewProjectionMatrix();

        // ▼ WVP = World × VP
        Matrix4x4 wvp = MatrixMath::Multiply(world, vp);

        // ▼ GPU に送る
        transformData_.World = world;
        transformData_.WVP = wvp;

        *transCB = transformData_;
        transformResource->Unmap(0, nullptr);
    }
}