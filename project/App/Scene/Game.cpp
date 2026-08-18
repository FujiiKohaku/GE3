#include "Game.h"
#include "Engine/Debug/Profiler/Profiler.h"
#include "Engine/Debug/Profiler/BootProfiler.h"
#include "Engine/Debug/Profiler/ProfilerScope.h"
#include "Engine/CollisionManager/CollisionManager.h"

#include <format>

namespace {
void CheckInitializeTime(const char* name, std::chrono::steady_clock::time_point& prevTime)
{
    auto nowTime = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - prevTime).count();

    Logger::Log(std::string(name) + " : " + std::to_string(ms) + "ms");

    prevTime = nowTime;
}

BootProfiler* GetBootProfilerForGame()
{
    BootProfiler* bootProfiler =
        Profiler::GetInstance()->GetBootProfiler();
    if (bootProfiler != nullptr) {
        return bootProfiler;
    }

    static BootProfiler dummyBootProfiler;
    return &dummyBootProfiler;
}

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
uint64_t FileTimeToUint64(const FILETIME& fileTime)
{
    return
        (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) |
        static_cast<uint64_t>(fileTime.dwLowDateTime);
}

uint64_t GetCurrentProcessCpuTime()
{
    FILETIME creationTime {};
    FILETIME exitTime {};
    FILETIME kernelTime {};
    FILETIME userTime {};
    if (!GetProcessTimes(
            GetCurrentProcess(),
            &creationTime,
            &exitTime,
            &kernelTime,
            &userTime)) {
        return 0;
    }

    return FileTimeToUint64(kernelTime) + FileTimeToUint64(userTime);
}
#endif
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
    GetBootProfilerForGame()->Begin("Engine Initialize");

    GetBootProfilerForGame()->Begin("Window");
    WinApp::GetInstance()->initialize();
    GetBootProfilerForGame()->End("Window");

    LockCursorToWindow();

    CheckInitializeTime("WinApp", prevTime);

    GetBootProfilerForGame()->Begin("DirectX");
    DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    WinApp::GetInstance()->Show();
    GetBootProfilerForGame()->End("DirectX");
    CheckInitializeTime("DirectXCommon", prevTime);

    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SrvManager", prevTime);

    GetBootProfilerForGame()->Begin("Texture");
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    GetBootProfilerForGame()->End("Texture");
    CheckInitializeTime("TextureManager", prevTime);

#ifdef USE_IMGUI
    GetBootProfilerForGame()->Begin("ImGui");
    ImGuiManager::GetInstance()->Initialize(
        WinApp::GetInstance(),
        DirectXCommon::GetInstance(),
        SrvManager::GetInstance());
    GetBootProfilerForGame()->End("ImGui");
    CheckInitializeTime("ImGuiManager", prevTime);
#endif

    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("SpriteManager", prevTime);

    TextRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());
    CheckInitializeTime("TextRenderer", prevTime);

    GetBootProfilerForGame()->Begin("Model");
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    GetBootProfilerForGame()->End("Model");
    CheckInitializeTime("ModelManager", prevTime);

    // Shader初期化ダミー計測 (DirectXCommon等に含まれるが要件定義のため)
    GetBootProfilerForGame()->Begin("Shader");
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    GetBootProfilerForGame()->End("Shader");

    Input::GetInstance()->Initialize(WinApp::GetInstance());

    Logger::Log("Load Default Textures");
    TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");

    // エフェクトのシェーダーとパイプラインはゲーム起動時に一度だけ作成する。
    // 使用するカメラは各シーンのInitializeで設定する。
    GetBootProfilerForGame()->Begin("Scene");
    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    GetBootProfilerForGame()->End("Scene");

    renderer_ = std::make_unique<Renderer>();
    renderer_->Initialize();

    GetBootProfilerForGame()->Begin("Audio");
    SoundManager::GetInstance()->Initialize();
    GetBootProfilerForGame()->End("Audio");

    // Boot計測完了
    GetBootProfilerForGame()->End("Engine Initialize");
    GetBootProfilerForGame()->FinalizeBootMeasure();

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    InitializePerformanceLog();
#endif

    TimeManager::GetInstance()->Initialize();

    Logger::Log("Game Initialize End");
}

