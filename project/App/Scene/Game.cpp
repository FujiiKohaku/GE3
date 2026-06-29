#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    auto CheckTime = [](const char* name, std::chrono::steady_clock::time_point& prevTime) {
        auto nowTime = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - prevTime).count();

        Logger::Log(std::string(name) + " : " + std::to_string(ms) + "ms");

        prevTime = nowTime;
    };
    auto prevTime = std::chrono::steady_clock::now();
    Logger::Log("Game Initialize Start");
    ShowCursor(FALSE); // カーソルを消す
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");

    WinApp::GetInstance()->initialize();

    LockCursorToWindow();

    CheckTime("WinApp", prevTime);

    DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    CheckTime("DirectXCommon", prevTime);

    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("SrvManager", prevTime);

    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    CheckTime("TextureManager", prevTime);

    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
    CheckTime("ImGuiManager", prevTime);

    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("SpriteManager", prevTime);

    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("ModelManager", prevTime);

    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("Object3dManager", prevTime);

    SkinningObject3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("SkinningObject3dManager", prevTime);

    SkyBoxManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("SkyBoxManager", prevTime);

    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckTime("LightManager", prevTime);

    DebugRenderer::GetInstance()->Initialize();
    CheckTime("DebugRenderer", prevTime);

    modelCommon_.Initialize(DirectXCommon::GetInstance());

    Input::GetInstance()->Initialize(WinApp::GetInstance());

    Logger::Log("Load Default Models");
    // ModelManager::GetInstance()->Load("plane.obj");
    // ModelManager::GetInstance()->Load("axis.obj");
    // ModelManager::GetInstance()->Load("titleTex.obj");
    // ModelManager::GetInstance()->Load("fence.obj");

    Logger::Log("Load Default Textures");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/fence.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/BaseColor_Cube.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/noise0.png");
    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());

    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();

    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(DirectXCommon::GetInstance());

    fog_ = std::make_unique<Fog>();
    fog_->Initialize(DirectXCommon::GetInstance());

    SoundManager::GetInstance()->Initialize();

    Logger::Log("Game Initialize End");
}

void Game::Update()
{
    Input::GetInstance()->Update();

    if (Input::GetInstance()->IsKeyPressed(DIK_P)) {
        isPostEffectEnabled_ = !isPostEffectEnabled_;
    }

if (Input::GetInstance()->IsKeyTrigger(DIK_F2)) {

        isMouseCursorVisible_ = !isMouseCursorVisible_;

        if (isMouseCursorVisible_) {

            ShowCursor(TRUE);
            UnlockCursor();

        } else {

            ShowCursor(FALSE);
            LockCursorToWindow();
        }
    }

    ImGuiManager::GetInstance()->Begin();

    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        Logger::Log("Escape Pressed");
        endRequest_ = true;
    }

    SceneManager::GetInstance()->Update();
    DebugRenderer::GetInstance()->Update();
    SceneManager::GetInstance()->DrawImGui();
    Camera* defaultCamera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (defaultCamera != nullptr) {
        fog_->SetCameraInfo(
            defaultCamera->GetNearClip(),
            defaultCamera->GetFarClip(),
            defaultCamera->GetFovY(),
            defaultCamera->GetAspectRatio());
    }
    fog_->Update();
    fog_->DrawImGui();

    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    SrvManager::GetInstance()->PreDraw();

    fog_->PreDrawDepth();
    offscreenRenderer_->PreDraw(fog_->GetDepthDSVHandle());
    SceneManager::GetInstance()->Draw3D();
    DebugRenderer::GetInstance()->Draw();
    fog_->PostDrawDepth();
    offscreenRenderer_->PostDraw();

   // ポストエフェクトをかける
    DirectXCommon::GetInstance()->PreDraw();
    copyImageRenderer_->SetPostEffectType(SceneManager::GetInstance()->GetPostEffectType()); // シーンマネージャーからポストエフェクトの種類を取得してセットする
    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
    const bool isBoosting = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT);
    postEffectParameter.radialBlurSampleCount = isBoosting ? 48 : 32;
    postEffectParameter.radialBlurWidth = isBoosting ? 0.25f : 0.05f;
    copyImageRenderer_->Draw(offscreenRenderer_->GetSrvHandleGPU(), fog_->GetDepthSRVHandle());
    fog_->Apply(offscreenRenderer_->GetSrvHandleGPU());

    SceneManager::GetInstance()->Draw2D();

    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    Logger::Log("Game Finalize Start");
   
    UnlockCursor(); // カーソルをウィンドウに固定解除
    ShowCursor(TRUE);
    SceneManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();

    SkinningObject3dManager::GetInstance()->Finalize();
    DebugRenderer::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();
    SkyBoxManager::GetInstance()->Finalize();
    LightManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();

    SoundManager::GetInstance()->Finalize();

    DirectXCommon::GetInstance()->Finalize();

    WinApp::FinalizeInstance();

    Logger::Log("Game Finalize End");
}

void Game::LockCursorToWindow()
{
    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    POINT leftTop;
    leftTop.x = clientRect.left;
    leftTop.y = clientRect.top;

    POINT rightBottom;
    rightBottom.x = clientRect.right;
    rightBottom.y = clientRect.bottom;

    ClientToScreen(hwnd, &leftTop);
    ClientToScreen(hwnd, &rightBottom);

    RECT clipRect;
    clipRect.left = leftTop.x;
    clipRect.top = leftTop.y;
    clipRect.right = rightBottom.x;
    clipRect.bottom = rightBottom.y;

    ClipCursor(&clipRect);
}

void Game::UnlockCursor()
{
    ClipCursor(nullptr);
}
