#include "Engine/Debug/Profiler/StartupTiming.h"
// エンジン機能とシーンを起動し、毎フレームの入力・更新・描画と終了時の解放を統括する。
#include "App/Scene/Application/Game.h"
#include "Engine/Debug/Profiler/Profiler.h"
#include "Engine/Debug/Profiler/BootProfiler.h"
#include "Engine/Debug/Profiler/ProfilerScope.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "App/Scene/Title/TitleScene.h"
#if defined(_DEBUG)
#include "Tests/ObjectFogVerificationScene.h"
#endif
#include "Engine/audio/SoundManager.h"
#include "Engine/Screenshot/ScreenshotManager.h"
#include "Engine/Capture/GifRecordingManager.h"

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

// ウィンドウ・描画・入力・音声・シーンなど、アプリ全体で使う機能を準備する。
void Game::Initialize()
{
    auto prevTime = std::chrono::steady_clock::now();
    Logger::Log("Game Initialize Start");
    ShowCursor(FALSE); // カーソルを消す
    SetUnhandledExceptionFilter(Utility::ExportDump);

    // Profilerの初期化とBoot計測開始
    {
        StartupTimingScope startupScope("Game.Profiler");
        Profiler::GetInstance()->Initialize();
    }
    GetBootProfilerForGame()->Begin("Engine Initialize");

    // 音声の準備を先に開始し、ウィンドウや描画機能の初期化と並行させる。
    GetBootProfilerForGame()->Begin("Audio");
    {
        StartupTimingScope startupScope("Game.Audio");
        SoundManager::GetInstance()->Initialize();
    }
    GetBootProfilerForGame()->End("Audio");

    GetBootProfilerForGame()->Begin("Window");
    {
        StartupTimingScope startupScope("Game.Window");
        WinApp::GetInstance()->initialize();
    }
    GetBootProfilerForGame()->End("Window");

    {
        StartupTimingScope startupScope("Game.ShowWindow");
        WinApp::GetInstance()->Show();
    }

    LockCursorToWindow();

    CheckInitializeTime("WinApp", prevTime);

    GetBootProfilerForGame()->Begin("DirectX");
    {
        StartupTimingScope startupScope("Game.DirectX");
        DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    }
    GetBootProfilerForGame()->End("DirectX");
    CheckInitializeTime("DirectXCommon", prevTime);

    {
        StartupTimingScope startupScope("Game.SrvManager");
        SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    }
    CheckInitializeTime("SrvManager", prevTime);

    GetBootProfilerForGame()->Begin("Texture");
    {
        StartupTimingScope startupScope("Game.TextureManager");
        TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    }
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

    {
        StartupTimingScope startupScope("Game.SpriteManager");
        SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    }
    CheckInitializeTime("SpriteManager", prevTime);

    {
        StartupTimingScope startupScope("Game.TextRenderer");
        TextRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());
    }
    CheckInitializeTime("TextRenderer", prevTime);

    GetBootProfilerForGame()->Begin("Model");
    {
        StartupTimingScope startupScope("Game.ModelManager");
        ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    }
    GetBootProfilerForGame()->End("Model");
    CheckInitializeTime("ModelManager", prevTime);

    // Shader初期化ダミー計測 (DirectXCommon等に含まれるが要件定義のため)
    GetBootProfilerForGame()->Begin("Shader");
    {
        StartupTimingScope startupScope("Game.ModelCommon");
        modelCommon_.Initialize(DirectXCommon::GetInstance());
    }
    GetBootProfilerForGame()->End("Shader");

    {
        StartupTimingScope startupScope("Game.Input");
        Input::GetInstance()->Initialize(WinApp::GetInstance());
    }

    Logger::Log("Load Default Textures");
    {
        StartupTimingScope startupScope("Game.DefaultTexture");
        TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
    }

    // エフェクトのシェーダーとパイプラインはゲーム起動時に一度だけ作成する。
    // 使用するカメラは各シーンのInitializeで設定する。
    GetBootProfilerForGame()->Begin("Scene");
    {
        StartupTimingScope startupScope("Game.ScheduleTitle");
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    }
    GetBootProfilerForGame()->End("Scene");

    renderer_ = std::make_unique<Renderer>();
    {
        StartupTimingScope startupScope("Game.Renderer");
        renderer_->Initialize();
    }
    SceneManager::GetInstance()->SetRenderer(renderer_.get());
    SceneManager::GetInstance()->SetVolumetricLightRenderer(renderer_->GetPostEffectManager()->GetVolumetricLightRenderer());
#if defined(_DEBUG)
    if (std::wstring(GetCommandLineW()).find(L"--verify-object-fog") != std::wstring::npos) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ObjectFogVerificationScene>(renderer_.get()));
        showDebugUI_ = false;
    }
#endif
    {
        StartupTimingScope startupScope("Game.Screenshot");
        ScreenshotManager::GetInstance()->Initialize(
            DirectXCommon::GetInstance(),
            WinApp::GetInstance());
    }
    {
        StartupTimingScope startupScope("Game.GifRecorder");
        GifRecordingManager::GetInstance()->Initialize(
            DirectXCommon::GetInstance(),
            WinApp::GetInstance());
    }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    renderer_->GetPostEffectManager()->RegisterDevelopmentPanel();
