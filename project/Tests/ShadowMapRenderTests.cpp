#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/PostEffect/Volumetric/VolumetricLightRenderer.h"
#include <limits>
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "App/Game/Stage/StageCatalog.h"
#include "App/Game/Stage/GameplayVisualPreset.h"
#include "App/Scene/Application/Game.h"
#include "App/Scene/Gameplay/GamePlayScene.h"
#include "App/Scene/Common/SceneManager.h"
#include "App/Scene/Loading/LoadingScene.h"
#include "Engine/Logger/Logger.h"
#include "DirectXTex/DirectXTex.h"
#include <d3d12sdklayers.h>
#include <wincodec.h>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <vector>
#include <cmath>

namespace {
void Require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}
std::vector<uint8_t> ReadFrame(DirectXCommon* dx, const std::filesystem::path& file)
{
    auto* source = dx->GetCurrentBackBuffer();
    auto desc = source->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
    UINT64 size = 0;
    dx->GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &size);
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    Require(SUCCEEDED(dx->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback))), "Readback allocation failed");
    auto before = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    dx->GetCommandList()->ResourceBarrier(1, &before);
    D3D12_TEXTURE_COPY_LOCATION destination {};
    destination.pResource = readback.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = layout;
    D3D12_TEXTURE_COPY_LOCATION origin {};
    origin.pResource = source; origin.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
    auto after = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx->GetCommandList()->ResourceBarrier(1, &after);
    dx->PostDraw();
    void* data = nullptr;
    D3D12_RANGE range { 0, static_cast<SIZE_T>(size) };
    Require(SUCCEEDED(readback->Map(0, &range, &data)), "Readback map failed");
    DirectX::ScratchImage image;
    Require(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, desc.Width, desc.Height, 1, 1)), "Image allocation failed");
    const auto* result = image.GetImage(0, 0, 0);
    const auto* bytes = static_cast<const uint8_t*>(data) + layout.Offset;
    for (size_t row = 0; row < desc.Height; ++row) {
        memcpy(result->pixels + result->rowPitch * row, bytes + layout.Footprint.RowPitch * row, desc.Width * 4);
    }
    D3D12_RANGE written { 0, 0 };
    readback->Unmap(0, &written);
    Require(SUCCEEDED(DirectX::SaveToWICFile(*result, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, file.c_str())), "PNG save failed");
    return { result->pixels, result->pixels + result->slicePitch };
}
size_t ChangedPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    Require(a.size() == b.size(), "Capture dimensions differ");
    size_t changed = 0;
    for (size_t i = 0; i < a.size(); i += 4) {
        int difference = 0;
        for (int c = 0; c < 3; ++c) { difference += std::abs(static_cast<int>(a[i+c]) - b[i+c]); }
        if (difference > 6) { ++changed; }
    }
    return changed;
}
}

// Exercise the actual game Renderer and Stage03 scene without modifying preview.
class ShadowTestStage : public GamePlayScene {
public:
    explicit ShadowTestStage(const std::string& stageId) : GamePlayScene(stageId) {}
    void SetShadowEnabled(bool enabled) { enabled_ = enabled; }
    void PrepareBossCapture()
    {
        const Vector3 bossPosition = stageSettings_.bossPosition;
        player_->SetTranslate(bossPosition + Vector3 { 0.0f, 0.0f, -120.0f });
        player_->Update();
        TimeManager::GetInstance()->SetTimeScale(1.0f);
        for (int frame = 0; frame < 180; ++frame) {
            Sleep(16);
            TimeManager::GetInstance()->Update();
            bossController_->Update(stageSettings_.bossSpawnDistance);
        }
        TimeManager::GetInstance()->SetTimeScale(0.0f);
        camera_->LookAt(bossPosition + Vector3 { 70.0f, 65.0f, -180.0f }, bossPosition);
        camera_->Update();
    }
    void AdvanceBossCapture()
    {
        TimeManager::GetInstance()->SetTimeScale(1.0f);
        for (int frame = 0; frame < 45; ++frame) {
            Sleep(16);
            TimeManager::GetInstance()->Update();
            bossController_->Update(stageSettings_.bossSpawnDistance);
        }
        TimeManager::GetInstance()->SetTimeScale(0.0f);
    }
    ShadowSettings GetShadowSettings() const override {
        ShadowSettings settings = GamePlayScene::GetShadowSettings();
        settings.enabled = enabled_;
        return settings;
    }
private:
    bool enabled_ = true;
};

class TitleSceneRenderTest {
public:
    static void EnterSelection(TitleScene& title)
    {
        title.StartStageSelectTransition();
        for (int frame = 0; frame < 220; ++frame) {
            title.UpdateStageSelectTransition(1.0f / 60.0f);
            if (title.viewState_ == TitleScene::ViewState::StageSelect) { break; }
        }
        Require(title.viewState_ == TitleScene::ViewState::StageSelect, "Title camera did not enter the hangar");
        Refresh(title);
    }
    static void StartRoomSwitch(TitleScene& title)
    {
        // Follow the catalog order; Stage02 can sit between the two populated rooms.
        for (size_t attempt = 0; attempt < title.stages_.size(); ++attempt) {
            const size_t next = (title.currentStageIndex_ + 1) % title.stages_.size();
            if (title.stages_[next].id == "stage03") { break; }
            title.StartStageRoomTransition(1);
            title.UpdateStageRoomTransition(0.80f);
        }
        title.StartStageRoomTransition(1);
        title.UpdateStageRoomTransition(0.30f);
        title.UpdateSceneLighting();
        title.UpdateInterface(0.0f);
    }
    static void FinishRoomSwitch(TitleScene& title)
    {
        title.UpdateStageRoomTransition(0.50f);
        Require(title.stages_[title.currentStageIndex_].id == "stage03", "Frozen hangar was not selected");
        Refresh(title);
    }
    static void ReturnToOverview(TitleScene& title)
    {
        title.StartTitleReturn();
        for (int frame = 0; frame < 150; ++frame) {
            title.UpdateStageSelectTransition(1.0f / 60.0f);
            if (title.viewState_ == TitleScene::ViewState::Title) { break; }
        }
        Require(title.viewState_ == TitleScene::ViewState::Title, "Title camera did not return to overview");
        Refresh(title);
    }
private:
    static void Refresh(TitleScene& title)
    {
        title.UpdateStageRoomTransition(0.0f);
        title.UpdateSceneLighting();
        title.UpdateInterface(0.0f);
    }
};

