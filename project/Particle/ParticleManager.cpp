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

    // パーティクル100個ぶんのデータを入れる箱を作る
    CreateInstancingBuffer();

    // その箱をシェーダーから読めるように登録する
    CreateSrvBuffer();

    // パーティクルの形となる四角ポリゴンを作る
    CreateBoardMesh();

    emitter.count = 10; // 1回のEmitで5つ出す
    emitter.frequency = 0.01f; // 毎フレーム出す (60FPS)
    kdeltaTime = 0.1f;
    emitter.transform.translate = { 0.0f, 0.0f, 0.0f };
    emitter.transform.rotate = { 0.0f, 0.0f, 0.0f };
    emitter.transform.scale = { 1.0f, 1.0f, 1.0f };
}

void ParticleManager::Update()
{
    emitter.frequencyTime += kdeltaTime;

    if (emitter.frequencyTime >= emitter.frequency) {

        switch (type) {

        case ParticleType::Normal:

            particles.splice(
                particles.end(),
                Emit(emitter, randomEngine_));
            break;

        case ParticleType::Fire:

            particles.splice(
                particles.end(),
                EmitFire(emitter, randomEngine_));
            break;
        case ParticleType::Smoke:

            particles.splice(
                particles.end(),
                EmitSmoke(emitter, randomEngine_));
            break;
        case ParticleType::Spark:

            particles.splice(
                particles.end(),
                EmitLightning(emitter, randomEngine_));
            break;
        case ParticleType::FireWork:
            particles.splice(
                particles.end(),
                EmitFireworkSpark(emitter, randomEngine_));
            break;
        }

        emitter.frequencyTime -= emitter.frequency;
    }

    UpdateTransforms();
    ImGui();
}