void Game::Update()
{
    TimeManager::GetInstance()->Update();

    // フレーム全体の開始
    Profiler::GetInstance()->BeginFrame();
    Profiler::GetInstance()->Update();

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    UpdatePerformanceLog();
#endif

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

    if (Input::GetInstance()->IsKeyTrigger(DIK_ESCAPE)) {
        Logger::Log("Escape exit confirmation opened");
        const int result = MessageBoxW(
            WinApp::GetInstance()->GetHwnd(),
            L"本当にゲームを終了しますか？",
            L"終了確認",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (result == IDYES) {
            Logger::Log("Exit confirmed");
            endRequest_ = true;
        } else {
            Logger::Log("Exit canceled");
        }
        Profiler::GetInstance()->EndFrame();
        return;
    }

#ifdef USE_IMGUI
    ImGuiManager::GetInstance()->Begin();
#endif

    {
        ProfilerScope scope("SceneUpdate");
        SceneManager::GetInstance()->Update();
    }
    
    DebugRenderer::GetInstance()->Update();
    
#ifdef USE_IMGUI
    if (showDebugUI_) {
        SceneManager::GetInstance()->DrawImGui();
    }
#endif
    
    {
        ProfilerScope scope("Renderer");
        renderer_->Update();
    }
    
#ifdef USE_IMGUI
    if (showDebugUI_) {
        renderer_->DrawImGui();
        Profiler::GetInstance()->DrawImGui();
    }

    ImGuiManager::GetInstance()->End();
#endif
    
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
    CollisionManager::Finalize();
    // EffectManagerの共通リソースはゲーム終了時にだけ破棄する。
    EffectManager::Finalize();
#ifdef USE_IMGUI
    ImGuiManager::Finalize();
#endif
    renderer_.reset();

    SkinningObject3dManager::GetInstance()->Finalize();
    DebugRenderer::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    TextRenderer::Finalize();
    FontManager::Finalize();
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

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
void Game::InitializePerformanceLog()
{
    SYSTEM_INFO systemInfo {};
    GetSystemInfo(&systemInfo);
    if (systemInfo.dwNumberOfProcessors > 0) {
        performanceLogProcessorCount_ = systemInfo.dwNumberOfProcessors;
    }

    performanceLogStartTime_ = std::chrono::steady_clock::now();
    performanceFrameStartTime_ = performanceLogStartTime_;
    performanceLogProcessTime_ = GetCurrentProcessCpuTime();
    performanceLogFrameCount_ = 0;
}

void Game::UpdatePerformanceLog()
{
    performanceLogFrameCount_++;

    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    const double frameTimeMs =
        std::chrono::duration<double, std::milli>(
            now - performanceFrameStartTime_).count();
    performanceFrameStartTime_ = now;

    EffectManager* effectManager = EffectManager::GetInstance();
    effectManager->ReportAndResetFramePerformance(frameTimeMs);

    const double elapsedSeconds =
        std::chrono::duration<double>(now - performanceLogStartTime_).count();
    if (elapsedSeconds < 1.0) {
        return;
    }

    const uint64_t currentProcessTime = GetCurrentProcessCpuTime();
    uint64_t processTimeDelta = 0;
    if (currentProcessTime >= performanceLogProcessTime_) {
        processTimeDelta =
            currentProcessTime - performanceLogProcessTime_;
    }
    const double availableProcessTime =
        elapsedSeconds *
        10000000.0 *
        static_cast<double>(performanceLogProcessorCount_);

    double processCpuUsage = 0.0;
    if (availableProcessTime > 0.0) {
        processCpuUsage =
            static_cast<double>(processTimeDelta) /
            availableProcessTime *
            100.0;
    }

    const double fps =
        static_cast<double>(performanceLogFrameCount_) /
        elapsedSeconds;
    const double averageFrameTimeMs =
        elapsedSeconds *
        1000.0 /
        static_cast<double>(performanceLogFrameCount_);

    Logger::Log(std::format(
        "[Performance] FPS={:.2f} FrameTime={:.2f}ms ProcessCPU={:.2f}% "
        "ParticleUpdateGPU={:.3f}ms ParticleDrawGPU={:.3f}ms",
        fps,
        averageFrameTimeMs,
        processCpuUsage,
        effectManager->GetParticleUpdateGpuTimeMs(),
        effectManager->GetParticleDrawGpuTimeMs()));
    Logger::Flush();

    performanceLogStartTime_ = now;
    performanceLogProcessTime_ = currentProcessTime;
    performanceLogFrameCount_ = 0;
}
#endif

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
