#include "ParticleManager.h"
#include "TextureManager.h"
void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // 乱数生成エンジンの初期化
    std::random_device romdum;
    randomEngine_.seed(romdum());
    // グラフィックスパイプラインの生成
    // ルートシグネチャ → パイプライン
    CreateRootSignature();
    CreateGraphicsPipeline();
    // 頂点バッファの生成
     CreateVertexBuffer();
}

void ParticleManager::Update()
{
    // ========================
    // ビルボード行列（カメラを向くため）
    // ※ここでは一旦単位行列にしておく（後でカメラ対応可）
    // ========================
    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();

    // ========================
    // 全てのパーティクルグループを処理
    // ========================
    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;

        // インスタンス数を初期化
        group.instanceCount = 0;

        // ========================
        // 各パーティクルを更新
        // ========================
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            Particle& particle = *it;

            // 時間を進める
            particle.currentTime += 1.0f / 60.0f; // 1フレームごと（60fps前提）

            // 寿命チェック：生存時間を超えたら削除
            if (particle.currentTime >= particle.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            // ------------------------
            // 加速・移動処理
            // ------------------------
            Vector3 acceleration = { 0.0f, -0.01f, 0.0f }; // 重力的な加速
            particle.velocity = particle.velocity + acceleration;
            particle.position = particle.position + particle.velocity;

            // ------------------------
            // 行列計算
            // ------------------------
            Vector3 scale = { 1.0f, 1.0f, 1.0f };
            Vector3 rotate = { 0.0f, 0.0f, 0.0f };

            // 平行移動・拡縮の行列を合成
            Matrix4x4 world = MatrixMath::MakeAffineMatrix(scale, rotate, particle.position);
            Matrix4x4 worldBillboard = MatrixMath::Multiply(world, billboardMatrix);

            // ------------------------
            // インスタンシング用データに書き込み
            // ------------------------
            group.instancingData[group.instanceCount].matWorld = worldBillboard;
            group.instancingData[group.instanceCount].color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白・不透明
            group.instanceCount++;

            ++it;
        }
    }
}

void ParticleManager::Draw()
{
    // コマンドリストを取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // === SRVヒープ設定（これが抜けると不安定になる） ===
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // ルートシグネチャを設定
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    // パイプラインステートを設定
    commandList->SetPipelineState(graphicsPipelineState.Get());

    // プリミティブトポロジ（頂点のつなぎ方）を設定
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファビューを設定
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // ==============================
    // 全てのパーティクルグループを描画
    // ==============================
    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;

        if (group.particles.empty())
            continue;

        // テクスチャSRVをセット（RootParameter[1]）
        srvManager_->SetGraphicsRootDescriptorTable(1, group.textureSrvIndex);

        // インスタンシング用SRVをセット（RootParameter[2]）
        srvManager_->SetGraphicsRootDescriptorTable(2, group.instancingSrvIndex);

        // ==========================
        // 描画コマンド発行（インスタンシング描画）
        // ==========================
        commandList->DrawIndexedInstanced(6,  group.instanceCount,  0, 0, 0);
    }

    // ★ここでバリアは不要★
    // （PostDrawがやるので、ここでは書かないこと）
}



void ParticleManager::CreateRootSignature()
{
    HRESULT hr;

    // === ルートパラメータ3つに変更 ===
    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // [0] Transform（CBV）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] テクスチャ（SRV）
    D3D12_DESCRIPTOR_RANGE texRange = {};
    texRange.BaseShaderRegister = 0;
    texRange.NumDescriptors = 1;
    texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &texRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [2] インスタンシング用SRV
    D3D12_DESCRIPTOR_RANGE instRange = {};
    instRange.BaseShaderRegister = 1;
    instRange.NumDescriptors = 1;
    instRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    instRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &instRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // サンプラー設定
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグネチャ作成
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob, &errorBlob);
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
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));
}


