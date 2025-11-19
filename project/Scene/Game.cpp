#include "Game.h"
#include <numbers>

void Game::Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager)
{
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
#pragma region Sprite関連
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });
#pragma endregion

#pragma region 3D関連
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    object3dManager_->SetDefaultCamera(camera_);

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    ModelManager::GetInstance()->LoadModel("fence.obj");
    player2_.Initialize(object3dManager_);
    player2_.SetModel("fence.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 0.0f });
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });
#pragma endregion

#pragma region 入力関連
    input_ = new Input();
    input_->Initialize(winApp_);
#pragma endregion

#pragma region サウンド関連
    soundManager_.Initialize();
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");
#pragma endregion

#pragma region パーティクル関連

#pragma endregion
#pragma region 三角形作成

    // ===========================
    // ① 頂点データ作成
    // ===========================
    VertexData vertices[4] {};

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

#pragma endregion
}

void Game::Update()
{
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



    input_->Update();
    player2_.Update();
    camera_->Update();
    sprite_->Update();

    camera_->DebugUpdate();
#ifdef USE_IMGUI

    // 現在の座標を取得
    Vector2 pos = sprite_->GetPosition();
    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_Always);
    // 実数4桁・小数1桁で表示
    // ImGui::SliderFloat2("Position", (float*)&pos, 0.0f, 500.0f, "%.1f");
    // 変更を反映
    sprite_->SetPosition(pos);
#endif

#ifdef USE_IMGUI
    ImGui::Begin("Player2 Debug");

    // 1. Transform（位置・回転・スケール）
    {
        Vector3 pos = player2_.GetTranslate();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
            player2_.SetTranslate(pos);
        }

        Vector3 rot = player2_.GetRotate();
        if (ImGui::DragFloat3("Rotate", &rot.x, 0.01f)) {
            player2_.SetRotate(rot);
        }

        Vector3 scale = player2_.GetScale();
        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
            player2_.SetScale(scale);
        }
    }

    // 2. ライト調整
    {
        Object3d::DirectionalLight* light = player2_.GetLight();
        Object3d::Material* material = player2_.GetMaterial();

        ImGui::Separator();
        ImGui::Text("Directional Light");

        ImGui::ColorEdit3("Light Color", &light->color.x);
        ImGui::SliderFloat3("Light Direction", &light->direction.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Alpha", &material->color.w, 0.0f, 1.0f);

        //  方向ベクトルの正規化
        light->direction = Normalize(light->direction);
    }
    ImGui::Separator();
    ImGui::Text("Blend Mode");

    // 現在のモードを取得
    int blendMode = object3dManager_->GetBlendMode();

    // 選択肢リスト
    const char* blendNames[] = {
        "None",
        "Normal ",
        "Add",
        "Subtract ",
        "Multiply",
        "Screen "
    };

    // ドロップダウン
    if (ImGui::Combo("Blend", &blendMode, blendNames, IM_ARRAYSIZE(blendNames))) {
        object3dManager_->SetBlendMode(static_cast<BlendMode>(blendMode));
    }

    ImGui::End();
    ImGui::Begin("Board Debug");

    ImGui::DragFloat3("Board Pos", &transformBoard_.translate.x, 0.1f);
    ImGui::DragFloat3("Board Rot", &transformBoard_.rotate.x, 0.01f);
    ImGui::DragFloat3("Board Scale", &transformBoard_.scale.x, 0.01f);

    ImGui::End();
#endif
}

void Game::Draw()
{
    // ① 3D描画ブロック
    {
        object3dManager_->PreDraw();
        auto* cmd = dxCommon_->GetCommandList();
        // Root[0] : Material（b0, PS）
        cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
        // Root[1] : Transform（b0, VS）
        cmd->SetGraphicsRootConstantBufferView(1, transformResource->GetGPUVirtualAddress());
        // Root[2] : Texture (t0, PS)
        cmd->SetGraphicsRootDescriptorTable(2, srvHandle);
        // Root[3] : DirectionalLight（b1, PS）
        cmd->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
        cmd->IASetIndexBuffer(&indexBufferView);
        cmd->IASetVertexBuffers(0, 1, &vertexBufferView);

        cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);

        player2_.Draw();
    }

    // ② 2D描画ブロック
    {
        spriteManager_->PreDraw();
        // sprite_->Draw();
    }
}

void Game::Finalize()
{
    delete object3dManager_;
    delete spriteManager_;
    delete sprite_;
    delete camera_;
    delete input_;

    ModelManager::GetInstance()->Finalize();
    soundManager_.Finalize(&bgm);
}
