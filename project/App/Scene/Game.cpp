#include "Game.h"
#include "Engine/Debug/Profiler/Profiler.h"
#include "Engine/Debug/Profiler/BootProfiler.h"
#include "Engine/Debug/Profiler/ProfilerScope.h"

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

    // Profilerの初期化とBoot計測開始
    Profiler::GetInstance()->Initialize();
    Profiler::GetInstance()->GetBootProfiler()->Begin("Engine Initialize");

    Profiler::GetInstance()->GetBootProfiler()->Begin("Window");
    WinApp::GetInstance()->initialize();
    Profiler::GetInstance()->GetBootProfiler()->End("Window");

    LockCursorToWindow();

    CheckInitializeTime("WinApp", prevTime);

    Profiler::GetInstance()->GetBootProfiler()->Begin("DirectX");
    DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    Profiler::GetInstance()->GetBootProfiler()->End("DirectX");
    CheckInitializeTime("DirectXCommon", prevTime);

    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SrvManager", prevTime);

    Profiler::GetInstance()->GetBootProfiler()->Begin("Texture");
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    Profiler::GetInstance()->GetBootProfiler()->End("Texture");
    CheckInitializeTime("TextureManager", prevTime);

    Profiler::GetInstance()->GetBootProfiler()->Begin("ImGui");
    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
    Profiler::GetInstance()->GetBootProfiler()->End("ImGui");
    CheckInitializeTime("ImGuiManager", prevTime);

    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SpriteManager", prevTime);

    Profiler::GetInstance()->GetBootProfiler()->Begin("Model");
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Profiler::GetInstance()->GetBootProfiler()->End("Model");
    CheckInitializeTime("ModelManager", prevTime);

    // Shader初期化ダミー計測 (DirectXCommon等に含まれるが要件定義のため)
    Profiler::GetInstance()->GetBootProfiler()->Begin("Shader");
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    Profiler::GetInstance()->GetBootProfiler()->End("Shader");

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

    Input::GetInstance()->Initialize(WinApp::GetInstance());

    Logger::Log("Load Default Textures");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");

    // エフェクトのシェーダーとパイプラインはゲーム起動時に一度だけ作成する。
    // 使用するカメラは各シーンのInitializeで設定する。
    EffectManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(),
        SrvManager::GetInstance(),
        nullptr);

    Profiler::GetInstance()->GetBootProfiler()->Begin("Scene");
    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    Profiler::GetInstance()->GetBootProfiler()->End("Scene");

    renderer_ = std::make_unique<Renderer>();
    renderer_->Initialize();

    Profiler::GetInstance()->GetBootProfiler()->Begin("Audio");
    SoundManager::GetInstance()->Initialize();
    Profiler::GetInstance()->GetBootProfiler()->End("Audio");

    // Boot計測完了
    Profiler::GetInstance()->GetBootProfiler()->End("Engine Initialize");
    Profiler::GetInstance()->GetBootProfiler()->FinalizeBootMeasure();

    Logger::Log("Game Initialize End");
}

void Game::Update()
{
    // フレーム全体の開始
    Profiler::GetInstance()->BeginFrame();
    Profiler::GetInstance()->Update();

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

    if (Input::GetInstance()->IsKeyTrigger(DIK_F10)) {
        showDebugUI_ = !showDebugUI_;
        DebugRenderer::GetInstance()->SetVisible(showDebugUI_);
    }

    ImGuiManager::GetInstance()->Begin();

    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        Logger::Log("Escape Pressed");
        endRequest_ = true;
    }

    {
        ProfilerScope scope("SceneUpdate");
        SceneManager::GetInstance()->Update();
    }
    
    DebugRenderer::GetInstance()->Update();
    
    if (showDebugUI_) {
        SceneManager::GetInstance()->DrawImGui();
    }
    
    {
        ProfilerScope scope("Renderer");
        renderer_->Update();
    }
    
    if (showDebugUI_) {
        renderer_->DrawImGui();
        Profiler::GetInstance()->DrawImGui();
    }

    ImGuiManager::GetInstance()->End();
    
    // フレームの終了
    Profiler::GetInstance()->EndFrame();
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
    // EffectManagerの共通リソースはゲーム終了時にだけ破棄する。
    EffectManager::Finalize();
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

    // Profilerの解放
    Profiler::FinalizeInstance();

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