#endif

    // Boot計測完了
    GetBootProfilerForGame()->End("Engine Initialize");
    GetBootProfilerForGame()->FinalizeBootMeasure();

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    InitializePerformanceLog();
#endif

    {
        StartupTimingScope startupScope("Game.TimeManager");
        TimeManager::GetInstance()->Initialize();
    }

#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (!DevelopmentWebPanel::GetInstance().Start()) {
        Logger::Log("Development browser panel failed to start");
    }
#endif

    Logger::Log("Game Initialize End");
}

// 入力と各管理機能を更新し、現在のシーンを一フレーム進める。
void Game::Update()
{
    TimeManager::GetInstance()->Update();
    SoundManager::GetInstance()->Update();

    // フレーム全体の開始
    Profiler::GetInstance()->BeginFrame();
    Profiler::GetInstance()->Update();

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    UpdatePerformanceLog();
#endif

    Input::GetInstance()->Update();

    if (Input::GetInstance()->IsKeyTrigger(DIK_F11)) {
        if (!renderer_->ToggleFullscreen()) {
            Logger::Log("F11 display switch failed");
        }
        if (!isSystemCursorVisible_) {
            LockCursorToWindow();
        }
    }

    ScreenshotManager::GetInstance()->Update();
    GifRecordingManager::GetInstance()->Update();
    const bool gifShiftHeld = Input::GetInstance()->IsKeyPressed(DIK_LSHIFT) ||
        Input::GetInstance()->IsKeyPressed(DIK_RSHIFT);
    if (!gifShiftHeld && Input::GetInstance()->IsKeyTrigger(DIK_F7)) {
        GifRecordingManager::GetInstance()->ChangeRecordingDuration(-1);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_F8)) {
        GifRecordingManager::GetInstance()->ChangeRecordingDuration(1);
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_F9)) {
        GifRecordingManager::GetInstance()->ToggleRecording();
    }
    if (Input::GetInstance()->IsKeyTrigger(DIK_F12)) {
        bool savePng = false;
        if (Input::GetInstance()->IsKeyPressed(DIK_LSHIFT) ||
            Input::GetInstance()->IsKeyPressed(DIK_RSHIFT)) {
            savePng = true;
        }
        ScreenshotManager::GetInstance()->RequestCapture(savePng);
    }

#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().Poll();
#endif

    if (Input::GetInstance()->IsKeyTrigger(DIK_F2)) {
        isMouseCursorVisible_ = !isMouseCursorVisible_;
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_F10)) {
        showDebugUI_ = !showDebugUI_;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
        DevelopmentWebPanel::GetInstance().SetLegacyUiVisible(showDebugUI_);
#else
        DebugRenderer::GetInstance()->SetVisible(showDebugUI_);
#endif
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_ESCAPE) && !SceneManager::GetInstance()->IsPauseMenuOpen()) {
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
    
    const bool wantsCursor = isMouseCursorVisible_ || SceneManager::GetInstance()->IsPauseMenuOpen();
    if (wantsCursor != isSystemCursorVisible_) {
        ShowCursor(wantsCursor ? TRUE : FALSE);
        if (wantsCursor) UnlockCursor();
        else LockCursorToWindow();
        isSystemCursorVisible_ = wantsCursor;
    }

    DebugRenderer::GetInstance()->Update();
    
#ifdef USE_IMGUI
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    // Pause controls use the game's own UI in every configuration.
    SceneManager::GetInstance()->DrawImGui();
#else
    if (showDebugUI_) {
        SceneManager::GetInstance()->DrawImGui();
    }
#endif
#endif
    
    {
        ProfilerScope scope("Renderer");
        renderer_->Update();
    }
    
#ifdef USE_IMGUI
    if (showDebugUI_) {
#if !defined(ENABLE_DEVELOPMENT_TOOLS)
        renderer_->DrawImGui();
#endif
        Profiler::GetInstance()->DrawImGui();
    }

    ImGuiManager::GetInstance()->End();
#endif
    
    // フレームの終了
    Profiler::GetInstance()->EndFrame();
}

// 描画処理を実行し、更新済みのゲーム画面を表示する。
void Game::Draw()
{
    renderer_->Draw(SceneManager::GetInstance());
}

// シーンやエンジン機能を終了し、確保したリソースを解放する。
void Game::Finalize()
{
    Logger::Log("Game Finalize Start");
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().Stop();
#endif
   
    UnlockCursor(); // カーソルをウィンドウに固定解除
    ShowCursor(TRUE);
    SceneManager::GetInstance()->Finalize();
    CollisionManager::Finalize();
    // EffectManagerの共通リソースはゲーム終了時にだけ破棄する。
    EffectManager::Finalize();
#ifdef USE_IMGUI
    ImGuiManager::Finalize();
#endif
    SceneManager::GetInstance()->SetVolumetricLightRenderer(nullptr);
    SceneManager::GetInstance()->SetRenderer(nullptr);
    renderer_.reset();

    GifRecordingManager::Finalize();
    ScreenshotManager::Finalize();

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
// 性能計測ログを記録するための出力先と開始状態を準備する。
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

// フレームの計測値を集計し、一定の間隔で性能ログへ書き出す。
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

// 操作中にマウスがゲームの表示範囲から出ないようにする。
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

// ウィンドウへのマウス固定を解除し、通常のカーソル操作へ戻す。
void Game::UnlockCursor()
{
    ClipCursor(nullptr);
}
