#include "Engine/Shadow/ShadowMapRenderer.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Light/LightManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/CopyImageRenderer.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "App/Game/Stage/StageCatalog.h"
#include "App/Game/Stage/GameplayVisualPreset.h"
#include "App/Scene/Game.h"
#include "App/Scene/GamePlayScene.h"
#include "App/Scene/SceneManager.h"
#include "App/Scene/LoadingScene.h"
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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
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
            Require(!StageCatalog::GetInstance()->Find("stage01")->shadows.enabled, "Other stage should keep shadows off");
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
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                    Logger::Log(message->pDescription);
                    throw std::runtime_error("D3D12 debug validation failed");
                }
            }
        }
        std::ofstream(directory / "result.txt") << "PASS: shadows, ice/floor directional and ambient controls, stage fog presets, D3D12 validation\n";
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
        exitCode = 1;
    }
    return exitCode;
}
