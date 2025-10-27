#include "Game.h"
#include <numbers>
void Game::Initialize()
{

    // 誰も補足しなかった場合(Unhandled),補足する関数を登録
    // main関数はじまってすぐに登録するとよい
    SetUnhandledExceptionFilter(Utility::ExportDump);
    // ログのディレクトリを用意
    std::filesystem::create_directory("logs");
    // main関数の先頭//

#pragma region object説明書
    // ===============================
    // 3Dモデルとオブジェクトの関係まとめ
    // ===============================
    //
    // Model（モデル）
    //   ・見た目のデータ（形・テクスチャ）を管理
    //   ・同じモデルを複数のオブジェクトで使い回せる
    //
    // Object3d（オブジェクト）
    //   ・実際に表示される実体
    //   ・位置・回転・スケールを持つ
    //   ・どのModelを使うかを指定して描画する
    //
    // ModelCommon / Object3dManager
    //   ・共通の描画設定やパイプラインを管理
    //
    //  使用手順（操作説明）
    // 1. Modelを作る → model.Initialize(&modelCommon);
    // 2. Object3dを作る → object.Initialize(manager, camera);
    // 3. Object3dにModelをセット → object.SetModel(&model);
    // 4. 必要に応じて位置や回転を変更 → object.SetTranslate({x, y, z});
    // 5. 毎フレーム Update() → Draw() で描画
    //
    // 例：
    // Model modelPlayer;
    // Object3d player;
    // player.SetModel(&modelPlayer);
    // player.SetTranslate({3, 0, 0});
    // player.Draw();
    //
    // ===============================

#pragma endregion

#pragma region sprite説明書

    // ===============================
    // 2Dスプライト関連まとめ
    // ===============================
    //
    //  TextureManager（シングルトン）
    //   ・全テクスチャを一括で管理するクラス
    //   ・同じ画像を何度も読み込まないようにする
    //   ・Initialize(dxCommon)でDirectX情報を登録
    //   ・LoadTexture("path")で画像をGPUに読み込み
    //
    // SpriteManager
    //   ・スプライト描画用の共通設定を管理
    //   ・複数のSpriteをまとめて扱う
    //
    //  Sprite
    //   ・実際に画面に表示する2D画像
    //   ・1枚ごとに位置・回転・スケールを持つ
    //
    // 使用手順
    // 1. TextureManagerを初期化して画像を読み込む
    // 2. SpriteManagerを初期化する
    // 3. Spriteを生成して画像パスを指定して初期化
    // 4. Update() → Draw() で描画する
    //
    //  例：スプライトを5枚生成
    //   std::vector<Sprite*> sprites;
    //   for (int i = 0; i < 5; i++) {
    //       Sprite* sprite = new Sprite();
    //       sprite->Initialize(spriteManager, "resources/uvChecker.png");
    //       sprites.push_back(sprite);
    //   }
    //
    // ===============================

#pragma endregion

#pragma region object sprite

    // =============================
    // 1. 基本システムの初期化
    // =============================
    winApp_ = new WinApp();
    winApp_->initialize();

    dxCommon_ = new DirectXCommon();
    dxCommon_->Initialize(winApp_);

    // =============================
    // 2. テクスチャ・スプライト関係
    // =============================

    // TextureManager（シングルトン）
    TextureManager::GetInstance()->Initialize(dxCommon_);
    TextureManager::GetInstance()->LoadTexture("resources/y.png");

    // SpriteManager
    spriteManager_ = new SpriteManager();
    spriteManager_->Initialize(dxCommon_);
    sprite_ = new Sprite();
    sprite_->Initialize(spriteManager_, "resources/y.png");

    // =============================
    // 3. 3D関連の初期化
    // =============================

    // Object3dManager
    object3dManager_ = new Object3dManager();
    object3dManager_->Initialize(dxCommon_);

    // カメラ
    camera_ = new Camera();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    object3dManager_->SetDefaultCamera(camera_);
    // モデル共通設定

    modelCommon_.Initialize(dxCommon_);

    ModelManager::GetInstance()->initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    // =============================
    // 4. モデルと3Dオブジェクト生成
    // =============================

    //// モデルを生成
    // Model model; // 汎用モデル
    // model.Initialize(&modelCommon);

    // Model modelPlayer; // プレイヤー用モデル
    // modelPlayer.Initialize(&modelCommon);

    // Model modelEnemy; // 敵用モデル
    // modelEnemy.Initialize(&modelCommon);

    // 3Dオブジェクト生成

    // プレイヤー

    player2_.Initialize(object3dManager_);
    player2_.SetModel("titleTex.obj");
    player2_.SetTranslate({ 3.0f, 0.0f, 0.0f }); // 右に移動
    player2_.SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });
    // 敵