class ShadowTestTitle : public TitleScene {
public:
    void SetShadowEnabled(bool enabled) { enabled_ = enabled; }
    ShadowSettings GetShadowSettings() const override
    {
        ShadowSettings settings = TitleScene::GetShadowSettings();
        settings.enabled = enabled_;
        return settings;
    }
private:
    bool enabled_ = true;
};

int RunTitleShadowTest()
{
    Logger::Initialize();
    const std::filesystem::path directory = "captures/ShadowMapTests";
    std::filesystem::create_directories(directory);
    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
    Game game;
    bool initialized = false;
    try {
        game.Initialize();
        initialized = true;
        ShowWindow(WinApp::GetInstance()->GetHwnd(), SW_HIDE);
        auto scene = std::make_unique<ShadowTestTitle>();
        ShadowTestTitle* title = scene.get();
        // Replace the pending title before its first update, preserving normal startup.
        SceneManager::GetInstance()->SetNextScene(std::move(scene));
        TimeManager::GetInstance()->SetTimeScale(0.0f);
        game.Update();
        game.Draw();
        auto* dx = DirectXCommon::GetInstance();
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) { info->ClearStoredMessages(); }
        const char* views[] = { "overview", "hangar", "room-transition", "frozen-hangar", "return-overview" };
        for (int view = 0; view < 5; ++view) {
            if (view == 1) { TitleSceneRenderTest::EnterSelection(*title); }
            if (view == 2) { TitleSceneRenderTest::StartRoomSwitch(*title); }
            if (view == 3) { TitleSceneRenderTest::FinishRoomSwitch(*title); }
            if (view == 4) { TitleSceneRenderTest::ReturnToOverview(*title); }
            std::vector<uint8_t> captures[2];
            for (int mode = 0; mode < 2; ++mode) {
                title->SetShadowEnabled(mode == 0);
                // Present rotates the back buffer. Fill both buffers before reading
                // the current one so ON/OFF captures cannot reuse the previous mode.
                for (int frame = 0; frame < 3; ++frame) { game.Draw(); }
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(),
                    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                dx->GetCommandList()->ResourceBarrier(1, &barrier);
                std::string name = "title-" + std::string(views[view]) + "-enabled.png";
                if (mode == 1) { name = "title-" + std::string(views[view]) + "-disabled.png"; }
                captures[mode] = ReadFrame(dx, directory / name);
            }
            const size_t changed = ChangedPixels(captures[0], captures[1]);
            Logger::Log("Title shadow " + std::string(views[view]) + ": affected pixels=" + std::to_string(changed));
            Require(changed > 500, "Title shadow did not affect enough pixels");
        }
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    Logger::Log(message->pDescription);
                    throw std::runtime_error("Title D3D12 validation failed");
                }
            }
        }
        TimeManager::GetInstance()->SetTimeScale(1.0f);
        game.Finalize();
        std::ofstream(directory / "title-result.txt") << "PASS: title overview, hangar, room transition, frozen hangar, return; shadow ON/OFF; no D3D12 errors\n";
        Logger::Finalize();
        return 0;
    } catch (const std::exception& error) {
        std::ofstream(directory / "title-result.txt") << "FAIL: " << error.what();
        Logger::Log(error.what());
        if (initialized) { game.Finalize(); }
        Logger::Finalize();
        return 1;
    }
}

int RunGameStageShadowTest(const std::string& stageId)
{
    Logger::Initialize();
    std::filesystem::create_directories("captures/ShadowMapTests");
    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
    Game game;
    bool initialized = false;
    try {
        game.Initialize();
        initialized = true;
        ShowWindow(WinApp::GetInstance()->GetHwnd(), SW_HIDE);
        // Follow normal startup: the title initializes the shared 3D/light managers.
        game.Update();
        game.Draw();
        auto scene = std::make_unique<ShadowTestStage>(stageId);
        ShadowTestStage* stage = scene.get();
        SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(std::move(scene)));
        bool stageReady = false;
        for (int frame = 0; frame < 400; ++frame) {
            game.Update();
            game.Draw();
            if (SceneManager::GetInstance()->GetShadowSettings().enabled) { stageReady = true; break; }
        }
        Require(stageReady, "Stage did not finish the normal loading sequence");
        TimeManager::GetInstance()->SetTimeScale(0.0f);
        auto* dx = DirectXCommon::GetInstance();
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) { info->ClearStoredMessages(); }
        std::vector<uint8_t> enabledFrame;
        std::vector<uint8_t> disabledFrame;
        int viewCount = 2;
        if (stageId == "stage03") { viewCount = 3; }
        if (stageId == "stage04") { viewCount = 1; }
        for (int view = 0; view < viewCount; ++view) {
            if (view == 1) { stage->PrepareBossCapture(); }
            if (view == 2) { stage->AdvanceBossCapture(); }
            for (int mode = 0; mode < 2; ++mode) {
                stage->SetShadowEnabled(mode == 0);
                for (int frame = 0; frame < 4; ++frame) { game.Update(); game.Draw(); }
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(),
                    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                dx->GetCommandList()->ResourceBarrier(1, &barrier);
                std::filesystem::path file = "captures/ShadowMapTests/" + stageId + "-enabled.png";
                if (mode == 1) { file = "captures/ShadowMapTests/" + stageId + "-disabled.png"; }
                if (view == 1) {
                    file = "captures/ShadowMapTests/" + stageId + "-boss-enabled.png";
                    if (mode == 1) { file = "captures/ShadowMapTests/" + stageId + "-boss-disabled.png"; }
                }
                if (view == 2) {
                    file = "captures/ShadowMapTests/" + stageId + "-boss-motion-enabled.png";
                    if (mode == 1) { file = "captures/ShadowMapTests/" + stageId + "-boss-motion-disabled.png"; }
                }
                if (mode == 0) { enabledFrame = ReadFrame(dx, file); }
                else { disabledFrame = ReadFrame(dx, file); }
            }
            Require(ChangedPixels(enabledFrame, disabledFrame) > 100,
                "Stage shadow ON/OFF did not change enough pixels");
        }
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    Logger::Log(message->pDescription);
                    throw std::runtime_error("Game stage D3D12 validation failed");
                }
            }
        }
        game.Finalize();
        std::ofstream("captures/ShadowMapTests/stage-result.txt") << "PASS: actual " << stageId << " game render, shadow ON/OFF, no D3D12 errors\n";
        Logger::Finalize();
        return 0;
    } catch (const std::exception& error) {
        std::ofstream("captures/ShadowMapTests/stage-result.txt") << "FAIL: " << error.what();
        Logger::Log(error.what());
        if (initialized) { game.Finalize(); }
        Logger::Finalize();
        return 1;
    }
}