void ParticleManager::CreateGraphicsPipeline()
{

    HRESULT hr;

    // ------------------------------
    // 入力レイアウト（頂点構造）
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};

    // POSITION
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = 0; // 最初の要素は 0
    inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    // TEXCOORD
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    // 登録
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // ------------------------------
    // ブレンド設定（透明度の扱い）
    // ------------------------------
    D3D12_BLEND_DESC blendDesc {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    // ------------------------------
    // ラスタライザ設定（面の塗り方やカリング）
    // ------------------------------
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 両面描画
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 塗りつぶし描画

    // ------------------------------
    // 深度ステンシル（Zバッファ）無効
    // ------------------------------
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    // ------------------------------
    // シェーダーコンパイル
    // ------------------------------
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && pixelShaderBlob);

    // ------------------------------
    // PSO（パイプラインステートオブジェクト）設定
    // ------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // ------------------------------
    // PSO作成
    // ------------------------------
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateVertexBuffer()
{
    //  頂点リソース作成（4頂点分）
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kVertexCount);

    //  インデックスリソース作成（6インデックス分）
    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * 6);

    //  頂点バッファビュー設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * kVertexCount);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    //  インデックスバッファビュー設定
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    //  GPU書き込み用のアドレスを取得（Map）
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

    //  頂点データ
    vertexData_[0] = { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } };
    vertexData_[1] = { { -0.5f, +0.5f, 0.0f }, { 0.0f, 0.0f } };
    vertexData_[2] = { { +0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } };
    vertexData_[3] = { { +0.5f, +0.5f, 0.0f }, { 1.0f, 0.0f } };

    //  インデックスデータ
    indexData_[0] = 0;
    indexData_[1] = 1;
    indexData_[2] = 2;
    indexData_[3] = 2;
    indexData_[4] = 1;
    indexData_[5] = 3;
}


void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath)
{
    // すでに同名グループが存在しないことを確認
    assert(particleGroups_.find(name) == particleGroups_.end());

    ParticleGroup group {};
    group.textureFilePath = textureFilePath;

    // -------------------------
    // TextureManager 経由で読み込み（登録のみ）
    // -------------------------
    TextureManager::GetInstance()->LoadTexture(textureFilePath);

    // 読み込んだテクスチャ情報を取得
    const TextureManager::TextureData* textureData = TextureManager::GetInstance()->GetTextureData(textureFilePath);
    assert(textureData); // 取得失敗チェック

    ID3D12Resource* texResource = textureData->resource.Get();

    // SRVをSrvManagerで作成
    group.textureSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D( group.textureSrvIndex,  texResource,textureData->metadata.format, static_cast<UINT>(textureData->metadata.mipLevels));

    // -------------------------
    // インスタンシング用リソース生成
    // -------------------------
    const uint32_t kMaxInstance = 1024;
    group.instancingResource = dxCommon_->CreateBufferResource(sizeof(InstancingData) * kMaxInstance);
    group.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instancingData));

    group.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer( group.instancingSrvIndex, group.instancingResource.Get(), kMaxInstance, sizeof(InstancingData));

    // -------------------------
    // 登録
    // -------------------------
    particleGroups_.insert({ name, group });
}

void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count)
{
    // 1. グループ名の確認
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end()); // 存在しなければ停止

    // 2. 指定回数分パーティクルを発生
    for (uint32_t i = 0; i < count; i++) {
        Particle particle {};

        // 初期位置
        particle.position = position;

        // 速度をランダムに決める（例：上下左右に少しばらつかせる）
        std::uniform_real_distribution<float> velDist(-0.1f, 0.1f);
        particle.velocity = { velDist(randomEngine_), velDist(randomEngine_), velDist(randomEngine_) };

        // 寿命と経過時間
        particle.lifeTime = 60.0f; // 例：60フレーム
        particle.currentTime = 0.0f;

        // グループに登録
        it->second.particles.push_back(particle);
    }
}
// シングルトン用の静的インスタンス
ParticleManager* ParticleManager::instance = nullptr;

// インスタンス取得関数の定義
ParticleManager* ParticleManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ParticleManager();
    }
    return instance;
}
// ParticleManager.cpp に追加
void ParticleManager::Finalize()
{
    particleGroups_.clear();
    rootSignature.Reset();
    graphicsPipelineState.Reset();
    vertexResource_.Reset();
    indexResource_.Reset();

    // シングルトンインスタンス解放
    instance = nullptr;
}