#pragma endregion

    //=================================
    // キーボードインスタンス作成
    //=================================

    input_ = new Input();
    //=================================
    // キーボード情報の取得開始
    //=================================
    input_->Initialize(winApp_);

    //=================================
    // サウンドマネージャーインスタンス作成
    //=================================

    // サウンドマネージャー初期化！
    soundManager_.Initialize();
    // サウンドファイルを読み込み（パスはプロジェクトに合わせて調整）
    bgm = soundManager_.SoundLoadWave("Resources/BGM.wav");

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue>
        infoQueue = nullptr;
    if (SUCCEEDED(dxCommon_->GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのＩＤ
        D3D12_MESSAGE_ID denyIds[] = {
            // windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
            // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示wp抑制する
        infoQueue->PushStorageFilter(&filter);
        // 解放
        /*  infoQueue->Release();*/
    }
#endif // DEBUG
}

void Game::Update()
{
    // ==============================
    //  フレームの先頭処理
    // ==============================
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ==============================
    //  開発用UI
    // ==============================

    // ==============================
    // ImGui更新（UI構築）
    // ==============================
    ImGui::Begin("Camera Controller");
    ImGui::SliderFloat3("Translate", &camera_->GetTranslate().x, -50.0f, 50.0f);
    ImGui::SliderFloat3("Rotate", &camera_->GetRotate().x, -3.14f, 3.14f);

    ImGui::End();

    ImGui::Render(); // ImGuiの内部コマンドを生成（描画直前に呼ぶ）

    // ==============================
    //  更新処理（Update）
    // ==============================
    // 入力状態の更新
    input_->Update();

    //============================================

    //============================================

    // static float t = 0.0f;
    // t += 0.5f; // ← 高速で回すのがコツ（0.01じゃ遅すぎ）

    // // 速いsin波で小刻みに揺らす
    // float shake = std::sin(t * 90.0f) * 0.05f; // 周波数60、振幅0.05

    // float baseScale = 1.0f + std::sin(t) * 0.5f;
    //   float scale = baseScale + shake;

    // player2_.SetScale({ scale, scale, scale });

    // if (t > 6.28f)
    //     t = 0.0f;

    // 各3Dオブジェクトの更新

    player2_.Update();

    camera_->Update();


    sprite_->SetAnchorPoint({ 0.5f, 0.5f });
    sprite_->SetPosition({ 300.0f, 200.0f });
    sprite_->SetIsFlipY(true);
    sprite_->Update();
}

void Game::Draw()
{
    // ===== 3D描画 =====
    dxCommon_->PreDraw();
    object3dManager_->PreDraw();
    player2_.Draw();

    // ===== 2D描画（スプライト） =====
    spriteManager_->PreDraw();
    sprite_->Draw();

    // ===== ImGui（最前面） =====
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList());

    // ===== 終了 =====
    dxCommon_->PostDraw();
}

void Game::Finalize()
{
    // 1. 描画系（Object/Sprite）を先に破棄
    delete object3dManager_;
    sprites_.clear();
    delete spriteManager_;
    delete sprite_;
    // 2. ImGuiを破棄（DirectXがまだ生きているうちに）
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // 3. モデルやテクスチャを解放
    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();

    // 4. サウンドを解放
    soundManager_.Finalize(&bgm);

    // 5. DirectX解放
    delete dxCommon_;

    // 6. WinApp解放
    winApp_->Finalize();
    delete winApp_;
}