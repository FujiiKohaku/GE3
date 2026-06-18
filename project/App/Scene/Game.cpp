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
    ShowCursor(FALSE); // カーソル非表示
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
    SceneManager::GetInstance()->DrawImGui();

    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    SrvManager::GetInstance()->PreDraw();

    offscreenRenderer_->PreDraw();
    SceneManager::GetInstance()->Draw3D();
    offscreenRenderer_->PostDraw();

    // ↑で描画しためE��をテクスチャとして描画する
    DirectXCommon::GetInstance()->PreDraw();
    copyImageRenderer_->SetPostEffectType(SceneManager::GetInstance()->GetPostEffectType()); // シーンマネージャーからポストエフェクト�E種類を取得してセチE��
    copyImageRenderer_->Draw(offscreenRenderer_->GetSrvHandleGPU(), offscreenRenderer_->GetDepthSrvHandleGPU());
    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();

    SceneManager::GetInstance()->Draw2D();

    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    Logger::Log("Game Finalize Start");
    // カーソルのロチE��を解除して表示する
    UnlockCursor();
    ShowCursor(TRUE);
    SceneManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();

    SkinningObject3dManager::GetInstance()->Finalize();
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