#include "Game.h"

namespace {
void CheckInitializeTime(const char* name, std::chrono::steady_clock::time_point& prevTime)
{
    auto nowTime = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - prevTime).count();

    Logger::Log(std::string(name) + " : " + std::to_string(ms) + "ms");

    prevTime = nowTime;
}
}

void Game::Initialize()
{
    auto prevTime = std::chrono::steady_clock::now();
    Logger::Log("Game Initialize Start");
    ShowCursor(FALSE); // カーソルを消す
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");

    WinApp::GetInstance()->initialize();

    LockCursorToWindow();

    CheckInitializeTime("WinApp", prevTime);

    DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    CheckInitializeTime("DirectXCommon", prevTime);

    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SrvManager", prevTime);

    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    CheckInitializeTime("TextureManager", prevTime);

    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
    CheckInitializeTime("ImGuiManager", prevTime);

    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SpriteManager", prevTime);

    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("ModelManager", prevTime);

    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("Object3dManager", prevTime);

    SkinningObject3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SkinningObject3dManager", prevTime);

    SkyBoxManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SkyBoxManager", prevTime);

    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("LightManager", prevTime);

    DebugRenderer::GetInstance()->Initialize();
    CheckInitializeTime("DebugRenderer", prevTime);

    modelCommon_.Initialize(DirectXCommon::GetInstance());

    Input::GetInstance()->Initialize(WinApp::GetInstance());

    Logger::Log("Load Default Models");
    // Model Path
    // ModelManager::GetInstance()->Load("Debug/Samples/Plane/plane.obj");
    // ModelManager::GetInstance()->Load("Debug/Axis/axis.obj");
    // ModelManager::GetInstance()->Load("UI/Title/TitleTex/titleTex.obj");
    // ModelManager::GetInstance()->Load("Environment/Fence/fence.obj");

    Logger::Log("Load Default Textures");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/fence.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/BaseColor_Cube.png");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/noise0.png");
    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());

    renderer_ = std::make_unique<Renderer>();
    renderer_->Initialize();

    SoundManager::GetInstance()->Initialize();

    Logger::Log("Game Initialize End");
}

void Game::Update()
{
    Input::GetInstance()->Update();

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
    renderer_->Update();
    renderer_->DrawImGui();

    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    renderer_->Draw(SceneManager::GetInstance());
}

void Game::Finalize()
{
    Logger::Log("Game Finalize Start");
   
    UnlockCursor(); // カーソルをウィンドウに固定解除
    ShowCursor(TRUE);
    SceneManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();
    renderer_.reset();

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