void ParticleManager::Draw()
{
    // 描くパーティクルが1つもないなら何もしない
    if (numInstance_ == 0)
        return;

    auto* cmd = dxCommon_->GetCommandList();

    // 色や明るさなどのマテリアル情報
    cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

    // パーティクルたちの位置や色のまとめデータ
    cmd->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

    // 四角に貼るテクスチャ
    cmd->SetGraphicsRootDescriptorTable(2, srvHandle);

    // 四角ポリゴンの形（頂点）
    cmd->IASetVertexBuffers(0, 1, &vertexBufferView);

    // 四角ポリゴンのつなぎ順（インデックス）
    cmd->IASetIndexBuffer(&indexBufferView);

    // 四角 × パーティクル数ぶん描く
    cmd->DrawIndexedInstanced(6, numInstance_, 0, 0, 0);
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

void ParticleManager::CreateInstancingBuffer()
{
    // GPUバッファを作成
    instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

    // 初期化

    instancingResource->Map(0, nullptr, (void**)&instanceData_);

    for (uint32_t i = 0; i < kNumMaxInstance; i++) {
        instanceData_[i].WVP = MatrixMath::MakeIdentity4x4();
        instanceData_[i].World = MatrixMath::MakeIdentity4x4();
        instanceData_[i].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void ParticleManager::CreateSrvBuffer()
{
    uint32_t index = srvManager_->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc {};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.NumElements = kNumMaxInstance;
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    auto handleCPU = srvManager_->GetCPUDescriptorHandle(index);
    auto handleGPU = srvManager_->GetGPUDescriptorHandle(index);

    dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, handleCPU);

    instancingSrvHandleGPU_ = handleGPU;
}

void ParticleManager::UpdateTransforms()
{
    // ビルボード用：カメラの回転だけ取り出す
    Matrix4x4 cameraMat = camera_->GetWorldMatrix();
    cameraMat.m[3][0] = cameraMat.m[3][1] = cameraMat.m[3][2] = 0.0f;

    Matrix4x4 billboardMatrix = MatrixMath::Multiply(MatrixMath::MakeRotateYMatrix(0.0f), cameraMat);

    Matrix4x4 vp = camera_->GetViewProjectionMatrix();

    numInstance_ = 0; // GPU へ送る個数をリセット

    // --- 全パーティクル更新 ---
    for (auto it = particles.begin(); it != particles.end();) {
        Particle& p = *it;

        // 寿命切れ → 削除
        if (p.currentTime >= p.lifeTime) {
            it = particles.erase(it);
            continue;
        }

        // 生存中：時間と位置を更新
        p.currentTime += kdeltaTime;
        p.transform.translate += p.velocity * kdeltaTime;

        // フェードアウト用 α（0〜1）
        float alpha = 1.0f - (p.currentTime / p.lifeTime);

        // 行列作成（スケール・位置）
        Matrix4x4 scaleMat = MatrixMath::Matrix4x4MakeScaleMatrix(p.transform.scale);
        Matrix4x4 transMat = MatrixMath::MakeTranslateMatrix(p.transform.translate);

        // ワールド行列（ビルボードか普通か）
        Matrix4x4 world;
        if (useBillboard_) {
            world = MatrixMath::Multiply(MatrixMath::Multiply(scaleMat, billboardMatrix), transMat);
        } else {
            world = MatrixMath::MakeAffineMatrix(p.transform.scale, p.transform.rotate, p.transform.translate);
        }

        // WVP 行列
        Matrix4x4 wvp = MatrixMath::Multiply(world, vp);

        // GPU へ書き込み
        if (numInstance_ < kNumMaxInstance) {
            instanceData_[numInstance_].World = world;
            instanceData_[numInstance_].WVP = wvp;
            instanceData_[numInstance_].color = p.color;
            instanceData_[numInstance_].color.w = alpha; // 透明度だけ更新
            ++numInstance_;
        }

        ++it; // 次へ
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

void ParticleManager::ImGui()
{
    if (ImGui::Begin("Particle Manager")) {

        // ======================
        // パーティクルタイプ
        // ======================
        const char* particleTypeNames[] = {
            "Normal",
            "Fire",
            "Smoke",
            "Lightning",
            "FireWork"
        };

        int selected = static_cast<int>(type);
        if (ImGui::Combo("Particle Type", &selected, particleTypeNames, IM_ARRAYSIZE(particleTypeNames))) {
            type = static_cast<ParticleType>(selected);
        }

        ImGui::Separator();

        // ======================
        // Emitter Settings
        // ======================
        ImGui::Text("Emitter Settings");

        ImGui::DragFloat3("Position", &emitter.transform.translate.x, 0.01f);

        ImGui::DragInt("Count", (int*)&emitter.count, 1, 1, 100);

        ImGui::DragFloat("Frequency", &emitter.frequency, 0.01f, 0.001f, 1.0f);

        ImGui::Separator();

        // ======================
        // Blend Mode
        // ======================
        const char* blendNames[] = {
            "None", "Normal", "Add", "Subtract", "Multiply", "Screen"
        };

        int mode = currentBlendMode_;
        if (ImGui::Combo("Blend Mode", &mode, blendNames, IM_ARRAYSIZE(blendNames))) {
            currentBlendMode_ = mode;
        }

        ImGui::Checkbox("Use Billboard", &useBillboard_);
    }

    ImGui::End();
}

void ParticleManager::Finalize()
{
    // GPU待機（リソースが消せない場合があるため）
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }

    // Unmap
    if (instancingResource && instanceData_) {
        instancingResource->Unmap(0, nullptr);
        instanceData_ = nullptr;
    }

    // GPUリソース解放
    instancingResource.Reset();
    materialResource.Reset();
    transformResource.Reset();
    lightResource.Reset();
    vertexResource.Reset();
    indexResource.Reset();

    // RootSignature / PSO 解放
    rootSignature.Reset();
    for (int i = 0; i < kCountOfBlendMode; i++) {
        pipelineStates[i].Reset();
    }

    // 最後にGPU排出
    if (dxCommon_) {
        dxCommon_->WaitForGPU();
    }

    // 依存先も無効化
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    camera_ = nullptr;
}

ParticleManager::Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& emitPos)
{
    Particle p;

    // -----------------------------------------
    // 内部パラメータ
    // -----------------------------------------
    float spread = 0.5f; // 広がり
    float speed = 0.05f; // 飛び散る速さ
    float scaleMin = 0.05f; // 最小サイズ
    float scaleMax = 0.15f; // 最大サイズ
    float lifeMin = 0.5f; // 最短寿命
    float lifeMax = 1.5f; // 最長寿命

    // ランダム
    std::uniform_real_distribution<float> distPos(-spread, spread);
    std::uniform_real_distribution<float> distScale(scaleMin, scaleMax);
    std::uniform_real_distribution<float> distLife(lifeMin, lifeMax);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    //  初期位置
    p.transform.translate = {
        emitPos.x + distPos(randomEngine),
        emitPos.y + distPos(randomEngine),
        emitPos.z + distPos(randomEngine)
    };

    //  スケール
    float s = distScale(randomEngine);
    p.transform.scale = { s, s, s };
    p.transform.rotate = { 0, 0, 0 };

    //  速度
    p.velocity = {
        distPos(randomEngine) * speed,
        distPos(randomEngine) * speed,
        distPos(randomEngine) * speed
    };

    //  色
    p.color = {
        dist01(randomEngine),
        dist01(randomEngine),
        dist01(randomEngine),
        1.0f
    };

    //  寿命
    p.lifeTime = distLife(randomEngine);
    p.currentTime = 0.0f;

    return p;
}

ParticleManager::Particle ParticleManager::MakeNewParticleSmoke(std::mt19937& randomEngine, const Vector3& pos)
{
    Particle p;

    // --- ランダム ---
    std::uniform_real_distribution<float> distXZ(-0.25f, 0.25f); // 横は控えめ
    std::uniform_real_distribution<float> distUp(0.02f, 0.06f); // 上昇力
    std::uniform_real_distribution<float> distScale(0.15f, 0.35f); // 大きめ粒
    std::uniform_real_distribution<float> distLife(1.0f, 2.5f); // 長めの寿命
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float dx = distXZ(randomEngine);
    float dz = distXZ(randomEngine);

    p.transform.translate = {
        pos.x + dx * 0.4f,
        pos.y,
        pos.z + dz * 0.4f
    };

    float s = distScale(randomEngine);
    p.transform.scale = { s, s, s };
    p.transform.rotate = { 0, 0, 0 };

    p.velocity = {
        dx * 0.15f, // 横ブレ
        distUp(randomEngine), // 上昇
        dz * 0.15f
    };

    float t = dist01(randomEngine);

    Vector4 bottom = { 0.9f, 0.9f, 0.9f, 0.8f };

    Vector4 top = { 0.2f, 0.2f, 0.2f, 0.4f };

    p.color = {
        bottom.x + (top.x - bottom.x) * t,
        bottom.y + (top.y - bottom.y) * t,
        bottom.z + (top.z - bottom.z) * t,
        bottom.w + (top.w - bottom.w) * t
    };

    p.lifeTime = distLife(randomEngine);
    p.currentTime = 0.0f;

    return p;
}

ParticleManager::Particle ParticleManager::MakeNewParticleFire(std::mt19937& randomEngine, const Vector3& pos)
{
    Particle p;

    // ---------------------------
    // ランダム生成
    // ---------------------------
    std::uniform_real_distribution<float> distXZ(-0.15f, 0.15f);
    std::uniform_real_distribution<float> distUp(0.3f, 0.6f);
    std::uniform_real_distribution<float> distScale(0.1f, 0.25f);
    std::uniform_real_distribution<float> distLife(0.5f, 1.0f);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float dx = distXZ(randomEngine);
    float dz = distXZ(randomEngine);

    p.transform.translate = {
        pos.x + dx * 0.4f,
        pos.y,
        pos.z + dz * 0.4f
    };

    float s = distScale(randomEngine);
    p.transform.scale = { s, s * 2.5f, s };
    p.transform.rotate = { 0, 0, 0 };

    p.velocity = {
        dx * 0.05f,
        -distUp(randomEngine),
        dz * 0.05f
    };

    float a = dist01(randomEngine);
    float t = dist01(randomEngine);
    t = t * t;

    Vector4 red = { 1.0f, 0.05f, 0.0f, 1.0f };
    Vector4 orange = { 1.0f, 0.25f, 0.0f, 1.0f };
    Vector4 yellow = { 1.0f, 0.85f, 0.1f, 1.0f };
    Vector4 white = { 1.0f, 1.0f, 0.9f, 1.0f };

    Vector4 c1 = (a < 0.33f) ? red : (a < 0.66f) ? orange
                                                 : yellow;

    float blend = t * 2.0f;

    if (blend > 1.0f) {
        blend = 1.0f;
    }

    p.color = {
        c1.x + (white.x - c1.x) * blend,
        c1.y + (white.y - c1.y) * blend,
        c1.z + (white.z - c1.z) * blend,
        1.0f
    };

    p.lifeTime = distLife(randomEngine);
    p.currentTime = 0.0f;

    return p;
}

ParticleManager::Particle ParticleManager::MakeNewParticleLightning(std::mt19937& randomEngine, const Vector3& pos)
{
    Particle p;

    // --- ランダム ---
    std::uniform_real_distribution<float> distXZ(-0.15f, 0.15f); // 横ぶれ
    std::uniform_real_distribution<float> distLength(1.2f, 2.5f);
    std::uniform_real_distribution<float> distThickness(0.02f, 0.07f);
    std::uniform_real_distribution<float> distLife(0.05f, 0.12f);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float dx = distXZ(randomEngine);
    float dz = distXZ(randomEngine);

    p.transform.translate = {
        pos.x + dx,
        pos.y,
        pos.z + dz
    };

    float len = distLength(randomEngine);
    float th = distThickness(randomEngine);
    p.transform.scale = { th, len, th };

    p.transform.rotate = { 0, 0, 0 };

    float vx = distXZ(randomEngine) * 0.2f;
    float vz = distXZ(randomEngine) * 0.2f;

    p.velocity = {
        vx,
        -0.3f, // 下に落ちる雷
        vz
    };

    float t = dist01(randomEngine);

    Vector4 inner = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（中心）
    Vector4 outer = { 0.3f, 0.5f, 1.0f, 0.6f }; // 青白い

    p.color = {
        inner.x + (outer.x - inner.x) * t,
        inner.y + (outer.y - inner.y) * t,
        inner.z + (outer.z - inner.z) * t,
        inner.w + (outer.w - inner.w) * t
    };

    p.lifeTime = distLife(randomEngine);
    p.currentTime = 0.0f;

    return p;
}

ParticleManager::Particle ParticleManager::MakeFireworkSpark(std::mt19937& randomEngine, const Vector3& center)
{
    Particle p;

    std::uniform_real_distribution<float> distAngle(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distRadius(0.02f, 0.12f); // 中心かなり近め
    std::uniform_real_distribution<float> distLife(0.4f, 0.9f);
    std::uniform_real_distribution<float> distSpeed(2.0f, 4.0f); // 強めの飛び散り
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float angle = distAngle(randomEngine);
    float radius = distRadius(randomEngine);

    // 小さくバラける中心スタート
    float offsetX = cosf(angle) * radius;
    float offsetY = sinf(angle) * radius;

    p.transform.translate = {
        center.x + offsetX,
        center.y + offsetY,
        center.z
    };

    float s = 0.1f;
    p.transform.scale = {
        s * 0.25f,
        s * 1.8f,
        s * 0.25f
    };
    p.transform.rotate = { 0, 0, 0 };

    Vector3 dir = { offsetX, offsetY, 0.0f };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);

    if (len > 0.00001f) {
        dir.x /= len;
        dir.y /= len;
    }

    float speed = distSpeed(randomEngine);

    p.velocity = { dir.x * speed, dir.y * speed, 0.0f };

    Vector4 corePink = { 1.0f, 0.4f, 0.7f, 1.0f };

    Vector4 outside;

    float c = dist01(randomEngine);
    if (c < 0.25f)
        outside = { 1.0f, 0.5f, 0.8f, 1.0f };
    else if (c < 0.50f)
        outside = { 0.4f, 0.6f, 1.0f, 1.0f };
    else if (c < 0.75f)
        outside = { 1.0f, 0.9f, 0.4f, 1.0f };
    else
        outside = { 0.9f, 0.4f, 0.1f, 1.0f };

    // 補間値
    float t = dist01(randomEngine);

    // 色補間（中心ピンク → 外側色）
    p.color = {
        corePink.x + (outside.x - corePink.x) * t,
        corePink.y + (outside.y - corePink.y) * t,
        corePink.z + (outside.z - corePink.z) * t,
        1.0f
    };

    // ================================
    //  寿命
    // ================================
    p.lifeTime = distLife(randomEngine);
    p.currentTime = 0.0f;

    return p;
}

// エミッターから粒を生む（count 個ぶん生成して返す）
std::list<ParticleManager::Particle> ParticleManager::Emit(const Emitter& emitter, std::mt19937& randomEngine)
{
    std::list<Particle> particles; // 生成した粒を入れる箱

    // emitter.count 回ぶん粒を作る
    for (uint32_t count = 0; count < emitter.count; ++count) {

        // 粒を1つ作って、エミッター位置から生やす
        particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
    }

    return particles; // 作った粒をまとめて返す
}

std::list<ParticleManager::Particle> ParticleManager::EmitFire(const Emitter& emitter, std::mt19937& randomEngine)
{
    std::list<Particle> particles;

    for (uint32_t i = 0; i < emitter.count; ++i) {

        float r = (float)(randomEngine() % 100) / 100.0f;

        if (r < 0.5f) {
            // 50% → 炎の芯 (Core)
            particles.push_back(MakeNewParticleFire(randomEngine, emitter.transform.translate));
        }
    }

    return particles;
}

std::list<ParticleManager::Particle> ParticleManager::EmitSmoke(const Emitter& emitter, std::mt19937& randomEngine)
{
    std::list<Particle> particles;

    for (uint32_t i = 0; i < emitter.count; ++i) {
        particles.push_back(MakeNewParticleSmoke(randomEngine, emitter.transform.translate));
    }

    return particles;
}

std::list<ParticleManager::Particle> ParticleManager::EmitLightning(const Emitter& emitter, std::mt19937& randomEngine)
{
    std::list<Particle> particles;

    for (uint32_t i = 0; i < emitter.count; ++i) {
        particles.push_back(MakeNewParticleLightning(randomEngine, emitter.transform.translate));
    }

    return particles;
}

std::list<ParticleManager::Particle> ParticleManager::EmitFireworkSpark(const Emitter& emitter, std::mt19937& randomEngine)
{
    std::list<Particle> particles;

    for (uint32_t i = 0; i < emitter.count; ++i) {
        particles.push_back(MakeFireworkSpark(randomEngine, emitter.transform.translate));
    }

    return particles;
}