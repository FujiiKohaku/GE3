#include "ParticleManager.h"
#include "ImGuiManager.h"
#include <cassert>
#include <numbers>
void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera)
{
    // エンジンやカメラを保存しておく
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;

    // パーティクルのランダム生成に使う装置を準備
    randomEngine_ = std::mt19937(seedGenerator_());

    // シェーダーにどうデータを渡すかを決める
    CreateRootSignature();

    // パーティクルの描画方法をまとめた“パイプライン”を作る
    CreateGraphicsPipeline();

    // パーティクルの形となる四角ポリゴンを作る
    CreateBoardMesh();
}

void ParticleManager::Update()
{

    // ビルボード行列
    Matrix4x4 cameraMat = camera_->GetWorldMatrix();
    cameraMat.m[3][0] = cameraMat.m[3][1] = cameraMat.m[3][2] = 0.0f;
    Matrix4x4 billboardMatrix = cameraMat;

    // VP 行列
    Matrix4x4 vp = camera_->GetViewProjectionMatrix();

    // 全グループ処理
    for (auto& [name, group] : particleGroups_) {

        group.numInstance = 0;

        // グループ内パーティクル
        for (auto it = group.particles.begin();
            it != group.particles.end();) {

            Particle& p = *it;

            // 寿命
            if (p.currentTime >= p.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            // 更新
            p.currentTime += kdeltaTime;
            p.transform.translate += p.velocity * kdeltaTime;

            // α
            float alpha = 1.0f - (p.currentTime / p.lifeTime);

            // World 行列
            Matrix4x4 world;
            if (useBillboard_) {
                Matrix4x4 scaleMat = MatrixMath::Matrix4x4MakeScaleMatrix(p.transform.scale);
                Matrix4x4 transMat = MatrixMath::MakeTranslateMatrix(p.transform.translate);

                world = MatrixMath::Multiply(MatrixMath::Multiply(scaleMat, billboardMatrix), transMat);
            } else {
                world = MatrixMath::MakeAffineMatrix(p.transform.scale, p.transform.rotate, p.transform.translate);
            }

            // WVP
            Matrix4x4 wvp = MatrixMath::Multiply(world, vp);

            // GPU へ
            if (group.numInstance < kNumMaxInstance) {
                group.instanceData[group.numInstance].World = world;
                group.instanceData[group.numInstance].WVP = wvp;
                group.instanceData[group.numInstance].color = p.color;
                group.instanceData[group.numInstance].color.w = alpha;
                ++group.numInstance;
            }

            ++it;
        }
    }

    //  ImGui();
}

void ParticleManager::Draw()
{
    auto* cmd = dxCommon_->GetCommandList();

    // マテリアル（共通）
    cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

    // 頂点・インデックス（共通）
    cmd->IASetVertexBuffers(0, 1, &vertexBufferView);
    cmd->IASetIndexBuffer(&indexBufferView);

    // 全パーティクルグループ
    for (auto& [name, group] : particleGroups_) {

        if (group.numInstance == 0) {
            continue;
        }

        // インスタンシング SRV
        cmd->SetGraphicsRootDescriptorTable(1, group.instancingSrvHandleGPU);

        // テクスチャ SRV
        cmd->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(group.texturePath));

        // 描画
        cmd->DrawIndexedInstanced(6, group.numInstance, 0, 0, 0);
    }
}

void ParticleManager::PreDraw()
{
    auto* cmd = dxCommon_->GetCommandList();
    cmd->SetGraphicsRootSignature(rootSignature.Get());
    cmd->SetPipelineState(pipelineStates[currentBlendMode_].Get());
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

    // ====== ラスタライザ設定 ======
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // ====== デプスステンシル設定 ======
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ====== シェーダーのコンパイル ======
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && pixelShaderBlob);

    // ====== PSO設定 ======
    D3D12_GRAPHICS_PIPELINE_STATE_DESC base {};
    base.pRootSignature = rootSignature.Get();
    base.InputLayout = inputLayoutDesc;
    base.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    base.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    base.RasterizerState = rasterizerDesc;
    base.DepthStencilState = depthStencilDesc;
    base.NumRenderTargets = 1;
    base.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    base.DepthStencilState = depthStencilDesc;
    base.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    base.SampleDesc.Count = 1;
    base.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    base.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // PSO 配列を作る
    for (int i = 0; i < (int)BlendMode::kCountOfBlendMode; i++) {

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = base;
        desc.BlendState = CreateBlendDesc((BlendMode)i);

        dxCommon_->GetDevice()->CreateGraphicsPipelineState(
            &desc,
            IID_PPV_ARGS(&pipelineStates[i]) // ← ここを直す！
        );
    }
}