class FxaaRenderTest {
public:
    static PostEffectManager* GetManager(Game& game)
    {
        return game.renderer_->GetPostEffectManager();
    }
};

int RunFxaaTest()
{
    const std::filesystem::path directory = "captures/ShadowMapTests";
    std::filesystem::create_directories(directory);
    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
    Game game;
    bool initialized = false;
    try {
        game.Initialize();
        initialized = true;
        ShowWindow(WinApp::GetInstance()->GetHwnd(), SW_HIDE);
        game.Update();
        game.Draw();
        auto* dx = DirectXCommon::GetInstance();
        auto* manager = FxaaRenderTest::GetManager(game);
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) { info->ClearStoredMessages(); }
        ShadowTestStage* stage = nullptr;
        for (int view = 0; view < 3; ++view) {
            if (view == 1) {
                TimeManager::GetInstance()->SetTimeScale(1.0f);
                auto scene = std::make_unique<ShadowTestStage>("stage03");
                stage = scene.get();
                SceneManager::GetInstance()->SetNextScene(std::make_unique<LoadingScene>(std::move(scene)));
                bool ready = false;
                for (int frame = 0; frame < 400; ++frame) {
                    game.Update(); game.Draw();
                    if (SceneManager::GetInstance()->GetShadowSettings().enabled) { ready = true; break; }
                }
                Require(ready, "FXAA stage loading failed");
            }
            if (view == 2) { stage->PrepareBossCapture(); }
            TimeManager::GetInstance()->SetTimeScale(0.0f);
            SpriteManager::GetInstance()->GetRenderManager()->FreezeFrameTimeForTests(true);
            std::vector<uint8_t> captures[3];
            const char* modes[] = { "off", "on", "zero" };
            const char* views[] = { "title", "stage03", "jellyfish" };
            for (int mode = 0; mode < 3; ++mode) {
                manager->SetFxaaEnabled(mode != 0);
                auto& parameter = manager->GetCopyImageRenderer()->GetPostEffectParameter();
                parameter.fxaaStrength = 1.0f;
                if (mode == 2) { parameter.fxaaStrength = 0.0f; }
                for (int frame = 0; frame < 3; ++frame) { game.Draw(); }
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->GetCurrentBackBuffer(),
                    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
                dx->GetCommandList()->ResourceBarrier(1, &barrier);
                captures[mode] = ReadFrame(dx, directory / ("fxaa-" + std::string(views[view]) + "-" + modes[mode] + ".png"));
            }
            Require(ChangedPixels(captures[0], captures[1]) > 100, "FXAA did not affect scene edges");
            Require(captures[0] == captures[2], "FXAA strength zero must exactly match OFF");
            if (view != 0) {
                const size_t width = dx->GetCurrentBackBuffer()->GetDesc().Width;
                for (size_t y = 44; y < 56; ++y) {
                    for (size_t x = 1050; x < 1230; ++x) {
                        const size_t offset = (y * width + x) * 4;
                        for (size_t channel = 0; channel < 4; ++channel) {
                            Require(captures[0][offset + channel] == captures[1][offset + channel], "FXAA modified opaque HUD pixels");
                        }
                    }
                }
            }
        }
        D3D12_QUERY_HEAP_DESC queryDesc {};
        queryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDesc.Count = 2;
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> query;
        Require(SUCCEEDED(dx->GetDevice()->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&query))), "GPU timer creation failed");
        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
        Microsoft::WRL::ComPtr<ID3D12Resource> timerReadback;
        Require(SUCCEEDED(dx->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&timerReadback))), "GPU timer readback failed");
        UINT64 frequency = 0;
        Require(SUCCEEDED(dx->GetCommandQueue()->GetTimestampFrequency(&frequency)), "GPU timer frequency failed");
        double averages[3] {};
        manager->GetCopyImageRenderer()->GetPostEffectParameter().fxaaStrength = 1.0f;
        for (int mode = 0; mode < 3; ++mode) {
            manager->SetFxaaEnabled(mode != 0);
            manager->GetBloomRenderer()->GetEditableBloomParameter()->isEnabled = 0;
            if (mode == 2) { manager->GetBloomRenderer()->GetEditableBloomParameter()->isEnabled = 1; }
            for (int sample = 0; sample < 24; ++sample) {
                dx->PreDraw();
                SrvManager::GetInstance()->PreDraw();
                dx->GetCommandList()->EndQuery(query.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
                manager->ApplyAfterParticleDraw(nullptr);
                dx->GetCommandList()->EndQuery(query.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
                dx->GetCommandList()->ResolveQueryData(query.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, timerReadback.Get(), 0);
                dx->PostDraw();
                UINT64* ticks = nullptr;
                D3D12_RANGE range { 0, sizeof(UINT64) * 2 };
                Require(SUCCEEDED(timerReadback->Map(0, &range, reinterpret_cast<void**>(&ticks))), "GPU timer map failed");
                if (sample >= 4) { averages[mode] += double(ticks[1] - ticks[0]) * 1000.0 / double(frequency) / 20.0; }
                D3D12_RANGE written { 0, 0 };
                timerReadback->Unmap(0, &written);
            }
        }
        std::ofstream(directory / "fxaa-timing.txt") << "1280x720, isolated HDR finishing, debug layer enabled, 20 samples\nToneMap ms: "
            << averages[0] << "\nToneMap + FXAA ms: " << averages[1] << "\nBloom + ToneMap + FXAA ms: " << averages[2]
            << "\nBloom difference ms: " << averages[2] - averages[1] << "\n";
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    Logger::Log(message->pDescription);
                    throw std::runtime_error("FXAA D3D12 validation failed");
                }
            }
        }
        game.Finalize();
        std::ofstream(directory / "fxaa-result.txt") << "PASS: title, stage03, jellyfish; FXAA ON/OFF, exact strength zero, opaque HUD unchanged, no D3D12 errors\n";
        Logger::Finalize();
        return 0;
    } catch (const std::exception& error) {
        std::ofstream(directory / "fxaa-result.txt") << "FAIL: " << error.what();
        if (initialized) { game.Finalize(); }
        Logger::Finalize();
        return 1;
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    if (std::string(commandLine).find("--fxaa") != std::string::npos) { return RunFxaaTest(); }
    if (std::string(commandLine).find("--title") != std::string::npos) { return RunTitleShadowTest(); }
    if (std::string(commandLine).find("--stage01") != std::string::npos) { return RunGameStageShadowTest("stage01"); }
    if (std::string(commandLine).find("--stage02") != std::string::npos) { return RunGameStageShadowTest("stage02"); }
    if (std::string(commandLine).find("--stage04") != std::string::npos) { return RunGameStageShadowTest("stage04"); }
    if (std::string(commandLine).find("--stage") != std::string::npos) { return RunGameStageShadowTest("stage03"); }
    int exitCode = 0;
    try {
        std::filesystem::path directory = "captures/ShadowMapTests";
        std::filesystem::create_directories(directory);
        Logger::Initialize();
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
        WinApp::GetInstance()->initialize();
        auto* dx = DirectXCommon::GetInstance();
        dx->Initialize(WinApp::GetInstance());
        SrvManager::GetInstance()->Initialize(dx);
        TextureManager::GetInstance()->Initialize(dx, SrvManager::GetInstance());
        TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
        Object3dManager::GetInstance()->Initialize(dx);
        ModelManager::GetInstance()->Initialize(dx);
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dx->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) { info->ClearStoredMessages(); }
        {
            Camera camera;
            camera.Initialize();
            camera.SetFovY(0.7f);
            camera.LookAt({ 0.0f, 40.0f, -95.0f }, { 0.0f, 0.0f, 35.0f });
            camera.Update();
            Object3dManager::GetInstance()->SetDefaultCamera(&camera);
            GameplayVisualPreset::ApplyLighting("stage03");
            Require(StageCatalog::GetInstance()->Load(), "Stage catalog failed");
            const StageSettings* stage = StageCatalog::GetInstance()->Find("stage03");
            Require(stage != nullptr && stage->shadows.enabled, "Stage03 must enable shadows");
            Require(StageCatalog::GetInstance()->Find("stage01")->shadows.enabled, "Stage01 shadow support regressed");
            ShadowSettings settings = stage->shadows;
            settings.distance = 180.0f;
            ShadowMapRenderer shadows;
            shadows.Initialize(dx, settings.resolution);
            OffscreenRenderer offscreen;
            offscreen.Initialize();
            CopyImageRenderer copy;
            copy.Initialize(dx);
            Object3d floor;
            floor.Initialize(Object3dManager::GetInstance());
            Require(!floor.GetCastShadow() && !floor.GetReceiveShadow(), "Shadow setters must default off");
            floor.SetModel(ModelManager::GetInstance()->CreatePlane("resources/Textures/white.png"));
            floor.SetScale({ 100.0f, 100.0f, 1.0f });
            floor.SetRotate({ std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
            floor.SetMaterial("resources/Shaders/Object3D/StageIceFloor");
            floor.SetColor({ 0.54f, 0.73f, 0.86f, 1.0f });
            floor.SetReceiveShadow(true);
            floor.Update();
            Object3d ice;
            ice.Initialize(Object3dManager::GetInstance());
            ice.SetModel(ModelManager::GetInstance()->Load("Environment/Ice/IceSpike.obj"));
            ice.SetScale({ 10.0f, 18.0f, 10.0f });
            ice.SetTranslate({ -10.0f, 0.0f, 25.0f });
            ice.SetMaterial("resources/Shaders/Object3D/StageIceSpire");
            ice.SetColor({ 0.82f, 0.94f, 1.0f, 1.0f });
            ice.GetMaterial()->shininess = 96.0f;
            ice.SetCastShadow(true);
            ice.SetReceiveShadow(true);
            ice.Update();
            std::vector<uint8_t> images[4];
            const char* names[] = { "enabled.png", "disabled.png", "cast-off.png", "receive-off.png" };
            for (int frame = 0; frame < 4; ++frame) {
                settings.enabled = frame != 1;
                ice.SetCastShadow(frame != 2);
                floor.SetReceiveShadow(frame != 3);
                ice.SetReceiveShadow(frame != 3);
                shadows.Update(camera, LightManager::GetInstance()->GetDirectionalDirection(), settings);
                shadows.BeginShadowPass();
                ice.DrawShadow(shadows);
                shadows.EndShadowPass();
                Object3dManager::GetInstance()->SetShadowRenderer(&shadows);
                TextureManager::GetInstance()->FlushUploads();
                SrvManager::GetInstance()->PreDraw();
                offscreen.PreDraw(dx->GetDSVHandle());
                dx->GetCommandList()->ClearDepthStencilView(dx->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                Object3dManager::GetInstance()->PreDraw();
                floor.Draw();
                ice.Draw();
                offscreen.PostDraw();
                dx->PreDraw();
                copy.Draw(offscreen.GetSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                images[frame] = ReadFrame(dx, directory / names[frame]);
            }
            size_t affected = ChangedPixels(images[0], images[1]);
            Require(affected > 500, "Shadow did not affect enough pixels");
            Require(ChangedPixels(images[1], images[2]) == 0, "CastShadow OFF must remove shadows");
            Require(ChangedPixels(images[1], images[3]) == 0, "ReceiveShadow OFF must remove shadows");
            Logger::Log("Shadow tests: affected pixels=" + std::to_string(affected));
            Object3dManager::GetInstance()->SetShadowRenderer(nullptr);
            // Isolate each receiver to prove both materials respond to real light controls.
            for (int objectIndex = 0; objectIndex < 2; ++objectIndex) {
                std::vector<uint8_t> lightImages[5];
                std::string prefix = "floor-lighting-";
                if (objectIndex == 1) { prefix = "ice-lighting-"; }
                for (int mode = 0; mode < 5; ++mode) {
                    GameplayVisualPreset::ApplyLighting("stage03");
                    LightManager* lights = LightManager::GetInstance();
                    if (mode >= 1) { lights->SetIntensity(0.0f); }
                    if (mode == 2) { lights->SetAmbientIntensity(0.0f); }
                    if (mode == 3) { lights->SetAmbientColor({ 0.9f, 0.1f, 0.1f }); }
                    if (mode == 4) { lights->SetAmbientColor({ 0.1f, 0.1f, 0.9f }); }
                    SrvManager::GetInstance()->PreDraw();
                    offscreen.PreDraw(dx->GetDSVHandle());
                    dx->GetCommandList()->ClearDepthStencilView(dx->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                    Object3dManager::GetInstance()->PreDraw();
                    if (objectIndex == 0) { floor.Draw(); }
                    if (objectIndex == 1) { ice.Draw(); }
                    offscreen.PostDraw();
                    dx->PreDraw();
                    copy.Draw(offscreen.GetSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                    lightImages[mode] = ReadFrame(dx, directory / (prefix + std::to_string(mode) + ".png"));
                }
                Require(ChangedPixels(lightImages[0], lightImages[1]) > 500, "Directional intensity did not affect material");
                Require(ChangedPixels(lightImages[1], lightImages[2]) > 500, "Ambient intensity did not affect material");
                Require(ChangedPixels(lightImages[3], lightImages[4]) > 500, "Ambient color did not affect material");
                Logger::Log(prefix + "PASS: directional intensity, ambient intensity and ambient color");
            }
            // Verify the actual HDR image path retains distinct radiance above one.
            copy.SetPostEffectType(PostEffectType::ToneMap);
            int highlightValues[3] {};
            const float radiances[] = { 1.0f, 2.0f, 4.0f };
            for (int sample = 0; sample < 3; ++sample) {
                float radiance = radiances[sample];
                offscreen.SetClearColor({ radiance, radiance, radiance, 1.0f });
                SrvManager::GetInstance()->PreDraw();
                offscreen.PreDraw(dx->GetDSVHandle());
                offscreen.PostDraw();
                dx->PreDraw();
                copy.Draw(offscreen.GetSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                auto pixels = ReadFrame(dx, directory / ("hdr-radiance-" + std::to_string(sample) + ".png"));
                highlightValues[sample] = pixels[(360 * 1280 + 640) * 4];
            }
            Require(highlightValues[0] < highlightValues[1] && highlightValues[1] < highlightValues[2],
                "HDR highlights above one were clipped before tone mapping");
            Require(highlightValues[2] < 255, "HDR highlight shoulder failed");

            // Compare material roughness/metallic and sky/ground lighting on geometry.
            offscreen.SetClearColor({ 0.04f, 0.04f, 0.04f, 1.0f });
            ice.SetMaterial("resources/Shaders/Object3D/ShadowStandard");
            ice.SetColor({ 0.50f, 0.60f, 0.70f, 1.0f });
            std::vector<uint8_t> surfaceImages[4];
            for (int mode = 0; mode < 4; ++mode) {
                GameplayVisualPreset::ApplyLighting("stage03");
                ice.SetSurfaceProperties(0.18f, 0.75f, 1.0f);
                if (mode == 1) { ice.SetSurfaceProperties(0.90f, 0.0f, 0.10f); }
                if (mode >= 2) {
                    LightManager::GetInstance()->SetIntensity(0.0f);
                    LightManager::GetInstance()->SetAmbientIntensity(0.65f);
                    LightManager::GetInstance()->SetHemisphereColors({ 1.0f, 0.1f, 0.1f }, { 0.1f, 0.1f, 1.0f });
                    if (mode == 3) {
                        LightManager::GetInstance()->SetHemisphereColors({ 0.1f, 0.1f, 1.0f }, { 1.0f, 0.1f, 0.1f });
                    }
                }
                SrvManager::GetInstance()->PreDraw();
                offscreen.PreDraw(dx->GetDSVHandle());
                dx->GetCommandList()->ClearDepthStencilView(dx->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                Object3dManager::GetInstance()->PreDraw();
                ice.Draw();
                offscreen.PostDraw();
                dx->PreDraw();
                copy.Draw(offscreen.GetSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                surfaceImages[mode] = ReadFrame(dx, directory / ("surface-response-" + std::to_string(mode) + ".png"));
            }
            Require(ChangedPixels(surfaceImages[0], surfaceImages[1]) > 100, "Material response did not change highlights");
            Require(ChangedPixels(surfaceImages[2], surfaceImages[3]) > 500, "Hemisphere colors did not affect surface normals");

            // Verify linear decoding, a visible lighting response and zero-strength equivalence.
            GameplayVisualPreset::ApplyLighting("stage03");
            std::vector<uint8_t> normalImages[3];
            std::vector<uint8_t> outlineNormalImages[3];
            for (int mode = 0; mode < 3; ++mode) {
                ice.SetNormalMap("");
                if (mode > 0) {
                    ice.SetNormalMap("resources/Textures/Normals/ice_detail.png", 1.0f);
                    const std::string key = "resources/Textures/Normals/ice_detail.png#linear";
                    Require(!DirectX::IsSRGB(TextureManager::GetInstance()->GetMetaData(key).format),
                        "Normal texture was decoded as sRGB");
                    Require(TextureManager::GetInstance()->GetMetaData(key).mipLevels > 1,
                        "Normal texture has no mipmaps");
                }
                if (mode == 2) { ice.SetNormalMapStrength(0.0f); }
                SrvManager::GetInstance()->PreDraw();
                offscreen.PreDraw(dx->GetDSVHandle());
                dx->GetCommandList()->ClearDepthStencilView(dx->GetDSVHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                Object3dManager::GetInstance()->PreDraw();
                ice.Draw();
                offscreen.PostDraw();
                dx->PreDraw();
                copy.Draw(offscreen.GetSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                normalImages[mode] = ReadFrame(dx, directory / ("normal-mapping-" + std::to_string(mode) + ".png"));
                SrvManager::GetInstance()->PreDraw();
                dx->PreDraw();
                copy.Draw(offscreen.GetNormalSrvHandleGPU(), shadows.GetSrv(), offscreen.GetNormalSrvHandleGPU());
                outlineNormalImages[mode] = ReadFrame(dx, directory / ("normal-outline-" + std::to_string(mode) + ".png"));
            }
            Require(ChangedPixels(normalImages[0], normalImages[1]) > 100,
                "Normal mapping did not change surface lighting");
            Require(normalImages[0] == normalImages[2], "Zero-strength normal map changed the image");
            Require(outlineNormalImages[0] == outlineNormalImages[1], "Normal map changed outline normals or depth");
            ice.SetNormalMap("");
            Logger::Log("Normal mapping PASS: linear mipmapped texture, surface lighting and zero-strength equivalence");

            PostEffectManager post;
            post.Initialize(dx);
            post.SetNormalTextureHandle(offscreen.GetNormalSrvHandleGPU());
            post.PostDrawDepth();
            post.SetFxaaEnabled(false);
            Logger::Log("HDR bloom test initialized");
            Logger::Flush();
            std::vector<uint8_t> bloomImages[2];
            for (int mode = 0; mode < 2; ++mode) {
                post.GetBloomRenderer()->GetEditableBloomParameter()->isEnabled = mode;
                SrvManager::GetInstance()->PreDraw();
                offscreen.SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
                offscreen.PreDraw(dx->GetDSVHandle());
                D3D12_RECT brightRectangle { 600, 300, 680, 420 };
                const float brightColor[] = { 4.0f, 2.0f, 0.5f, 1.0f };
                dx->GetCommandList()->ClearRenderTargetView(dx->GetRTVHandle(2), brightColor, 1, &brightRectangle);
                offscreen.PostDraw();
                dx->PreDraw();
                post.Apply(nullptr, offscreen.GetSrvHandleGPU());
                bloomImages[mode] = ReadFrame(dx, directory / ("bloom-halo-" + std::to_string(mode) + ".png"));
            }
            const size_t haloPixel = (360 * 1280 + 596) * 4;
            Require(bloomImages[1][haloPixel] > bloomImages[0][haloPixel] + 5,
                "HDR bloom did not spread outside the emissive surface");
            const size_t verticalHaloPixel = (296 * 1280 + 640) * 4;
            Require(bloomImages[1][verticalHaloPixel] > bloomImages[0][verticalHaloPixel] + 5,
                "HDR bloom did not spread vertically");
            auto* volume = post.GetVolumetricLightRenderer();
            Require(volume->IsReady(), "Volumetric light initialization failed");
            VolumetricLightRenderer uninitializedVolume;
            Require(!uninitializedVolume.Generate({}, true), "Uninitialized volume renderer should skip safely");
            ShadowMapRenderer uninitializedShadow;
            volume->SetFrameInputs(&camera, &uninitializedShadow);
            Require(!volume->HasValidFrameInputs(), "Uninitialized shadow was accepted");
            // Moving-camera regression: translated matrix precision must never toggle the effect.
            Camera movingCamera;
            movingCamera.Initialize(); movingCamera.SetFovY(0.45f);
            const float cameraDistances[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
            ShadowSettings movingSettings = settings; movingSettings.enabled = true;
            shadows.Update(camera, LightManager::GetInstance()->GetDirectionalDirection(), movingSettings);
            shadows.BeginShadowPass(); shadows.EndShadowPass();
            for (float distance : cameraDistances) {
                for (int frame = 0; frame < 240; ++frame) {
                    movingCamera.SetTranslate({ 0.0f, 40.0f, distance + frame * 0.25f });
                    movingCamera.SetRotate({ 0.2f, 0.0f, 0.0f }); movingCamera.Update();
                    volume->SetFrameInputs(&movingCamera, &shadows);
                    Require(volume->HasValidFrameInputs(), "Moving camera falsely disabled volumetric light");
                }
            }
            Logger::Log("Volumetric camera regression PASS: 1440 frames, positions 100 through 10000, no false safety toggles");
            volume->SetFogDensity(std::numeric_limits<float>::quiet_NaN());
            volume->SetLightIntensity(std::numeric_limits<float>::infinity());
            volume->SetSampleCount(100000);
            Require(volume->GetParameters().fogDensity == 0.0f && volume->GetParameters().lightIntensity == 0.0f &&
                volume->GetParameters().sampleCount == 64, "Unsafe volume parameters were accepted");
            GameplayVisualPreset::ApplyLighting("stage03");
            ice.SetCastShadow(true);
            floor.SetReceiveShadow(false);
            ice.SetReceiveShadow(false);
            settings.enabled = true;
            Camera outsideCamera;
            outsideCamera.Initialize(); outsideCamera.SetFovY(0.7f);
            outsideCamera.LookAt({ 1000.0f, 40.0f, -95.0f }, { 1000.0f, 0.0f, 35.0f });
            outsideCamera.Update();
            Camera singularCamera;
            singularCamera.Initialize(); singularCamera.SetScale({ 0.0f, 0.0f, 0.0f }); singularCamera.Update();
            std::vector<uint8_t> volumeImages[13];
            for (int mode = 0; mode < 13; ++mode) {
                volume->SetEnabled(mode != 0);
                volume->SetFogDensity(0.006f);
                volume->SetLightIntensity(0.8f);
                volume->SetSampleCount(32);
                if (mode == 6) { volume->SetFogDensity(0.0f); }
                if (mode == 7) { volume->SetLightIntensity(0.0f); }
                if (mode == 9) { volume->SetLightIntensity(std::numeric_limits<float>::quiet_NaN()); }
                settings.enabled = mode != 2;
                shadows.Update(camera, LightManager::GetInstance()->GetDirectionalDirection(), settings);
                Require(!shadows.IsReadyForSampling(), "Shadow update did not invalidate the old frame");
                shadows.BeginShadowPass();
                if (mode != 10) { ice.DrawShadow(shadows); }
                shadows.EndShadowPass();
                if (mode == 3) { volume->SetFrameInputs(nullptr, &shadows); }
                else if (mode == 4) { volume->SetFrameInputs(&camera, &uninitializedShadow); }
                else if (mode == 8) { volume->SetFrameInputs(&camera, nullptr); }
                else if (mode == 11) {
                    volume->SetFrameInputs(&outsideCamera, &shadows);
                    Require(volume->HasValidFrameInputs(), "Outside-volume camera should pass CPU validation");
                }
                else if (mode == 12) {
                    volume->SetFrameInputs(&singularCamera, &shadows);
                    Require(!volume->HasValidFrameInputs(), "Singular camera was accepted");
                }
                else { volume->SetFrameInputs(&camera, &shadows); }
                SrvManager::GetInstance()->PreDraw();
                offscreen.SetClearColor({ 0.04f, 0.04f, 0.04f, 1.0f });
                post.PreDrawDepth();
                offscreen.PreDraw(post.GetDepthDSVHandle());
                Object3dManager::GetInstance()->PreDraw();
                floor.Draw(); ice.Draw();
                offscreen.PostDraw(); post.PostDrawDepth();
                if (mode == 5) {
                    Require(!volume->Generate(shadows.GetSrv(), false), "Missing depth readiness was ignored");
                }
                dx->PreDraw();
                post.GetBloomRenderer()->GetEditableBloomParameter()->isEnabled = 0;
                post.Apply(nullptr, offscreen.GetSrvHandleGPU());
                volumeImages[mode] = ReadFrame(dx, directory / ("volumetric-safety-" + std::to_string(mode) + ".png"));
            }
            Require(ChangedPixels(volumeImages[0], volumeImages[1]) > 1000, "Volumetric lighting did not affect the image");
            for (int mode = 2; mode < 10; ++mode) {
                Require(volumeImages[0] == volumeImages[mode], "Volumetric safety fallback changed the base image");
            }
            Require(ChangedPixels(volumeImages[1], volumeImages[10]) > 20, "Shadow casters did not block volumetric light");
            Require(volumeImages[0] == volumeImages[11], "Light leaked outside the shadow camera volume");
            Require(volumeImages[0] == volumeImages[12], "Singular camera fallback changed the image");
            Require(!volume->Generate(shadows.GetSrv(), true), "Consumed frame inputs were reused");

            FogVolumeSettings testFog;
            testFog.isEnabled = true;
            testFog.center = { 0.0f, 0.0f, 35.0f };
            testFog.radius = 40.0f;
            testFog.halfExtents = { 40.0f, 20.0f, 40.0f };
            testFog.density = 0.025f;
            testFog.edgeSoftness = 10.0f;
            Require(volume->SetFogVolume(0, testFog), "Valid fog volume was rejected");
            FogVolumeSettings invalidFog = testFog;
            invalidFog.density = std::numeric_limits<float>::quiet_NaN();
            Require(!volume->SetFogVolume(0, invalidFog), "NaN fog density was accepted");
            invalidFog = testFog; invalidFog.radius = 0.0f;
            Require(!volume->SetFogVolume(0, invalidFog), "Zero fog radius was accepted");
            Require(!volume->SetFogVolume(8, testFog), "Fog volume limit was ignored");
            Require(!volume->SetNoiseParameters(0.0f, 0.5f, {}), "Zero noise scale was accepted");
            Require(!volume->SetHeightFog(0.0f, -1.0f, 0.1f), "Negative height fog was accepted");
            Require(!volume->SetFogColor({ std::numeric_limits<float>::infinity(), 0.0f, 0.0f }), "Infinite fog color was accepted");
            std::vector<uint8_t> localFogImages[11];
            for (int mode = 0; mode < 11; ++mode) {
                volume->ResetLocalFog();
                volume->SetEnabled(true);
                volume->SetFogDensity(0.0f);
                volume->SetLightIntensity(0.0f);
                volume->SetLocalFogEnabled(mode != 0 && mode != 8);
                FogVolumeSettings settingsFog = testFog;
                if (mode == 2) { settingsFog.shape = FogVolumeShape::Box; }
                if (mode == 3) { settingsFog.center.x = 1000.0f; }
                if (mode == 9) { settingsFog.center = camera.GetTranslate(); settingsFog.radius = 60.0f; }
                Require(volume->SetFogVolume(0, settingsFog), "Fog volume setup failed");
                if (mode == 5 || mode == 6) { Require(volume->SetNoiseParameters(0.1f, 0.8f, {}), "Noise setup failed"); }
                if (mode == 7) { volume->SetLightIntensity(0.35f); }
                if (mode == 10) {
                    volume->ClearFogVolumes();
                    Require(volume->SetHeightFog(0.0f, 0.025f, 0.1f), "Height fog setup failed");
                }
                if (mode == 4) { volume->SetFrameInputs(&camera, nullptr); }
                else { volume->SetFrameInputs(&camera, &shadows); }
                if (mode == 4) { Require(volume->HasValidFrameInputs(), "Local fog required shadow resources"); }
                SrvManager::GetInstance()->PreDraw();
                post.PreDrawDepth();
                offscreen.PreDraw(post.GetDepthDSVHandle());
                Object3dManager::GetInstance()->PreDraw(); floor.Draw(); ice.Draw();
                offscreen.PostDraw(); post.PostDrawDepth();
                dx->PreDraw();
                post.Apply(nullptr, offscreen.GetSrvHandleGPU());
                localFogImages[mode] = ReadFrame(dx, directory / ("local-fog-" + std::to_string(mode) + ".png"));
            }
            Require(ChangedPixels(localFogImages[0], localFogImages[1]) > 500, "Sphere fog did not affect the image");
            Require(ChangedPixels(localFogImages[1], localFogImages[2]) > 100, "Sphere and box fog produced identical images");
            Require(localFogImages[0] == localFogImages[3], "Distant local fog affected the image");
            Require(localFogImages[1] == localFogImages[4], "Missing shadow changed ambient fog");
            Require(ChangedPixels(localFogImages[1], localFogImages[5]) > 100, "Noise did not modulate fog");
            Require(localFogImages[5] == localFogImages[6], "Static fog noise flickered");
            Require(ChangedPixels(localFogImages[1], localFogImages[7]) > 100, "Fog did not scatter sunlight");
            Require(localFogImages[0] == localFogImages[8], "Reset fog changed the base image");
            Require(ChangedPixels(localFogImages[0], localFogImages[9]) > 500, "Camera inside fog failed");
            Require(ChangedPixels(localFogImages[0], localFogImages[10]) > 500, "Height fog did not affect the image");
            volume->ResetLocalFog();
            Require(!volume->GetLocalFogParameters().isEnabled && !volume->GetFogVolumes()[0].isEnabled, "Local fog reset retained settings");
            Logger::Log("Local fog PASS: sphere, box, height, noise, stable frames, camera inside, shadow fallback, invalid settings and reset");

            D3D12_QUERY_HEAP_DESC volumeQueryDesc {};
            volumeQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; volumeQueryDesc.Count = 2;
            Microsoft::WRL::ComPtr<ID3D12QueryHeap> volumeQuery;
            Require(SUCCEEDED(dx->GetDevice()->CreateQueryHeap(&volumeQueryDesc, IID_PPV_ARGS(&volumeQuery))), "Volume GPU timer creation failed");
            D3D12_HEAP_PROPERTIES timerHeap {}; timerHeap.Type = D3D12_HEAP_TYPE_READBACK;
            auto timerDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
            Microsoft::WRL::ComPtr<ID3D12Resource> volumeTimer;
            Require(SUCCEEDED(dx->GetDevice()->CreateCommittedResource(&timerHeap, D3D12_HEAP_FLAG_NONE, &timerDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&volumeTimer))), "Volume GPU timer allocation failed");
            UINT64 volumeFrequency = 0;
            Require(SUCCEEDED(dx->GetCommandQueue()->GetTimestampFrequency(&volumeFrequency)), "Volume GPU timer frequency failed");
            double volumeAverages[4] {};
            volume->SetFogDensity(0.003f); volume->SetLightIntensity(0.35f);
            for (int mode = 0; mode < 4; ++mode) {
                volume->SetEnabled(mode != 0);
                volume->ResetLocalFog();
                if (mode >= 2) {
                    volume->SetLocalFogEnabled(true);
                    uint32_t fogCount = 2;
                    if (mode == 3) { fogCount = 8; volume->SetNoiseParameters(0.05f, 0.5f, {}); }
                    for (uint32_t volumeIndex = 0; volumeIndex < fogCount; ++volumeIndex) {
                        FogVolumeSettings timingFog = testFog;
                        timingFog.center.x += static_cast<float>(volumeIndex) * 2.0f;
                        timingFog.density = 0.003f;
                        volume->SetFogVolume(volumeIndex, timingFog);
                    }
                }
                for (int sample = 0; sample < 24; ++sample) {
                    volume->SetFrameInputs(&camera, &shadows);
                    SrvManager::GetInstance()->PreDraw(); dx->PreDraw();
                    dx->GetCommandList()->EndQuery(volumeQuery.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
                    post.Apply(nullptr, offscreen.GetSrvHandleGPU());
                    dx->GetCommandList()->EndQuery(volumeQuery.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
                    dx->GetCommandList()->ResolveQueryData(volumeQuery.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, volumeTimer.Get(), 0);
                    dx->PostDraw();
                    UINT64* ticks = nullptr;
                    D3D12_RANGE readRange { 0, sizeof(UINT64) * 2 };
                    Require(SUCCEEDED(volumeTimer->Map(0, &readRange, reinterpret_cast<void**>(&ticks))), "Volume GPU timer map failed");
                    if (sample >= 4) { volumeAverages[mode] += double(ticks[1] - ticks[0]) * 1000.0 / double(volumeFrequency) / 20.0; }
                    D3D12_RANGE noWrites { 0, 0 }; volumeTimer->Unmap(0, &noWrites);
                }
            }
            std::ofstream(directory / "volumetric-timing.txt") << "1280x720, half-resolution 32 samples, debug layer enabled, 20 samples\nOFF ms: "
                << volumeAverages[0] << "\nLight only ms: " << volumeAverages[1] << "\nLocal 2 volumes ms: " << volumeAverages[2] << "\nLocal 8 + noise ms: " << volumeAverages[3] << "\nAdded light ms: " << volumeAverages[1] - volumeAverages[0] << "\n";
            volume->ResetLocalFog();
            Logger::Log("Volumetric PASS: lighting, missing/disabled shadows, missing camera/depth, zero/NaN intensity, zero density, consumed frame inputs");

            Logger::Log("Graphics tests PASS: HDR highlight separation, material reflection, hemisphere lighting, emissive bloom halo");
            GameplayVisualPreset::ApplyAtmosphere("stage03");
            Require(SceneManager::GetInstance()->GetSceneDistanceFog().start == 450.0f,
                "Ice atmosphere preset was not applied");
            GameplayVisualPreset::ApplyAtmosphere("stage01");
            Require(SceneManager::GetInstance()->GetSceneDistanceFog().start == 380.0f,
                "Ice fog preset leaked into another stage");
            dx->WaitForGPU();
        }
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0;
                info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                // Synthetic HDR clears deliberately differ from the optimized creation color.
                if (message->ID == D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE) { continue; }
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                    Logger::Log(message->pDescription);
                    throw std::runtime_error("D3D12 debug validation failed");
                }
            }
        }
        std::ofstream(directory / "result.txt") << "PASS: shadows, lighting, HDR highlights, material reflection, hemisphere colors, bloom halo, stage fog presets, normal mapping, stable outline normals, volumetric lighting, local sphere/box/height fog, stable noise, shadow fallbacks, D3D12 validation\n";
        ModelManager::Finalize();
        Object3dManager::Finalize();
        LightManager::Finalize();
        TextureManager::GetInstance()->Finalize();
        SrvManager::GetInstance()->Finalize();
        DirectXCommon::Finalize();
        WinApp::FinalizeInstance();
        Logger::Finalize();
    } catch (const std::exception& error) {
        std::ofstream("captures/ShadowMapTests/result.txt") << "FAIL: " << error.what();
        Logger::Log(error.what());
        Logger::Flush();
        ModelManager::Finalize();
        Object3dManager::Finalize();
        LightManager::Finalize();
        TextureManager::GetInstance()->Finalize();
        SrvManager::GetInstance()->Finalize();
        DirectXCommon::Finalize();
        WinApp::FinalizeInstance();
        Logger::Finalize();
        exitCode = 1;
    }
    return exitCode;
}
