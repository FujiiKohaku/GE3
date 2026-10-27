#pragma once

// Included by ShadowMapRenderTests after its shared game/capture fixtures.
void RequireDisplaySize(ID3D12Resource* resource, uint32_t width, uint32_t height)
{
    Require(resource != nullptr, "Display resource is missing");
    const auto description = resource->GetDesc();
    Require(description.Width == width && description.Height == height, "Display resource dimensions are incorrect");
}

int RunFullscreenIntegrationTest()
{
    Logger::Initialize();
    const std::filesystem::path directory = "generated/Captures/ShadowMapTests";
    std::filesystem::create_directories(directory);
    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
    Game game;
    bool isInitialized = false;
    try {
        game.Initialize();
        isInitialized = true;
        auto* window = WinApp::GetInstance();
        auto* dx = DirectXCommon::GetInstance();
        auto* renderer = FxaaRenderTest::GetRenderer(game);
        auto* temporal = renderer->GetTemporalSuperResolution();
        FxaaRenderTest::HideDebugUi(game);
        ShowWindow(window->GetHwnd(), SW_HIDE);
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) {
            info->ClearStoredMessages();
            info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
            info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
        }
        renderer->SetTemporalAntiAliasing(true, false);
        TimeManager::GetInstance()->SetTimeScale(0);
        for (uint32_t frame = 0; frame < 16; ++frame) { game.Update(); game.Draw(); }
        RECT originalRect {};
        GetWindowRect(window->GetHwnd(), &originalRect);
        RequireDisplaySize(dx->GetCurrentBackBuffer(), 1280, 720);
        const double windowedGpuMs = renderer->GetFrameGpuTimeMs();
        auto windowedBarrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dx->GetCommandList()->ResourceBarrier(1, &windowedBarrier);
        ReadFrame(dx, directory / "fullscreen-title-720p.png");
        uint32_t allocatedCount = 0;
        for (uint32_t cycle = 0; cycle < 5; ++cycle) {
            Require(renderer->ToggleFullscreen(), "Entering borderless fullscreen failed");
            Require(window->IsBorderlessFullscreen(), "Borderless state was not set");
            Require((GetWindowLongPtr(window->GetHwnd(), GWL_STYLE) & WS_CAPTION) == 0, "Fullscreen still has a caption");
            MONITORINFO monitorInfo { sizeof(MONITORINFO) };
            Require(GetMonitorInfo(MonitorFromWindow(window->GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitorInfo) != 0, "Monitor query failed");
            RECT fullscreenRect {};
            GetWindowRect(window->GetHwnd(), &fullscreenRect);
            Require(EqualRect(&fullscreenRect, &monitorInfo.rcMonitor) != 0, "Borderless window does not cover the monitor");
            ShowWindow(window->GetHwnd(), SW_HIDE);
            for (uint32_t frame = 0; frame < 5; ++frame) { game.Update(); game.Draw(); }
            RequireDisplaySize(dx->GetCurrentBackBuffer(), 1920, 1080);
            Require(renderer->GetSceneRenderWidth() == 1920 && renderer->GetSceneRenderHeight() == 1080,
                "Fullscreen must render native 1080p geometry");
            RequireDisplaySize(renderer->GetPostEffectManager()->GetSceneColorTexture(), 1920, 1080);
            RequireDisplaySize(renderer->GetPostEffectManager()->GetDepthTexture(), 1920, 1080);
            RequireDisplaySize(renderer->GetMotionVectorRenderer()->GetTexture(), 1920, 1080);
            RequireDisplaySize(temporal->GetOutputTexture(), 1920, 1080);
            const Vector2 center = window->UiToClient({640, 360});
            const Vector2 restoredCenter = window->ClientToUi(center);
            Require(std::abs(center.x - window->GetClientWidth() * 0.5f) < 0.01f &&
                std::abs(center.y - window->GetClientHeight() * 0.5f) < 0.01f &&
                std::abs(restoredCenter.x - 640) < 0.01f && std::abs(restoredCenter.y - 360) < 0.01f,
                "UI and mouse coordinates do not agree");
            if (cycle == 0) {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                dx->GetCommandList()->ResourceBarrier(1, &barrier);
                ReadFrame(dx, directory / "fullscreen-title-1080p.png");
                std::ofstream(directory / "fullscreen-timing.txt") << "Debug title; single-frame GPU draw time, not a benchmark\n720p ms: "
                    << windowedGpuMs << "\n1080p ms: " << renderer->GetFrameGpuTimeMs() << '\n';
            }
            // Scene size can already be 720p when the output must return to 720p.
            Require(renderer->SetLowResolutionRendering(true, 1280, 720), "Low-resolution setup failed");
            game.Update(); game.Draw();
            Require(renderer->ToggleFullscreen(), "Leaving borderless fullscreen failed");
            ShowWindow(window->GetHwnd(), SW_HIDE);
            for (uint32_t frame = 0; frame < 5; ++frame) { game.Update(); game.Draw(); }
            Require(!window->IsBorderlessFullscreen() && !renderer->IsLowResolutionRenderingEnabled(), "Windowed native state was not restored");
            RequireDisplaySize(dx->GetCurrentBackBuffer(), 1280, 720);
            RequireDisplaySize(temporal->GetOutputTexture(), 1280, 720);
            RequireDisplaySize(renderer->GetPostEffectManager()->GetDepthTexture(), 1280, 720);
            RECT restoredRect {};
            GetWindowRect(window->GetHwnd(), &restoredRect);
            Require(EqualRect(&originalRect, &restoredRect) != 0, "Original window position/size was not restored");
            const uint32_t currentCount = SrvManager::GetInstance()->GetAllocatedCount();
            if (cycle == 1) { allocatedCount = currentCount; }
            if (cycle > 1) { Require(currentCount == allocatedCount, "Repeated fullscreen switches leak descriptors"); }
        }
        renderer->PrepareDlaaForGameplay();
        const bool isDlaaAvailable = renderer->IsDlaaAvailable();
        Require(renderer->ToggleFullscreen(), "DLAA fullscreen switch failed");
        ShowWindow(window->GetHwnd(), SW_HIDE);
        for (uint32_t frame = 0; frame < 4; ++frame) { game.Update(); game.Draw(); }
        if (isDlaaAvailable) {
            Require(renderer->IsDlaaActive() && renderer->GetDlssSuperResolution()->GetRenderWidth() == 1920 &&
                renderer->GetDlssSuperResolution()->GetRenderHeight() == 1080, "DLAA did not recreate at 1080p");
        }
        Require(renderer->ToggleFullscreen(), "DLAA windowed switch failed");
        ShowWindow(window->GetHwnd(), SW_HIDE);
        game.Update(); game.Draw();
        if (isDlaaAvailable) { Require(renderer->IsDlaaActive(), "DLAA failed after returning to 720p"); }
        TimeManager::GetInstance()->SetTimeScale(1);
        auto scene = std::make_unique<ShadowTestStage>("stage03");
        auto* stage = scene.get();
        SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(std::move(scene)));
        for (uint32_t frame = 0; frame < 400 && !stage->HasUpdated(); ++frame) { game.Update(); game.Draw(); }
        Require(stage->HasUpdated(), "Fullscreen test stage did not load");
        renderer->SetTemporalAntiAliasing(true, false);
        Require(renderer->ToggleFullscreen(), "Gameplay fullscreen switch failed");
        ShowWindow(window->GetHwnd(), SW_HIDE);
        for (uint32_t frame = 0; frame < 4; ++frame) { game.Update(); game.Draw(); }
        Require(stage->HasAlignedAimHud(), "1080p aim HUD is misaligned");
        RequireDisplaySize(renderer->GetPostEffectManager()->GetSceneColorTexture(), 1920, 1080);
        auto gameplayBarrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dx->GetCommandList()->ResourceBarrier(1, &gameplayBarrier);
        ReadFrame(dx, directory / "fullscreen-gameplay-1080p.png");
        stage->SetPausedForTest(true);
        for (uint32_t frame = 0; frame < 3; ++frame) { game.Update(); game.Draw(); }
        Require(SceneManager::GetInstance()->IsPauseMenuOpen(), "Pause state was not retained at 1080p");
        auto stageBarrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dx->GetCommandList()->ResourceBarrier(1, &stageBarrier);
        ReadFrame(dx, directory / "fullscreen-pause-1080p.png");
        Require(renderer->ToggleFullscreen(), "Paused windowed switch failed");
        ShowWindow(window->GetHwnd(), SW_HIDE);
        game.Update(); game.Draw();
        Require(SceneManager::GetInstance()->IsPauseMenuOpen(), "Resolution switch closed the pause menu");
        stage->SetPausedForTest(false);
        game.Update(); game.Draw();
        Require(stage->HasAlignedAimHud(), "720p aim HUD is misaligned after resuming");
        auto* rayScene = renderer->GetDxrRenderer();
        if (rayScene->IsSupported()) {
            const auto previousSettings = rayScene->GetSettings();
            auto settings = previousSettings;
            settings.isEnabled = true;
            settings.isDebugVisible = true;
            rayScene->SetSettings(settings);
            Require(renderer->ToggleFullscreen(), "DXR fullscreen switch failed");
            ShowWindow(window->GetHwnd(), SW_HIDE);
            for (uint32_t frame = 0; frame < 3; ++frame) { game.Update(); game.Draw(); }
            Require(rayScene->GetStatistics().hasValidFrame &&
                rayScene->GetStatistics().outputAllocationBytes >= 1920ull * 1080 * 4,
                "DXR debug output did not resize to 1080p");
            Require(renderer->ToggleFullscreen(), "DXR windowed switch failed");
            ShowWindow(window->GetHwnd(), SW_HIDE);
            for (uint32_t frame = 0; frame < 3; ++frame) { game.Update(); game.Draw(); }
            Require(rayScene->GetStatistics().hasValidFrame, "DXR failed after returning to 720p");
            rayScene->SetSettings(previousSettings);
        }
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                Require(message->Severity > D3D12_MESSAGE_SEVERITY_ERROR, message->pDescription);
            }
        }
        TimeManager::GetInstance()->SetTimeScale(1);
        game.Finalize();
        isInitialized = false;
        std::ofstream(directory / "fullscreen-result.txt") << "PASS: native 1080p color/depth/motion/TAA, borderless monitor bounds, UI coordinates, window placement restore, repeated toggles without descriptor leaks, DLAA resizing/fallback, gameplay aim HUD, pause/resume switching, supported DXR debug resizing, D3D12 validation\n";
        Logger::Finalize();
        return 0;
    } catch (const std::exception& error) {
        std::ofstream(directory / "fullscreen-result.txt") << "FAIL: " << error.what();
        Logger::Log(error.what());
        if (isInitialized) { game.Finalize(); }
        Logger::Finalize();
        return 1;
    }
}