void ParticleManager::CreateBoardMesh()
{
    // ===========================
    //  頂点データ作成
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
    //  頂点バッファ作成
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
    // マテリアルCB作成（Root[0]）
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
    //  DirectionalLightCB作成（Root[3]）
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
    //  テクスチャSRVハンドル取得
    // ===========================
    TextureManager::GetInstance()->LoadTexture("resources/circle.png");
    srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU("resources/circle.png");

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

    TransformationMatrix* transCB = nullptr;
    transformResource->Map(0, nullptr, (void**)&transCB);

    //  IMGUI で調整できる Transform（例：transformBoard_）
    Matrix4x4 world = MatrixMath::MakeAffineMatrix(
        transformBoard_.scale,
        transformBoard_.rotate,
        transformBoard_.translate);

    //  カメラの ViewProjection（Object3d と同じ）
    Matrix4x4 vp = camera_->GetViewProjectionMatrix();

    //  WVP = World × VP
    Matrix4x4 wvp = MatrixMath::Multiply(world, vp);

    // GPU に送る
    transformData_.World = world;
    transformData_.WVP = wvp;

    *transCB = transformData_;
    transformResource->Unmap(0, nullptr);
}

void ParticleManager::Finalize()
{
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }

    // -------------------------
    // ParticleGroup ごとに解放
    // -------------------------
    for (auto& [name, group] : particleGroups_) {

        // Unmap
        if (group.instancingResource && group.instanceData) {
            group.instancingResource->Unmap(0, nullptr);
            group.instanceData = nullptr;
        }

        // GPUリソース解放
        group.instancingResource.Reset();
    }

    // グループ自体をクリア
    particleGroups_.clear();

    // -------------------------
    // 共通リソース解放
    // -------------------------
    materialResource.Reset();
    transformResource.Reset();
    lightResource.Reset();
    vertexResource.Reset();
    indexResource.Reset();

    rootSignature.Reset();
    for (int i = 0; i < kCountOfBlendMode; i++) {
        pipelineStates[i].Reset();
    }

    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    camera_ = nullptr;
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
    // 重複チェック
    assert(particleGroups_.find(name) == particleGroups_.end());

    ParticleGroup group {};

    //  テクスチャ設定
    group.texturePath = textureFilePath;
    TextureManager::GetInstance()->LoadTexture(textureFilePath);

    // インスタンシングバッファ作成
    group.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

    group.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instanceData));

    group.numInstance = 0;

    //  SRV 作成
    uint32_t srvIndex = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kNumMaxInstance;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    dxCommon_->GetDevice()->CreateShaderResourceView(group.instancingResource.Get(), &srvDesc, srvManager_->GetCPUDescriptorHandle(srvIndex));

    group.instancingSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(srvIndex);

    //  登録
    particleGroups_.emplace(name, std::move(group));
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    auto& group = particleGroups_.at(name);

    for (uint32_t i = 0; i < count; ++i) {
        group.particles.push_back(MakeParticleDefault(position));
    }
}

void ParticleManager::EmitFire(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end());

    ParticleGroup& group = it->second;

    std::uniform_real_distribution<float> distXZ(-0.1f, 0.1f);
    std::uniform_real_distribution<float> distUp(0.3f, 0.6f);
    std::uniform_real_distribution<float> distLife(0.5f, 1.0f);

    for (uint32_t i = 0; i < count; ++i) {

        Particle p {};

        // 炎の根元
        p.transform.translate = {
            position.x + distXZ(randomEngine_),
            position.y,
            position.z + distXZ(randomEngine_)
        };

        // 縦長
        float s = 0.1f;
        p.transform.scale = { s * 0.5f, s * 2.0f, s * 0.5f };

        // 上昇
        p.velocity = {
            distXZ(randomEngine_) * 0.1f,
            distUp(randomEngine_),
            distXZ(randomEngine_) * 0.1f
        };

        // 赤→黄
        p.color = { 1.0f, 0.4f, 0.0f, 1.0f };

        // 短命
        p.lifeTime = distLife(randomEngine_);
        p.currentTime = 0.0f;

        group.particles.push_back(p);
    }
}

ParticleManager::Particle
ParticleManager::MakeParticleDefault(const Vector3& pos)
{
    Particle p {};

    // -----------------------------
    // 乱数
    // -----------------------------
    std::uniform_real_distribution<float> offset(-0.05f, 0.05f); // にじみ
    std::uniform_real_distribution<float> dir(-1.0f, 1.0f); // 方向
    std::uniform_real_distribution<float> speed(1.0f, 1.5f); // 広がり強さ
    std::uniform_real_distribution<float> life(0.8f, 1.0f);

    // -----------------------------
    // 初期位置（少しだけ散らす）
    // -----------------------------
    p.transform.translate = {
        pos.x + offset(randomEngine_),
        pos.y + offset(randomEngine_),
        pos.z + offset(randomEngine_)
    };

    // -----------------------------
    // スケール
    // -----------------------------
    p.transform.scale = { 0.3f, 0.3f, 0.3f };
    p.transform.rotate = { 0, 0, 0 };

    // -----------------------------
    // 速度（全方向に広がる）
    // -----------------------------
    Vector3 v {
        dir(randomEngine_),
        dir(randomEngine_),
        dir(randomEngine_)
    };

    // 正規化
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f) {
        v.x /= len;
        v.y /= len;
        v.z /= len;
    }

    float s = speed(randomEngine_);
    p.velocity = { v.x * s, v.y * s, v.z * s };

    // -----------------------------
    // 色
    // -----------------------------
    p.color = { 1.0f, 1.0f, 1.0f, 1.0f };

    // -----------------------------
    // 寿命
    // -----------------------------
    p.lifeTime = life(randomEngine_);
    p.currentTime = 0.0f;

    return p;
}
