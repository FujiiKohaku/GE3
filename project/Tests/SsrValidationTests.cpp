#include "Engine/Reflection/ScreenSpaceReflection.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/PostEffectManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/MotionVector/MotionVectorRenderer.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include "App/Game/VisualPresetLibrary.h"
#include "DirectXTex/DirectXTex.h"
#include <d3d12sdklayers.h>
#include <wincodec.h>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <utility>
#include <cmath>
#include <algorithm>

namespace {
void RequireSsr(bool isValid, const char* message)
{
    if (!isValid) { throw std::runtime_error(message); }
}
std::vector<uint8_t> CaptureSsrFrame(DirectXCommon* dxCommon, const std::filesystem::path& path)
{
    auto* source = dxCommon->GetCurrentBackBuffer();
    auto description = source->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT64 byteCount = 0;
    dxCommon->GetDevice()->GetCopyableFootprints(&description, 0, 1, 0, &layout, nullptr, nullptr, &byteCount);
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    auto bufferDescription = CD3DX12_RESOURCE_DESC::Buffer(byteCount);
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    RequireSsr(SUCCEEDED(dxCommon->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &bufferDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback))), "SSR capture allocation failed");
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    dxCommon->GetCommandList()->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = readback.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = layout;
    D3D12_TEXTURE_COPY_LOCATION origin = {};
    origin.pResource = source; origin.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dxCommon->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon->GetCommandList()->ResourceBarrier(1, &barrier);
    dxCommon->PostDraw();
    void* mapped = nullptr;
    D3D12_RANGE range = {0, static_cast<SIZE_T>(byteCount)};
    RequireSsr(SUCCEEDED(readback->Map(0, &range, &mapped)), "SSR capture map failed");
    DirectX::ScratchImage image;
    RequireSsr(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, description.Width, description.Height, 1, 1)), "SSR image allocation failed");
    const auto* result = image.GetImage(0, 0, 0);
    const auto* sourceBytes = static_cast<const uint8_t*>(mapped) + layout.Offset;
    for (size_t pixelY = 0; pixelY < description.Height; ++pixelY) {
        memcpy(result->pixels + result->rowPitch * pixelY, sourceBytes + layout.Footprint.RowPitch * pixelY, description.Width * 4);
    }
    D3D12_RANGE written = {0, 0};
    readback->Unmap(0, &written);
    RequireSsr(SUCCEEDED(DirectX::SaveToWICFile(*result, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, path.c_str())), "SSR PNG write failed");
    return {result->pixels, result->pixels + result->slicePitch};
}
size_t CountReflectionColor(const std::vector<uint8_t>& frame, Vector2 center, uint32_t channel)
{
    size_t coloredPixels = 0;
    for (int pixelY = static_cast<int>(center.y) - 18; pixelY <= static_cast<int>(center.y) + 18; ++pixelY) {
        for (int pixelX = static_cast<int>(center.x) - 18; pixelX <= static_cast<int>(center.x) + 18; ++pixelX) {
            if (pixelX < 0 || pixelY < 0 || pixelX >= WinApp::kClientWidth || pixelY >= WinApp::kClientHeight) { continue; }
            size_t offset = (static_cast<size_t>(pixelY) * WinApp::kClientWidth + pixelX) * 4;
            uint32_t otherChannel = (channel + 1) % 3;
            if (frame[offset + channel] > 45 && frame[offset + channel] > frame[offset + otherChannel] * 2) { ++coloredPixels; }
        }
    }
    return coloredPixels;
}
uint64_t ReflectionDifference(const std::vector<uint8_t>& first, const std::vector<uint8_t>& second)
{
    RequireSsr(first.size() == second.size(), "Temporal capture dimensions differ");
    uint64_t difference = 0;
    for (uint32_t pixelY = 390; pixelY < 600; ++pixelY) {
        for (uint32_t pixelX = 420; pixelX < 800; ++pixelX) {
            size_t offset = (static_cast<size_t>(pixelY) * WinApp::kClientWidth + pixelX) * 4;
            for (uint32_t channel = 0; channel < 3; ++channel) {
                difference += static_cast<uint64_t>(std::abs(int(first[offset + channel]) - int(second[offset + channel])));
            }
        }
    }
    return difference;
}
void RequireUnchangedForeground(const std::vector<uint8_t>& first, const std::vector<uint8_t>& second)
{
    for (uint32_t pixelY = 180; pixelY < 330; ++pixelY) {
        for (uint32_t pixelX = 460; pixelX < 550; ++pixelX) {
            size_t offset = (static_cast<size_t>(pixelY) * WinApp::kClientWidth + pixelX) * 4;
            for (uint32_t channel = 0; channel < 3; ++channel) {
                RequireSsr(first[offset + channel] == second[offset + channel], "Reflection blur leaked into the foreground box");
            }
        }
    }
}
float DecodeSrgb(uint8_t value)
{
    float encoded = static_cast<float>(value) / 255.0f;
    if (encoded <= 0.04045f) { return encoded / 12.92f; }
    return std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}
void FinalizeSsrValidation()
{
    ModelManager::Finalize(); Object3dManager::Finalize(); LightManager::Finalize();
    TextureManager::GetInstance()->Finalize(); SrvManager::GetInstance()->Finalize();
    DirectXCommon::Finalize(); WinApp::FinalizeInstance(); Logger::Finalize();
}
}
int RunSsrValidation()
{
    const std::filesystem::path directory = "runtime/captures/SsrValidation";
    std::filesystem::create_directories(directory);
    Logger::Initialize();
    try {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); }
        WinApp::GetInstance()->initialize();
        ShowWindow(WinApp::GetInstance()->GetHwnd(), SW_HIDE);
        auto* dxCommon = DirectXCommon::GetInstance();
        dxCommon->Initialize(WinApp::GetInstance());
        SrvManager::GetInstance()->Initialize(dxCommon);
        TextureManager::GetInstance()->Initialize(dxCommon, SrvManager::GetInstance());
        TextureManager::GetInstance()->LoadTexture("resources/Textures/white.png");
        Object3dManager::GetInstance()->Initialize(dxCommon);
        ModelManager::GetInstance()->Initialize(dxCommon);
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;
        dxCommon->GetDevice()->QueryInterface(IID_PPV_ARGS(&info));
        if (info) {
            info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
            info->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
            info->ClearStoredMessages();
        }
        {
            Camera camera;
            camera.Initialize(); camera.SetFovY(0.9f);
            camera.LookAt({0, 5, -18}, {0, 0, 6}); camera.Update();
            Object3dManager::GetInstance()->SetDefaultCamera(&camera);
            auto* lights = LightManager::GetInstance();
            lights->ApplyLightingPreset(VisualPresetLibrary::GetInstance().GetLighting("stage03"));
            lights->SetAmbientColor({1, 1, 1}); lights->SetAmbientIntensity(0.5f); lights->SetIntensity(0.8f);
            OffscreenRenderer offscreen; offscreen.Initialize(); offscreen.SetClearColor({0.025f, 0.03f, 0.05f, 1});
            PostEffectManager post; post.Initialize(dxCommon);
            post.SetNormalTextureHandle(offscreen.GetNormalSrvHandleGPU()); post.UpdateCameraInputs(&camera);
            post.SetIndirectTextureHandle(offscreen.GetIndirectSrvHandleGPU());
            auto* copy = post.GetCopyImageRenderer();
            ScreenSpaceReflection reflection; reflection.Initialize();
            MotionVectorRenderer motion; motion.Initialize();
            RequireSsr(reflection.IsEnabled(), "SSR must be enabled by default");
            ScreenSpaceReflectionSettings settings;
            settings.isEnabled = true; settings.maxDistance = 60; settings.thickness = 2;
            Object3d floor;
            floor.Initialize(Object3dManager::GetInstance());
            floor.SetModel(ModelManager::GetInstance()->CreatePlane("resources/Textures/white.png"));
            floor.SetRotate({std::numbers::pi_v<float> / 2, 0, 0}); floor.SetScale({60, 70, 1}); floor.SetTranslate({0, 0, 10});
            floor.SetMaterial("resources/Shaders/Object3D/StageIceFloor"); floor.SetColor({0.15f, 0.2f, 0.3f, 1});
            floor.SetSurfaceProperties(0.18f, 0, 0.65f); floor.Update();
            Object3d box;
            box.Initialize(Object3dManager::GetInstance()); box.SetModel(ModelManager::GetInstance()->CreateCube("resources/Textures/white.png"));
            box.SetMaterial("resources/Shaders/Object3D/ShadowStandard"); box.SetColor({1, 0.025f, 0.025f, 1});
            box.SetTranslate({-4, 3, 5}); box.SetScale({3, 6, 3}); box.SetSurfaceProperties(0.8f, 0, 0); box.Update();
            Object3d pillar;
            pillar.Initialize(Object3dManager::GetInstance()); pillar.SetModel(ModelManager::GetInstance()->CreateCube("resources/Textures/white.png"));
            pillar.SetMaterial("resources/Shaders/Object3D/ShadowStandard"); pillar.SetColor({0.025f, 1, 0.025f, 1});
            pillar.SetTranslate({4, 5, 10}); pillar.SetScale({2, 10, 2}); pillar.SetSurfaceProperties(0.8f, 0, 0); pillar.Update();
            Object3d rail;
            rail.Initialize(Object3dManager::GetInstance()); rail.SetModel(ModelManager::GetInstance()->CreateCube("resources/Textures/white.png"));
            rail.SetMaterial("resources/Shaders/Object3D/ShadowStandard"); rail.SetColor({0.025f, 0.025f, 1, 1});
            rail.SetTranslate({0, 6, 15}); rail.SetScale({4, 0.12f, 0.2f}); rail.SetSurfaceProperties(0.8f, 0, 0); rail.Update();
            Vector2 boxMirror = camera.WorldToScreen({-4, -3, 3.5f});
            Vector2 pillarMirror = camera.WorldToScreen({4, -5, 9});
            std::ofstream report(directory / "diagnostics.txt");
            RequireSsr(lights->GetLightingComponents().z == 1.0f, "Ice ambient gain must start at one");
            RequireSsr(!lights->SetLightingComponents(-1, 1, 1, 0), "Invalid direct lighting strength was accepted");
            RequireSsr(!lights->SetLightingComponents(1, 1, 1, 3), "Invalid lighting view was accepted");
#if defined(ENABLE_DEVELOPMENT_TOOLS)
            RequireSsr(post.SetDevelopmentNumber("lightingView", 1), "Lighting view panel control failed");
            RequireSsr(!post.SetDevelopmentNumber("lightingView", 1.5), "Lighting view accepted a fractional selection");
            RequireSsr(post.GetDevelopmentSettings().at("lightingView").get<float>() == 1, "Lighting view panel state does not match");
            RequireSsr(post.SetDevelopmentNumber("lightingView", 0), "Lighting view panel could not restore combined mode");
#endif
            std::array<std::vector<uint8_t>, 6> lightingFrames;
            const char* kLightingNames[] = {"lighting-combined", "lighting-direct", "lighting-ambient", "lighting-legacy-gain", "lighting-no-ambient", "lighting-no-direct"};
            for (uint32_t lightingIndex = 0; lightingIndex < 6; ++lightingIndex) {
                uint32_t viewMode = 0;
                float directStrength = 1;
                float indirectStrength = 1;
                float iceMultiplier = 1;
                if (lightingIndex == 1 || lightingIndex == 2) { viewMode = lightingIndex; }
                if (lightingIndex == 3) { iceMultiplier = 1.7f; }
                if (lightingIndex == 4) { indirectStrength = 0; }
                if (lightingIndex == 5) { directStrength = 0; }
                RequireSsr(lights->SetLightingComponents(directStrength, indirectStrength, iceMultiplier, viewMode), "Lighting settings failed");
                TextureManager::GetInstance()->FlushUploads(); SrvManager::GetInstance()->PreDraw();
                post.PreDrawDepth(); offscreen.PreDraw(post.GetDepthDSVHandle());
                Object3dManager::GetInstance()->PreDraw(); floor.Draw(); box.Draw(); pillar.Draw(); rail.Draw();
                offscreen.PostDraw(); post.PostDrawDepth(); dxCommon->PreDraw();
                dxCommon->SetBackBufferRenderTarget(dxCommon->GetDSVHandle());
                copy->SetPostEffectType(PostEffectType::Copy);
                copy->Draw(offscreen.GetSrvHandleGPU(), post.GetDepthSrv(), offscreen.GetNormalSrvHandleGPU());
                lightingFrames[lightingIndex] = CaptureSsrFrame(dxCommon, directory / (std::string(kLightingNames[lightingIndex]) + ".png"));
            }
            float maximumLightingError = 0;
            for (uint32_t pixelY = 440; pixelY < 480; ++pixelY) {
                for (uint32_t pixelX = 600; pixelX < 680; ++pixelX) {
                    size_t offset = (static_cast<size_t>(pixelY) * WinApp::kClientWidth + pixelX) * 4;
                    for (uint32_t channel = 0; channel < 3; ++channel) {
                        float combined = DecodeSrgb(lightingFrames[0][offset + channel]);
                        float components = DecodeSrgb(lightingFrames[1][offset + channel]) + DecodeSrgb(lightingFrames[2][offset + channel]);
                        float error = std::abs(combined - components);
                        if (error > maximumLightingError) { maximumLightingError = error; }
                    }
                }
            }
            report << "Direct + ambient composition error: " << maximumLightingError << '\n';
            RequireSsr(maximumLightingError < 0.005f, "Direct and ambient sources do not sum to combined floor lighting");
            RequireSsr(lightingFrames[1] == lightingFrames[4], "Ambient strength zero did not isolate direct lighting");
            RequireSsr(lightingFrames[2] == lightingFrames[5], "Direct strength zero did not isolate ambient lighting");
            RequireSsr(ReflectionDifference(lightingFrames[0], lightingFrames[3]) > 1000, "Ice ambient gain control has no effect");
            GpuTimestampTimer aoTimer;
            aoTimer.Initialize();
            std::array<std::vector<uint8_t>, 4> aoFrames;
            const char* kAoNames[] = { "ssao-combined-off", "ssao-combined-on", "ssao-direct-off", "ssao-direct-on" };
            for (uint32_t frameIndex = 0; frameIndex < 4; ++frameIndex) {
                float indirectStrength = 1;
                if (frameIndex >= 2) { indirectStrength = 0; }
                lights->SetLightingComponents(1, indirectStrength, 1, 0);
                TextureManager::GetInstance()->FlushUploads(); SrvManager::GetInstance()->PreDraw();
                post.PreDrawDepth(); offscreen.PreDraw(post.GetDepthDSVHandle());
                Object3dManager::GetInstance()->PreDraw(); floor.Draw(); box.Draw(); pillar.Draw(); rail.Draw();
                offscreen.PostDraw(); post.PostDrawDepth(); dxCommon->PreDraw();
                dxCommon->SetBackBufferRenderTarget(dxCommon->GetDSVHandle());
                auto& parameters = copy->GetPostEffectParameter();
                parameters.ssaoSettings = { 0.6f, 5.0f, 0.02f, 0 };
                if (frameIndex == 1 || frameIndex == 3) { parameters.ssaoSettings.w = 1; }
                parameters.atmosphereSettings.x = 0;
                parameters.screenCameraSettings.w = 1;
                copy->SetPostEffectType(PostEffectType::ScreenLighting);
                aoTimer.Begin();
                copy->Draw(offscreen.GetSrvHandleGPU(), post.GetDepthSrv(), offscreen.GetNormalSrvHandleGPU());
                aoTimer.End();
                aoFrames[frameIndex] = CaptureSsrFrame(dxCommon, directory / (std::string(kAoNames[frameIndex]) + ".png"));
                aoTimer.ReadCompleted();
                report << kAoNames[frameIndex] << " GPU ms: " << aoTimer.GetDurationMs() << '\n';
            }
            RequireSsr(aoFrames[2] == aoFrames[3], "SSAO changed direct light with ambient disabled");
            size_t occludedPixels = 0;
            for (size_t offset = 0; offset < aoFrames[0].size(); offset += 4) {
                for (size_t channel = 0; channel < 3; ++channel) {
                    RequireSsr(aoFrames[1][offset + channel] <= aoFrames[0][offset + channel], "SSAO unexpectedly brightened a pixel");
                }
                if (aoFrames[1][offset] < aoFrames[0][offset]) { ++occludedPixels; }
            }
            RequireSsr(occludedPixels > 100, "Ambient-only SSAO did not darken contact regions");
            report << "Ambient-only SSAO affected pixels: " << occludedPixels << '\n';
            lights->SetLightingComponents(1, 1, 1, 0);
            report << "Box mirror center: " << boxMirror.x << ',' << boxMirror.y << "\nPillar mirror center: " << pillarMirror.x << ',' << pillarMirror.y << '\n';
            const ScreenSpaceReflectionDebugMode kModes[] = {ScreenSpaceReflectionDebugMode::None, ScreenSpaceReflectionDebugMode::Reflection,
                ScreenSpaceReflectionDebugMode::ViewDepth, ScreenSpaceReflectionDebugMode::RayDirection,
                ScreenSpaceReflectionDebugMode::HitCoordinates, ScreenSpaceReflectionDebugMode::HitStatus};
            const char* kNames[] = {"composite", "reflection", "depth", "direction", "hit-coordinates", "hit-status"};
            for (uint32_t modeIndex = 0; modeIndex < 8; ++modeIndex) {
                settings.isEnabled = modeIndex != 6;
                settings.shouldUseHierarchicalDepth = modeIndex != 7;
                settings.debugMode = ScreenSpaceReflectionDebugMode::None;
                if (modeIndex < 6) { settings.debugMode = kModes[modeIndex]; }
                if (modeIndex == 7) { settings.debugMode = ScreenSpaceReflectionDebugMode::Reflection; }
                reflection.SetSettings(settings);
                TextureManager::GetInstance()->FlushUploads(); SrvManager::GetInstance()->PreDraw();
                post.PreDrawDepth(); offscreen.PreDraw(post.GetDepthDSVHandle());
                motion.BeginFrame();
                Object3dManager::GetInstance()->PreDraw(); floor.Draw(); box.Draw(); pillar.Draw(); rail.Draw();
                motion.EndFrame(post.GetDepthDSVHandle());
                offscreen.PostDraw(); post.PostDrawDepth(); dxCommon->PreDraw();
                ScreenSpaceReflectionInputs inputs;
                inputs.colorSrv = offscreen.GetSrvHandleGPU(); inputs.depthSrv = post.GetDepthSrv();
                inputs.normalSrv = offscreen.GetNormalSrvHandleGPU(); inputs.camera = &camera;
                inputs.motionVectorSrv = motion.GetSrvHandle();
                auto result = reflection.Draw(inputs);
                if (!settings.isEnabled) {
                    RequireSsr(result.ptr == inputs.colorSrv.ptr, "SSR OFF did not bypass drawing");
                    RequireSsr(!reflection.HasHistory(), "SSR OFF must discard reflection history");
                }
                dxCommon->SetBackBufferRenderTarget(dxCommon->GetDSVHandle());
                copy->SetPostEffectType(PostEffectType::Copy);
                copy->Draw(result, inputs.depthSrv, inputs.normalSrv);
                std::string name = "off";
                if (modeIndex < 6) { name = kNames[modeIndex]; }
                if (modeIndex == 7) { name = "legacy-reflection"; }
                auto frame = CaptureSsrFrame(dxCommon, directory / (name + ".png")); reflection.ReadCompleted();
                if (modeIndex == 1) {
                    size_t bluePixels = CountReflectionColor(frame, camera.WorldToScreen({0, -6, 14.9f}), 2);
                    report << "Hi-Z thin rail pixels: " << bluePixels << '\n';
                    RequireSsr(bluePixels > 20, "Hi-Z missed the thin rail reflection");
                    size_t redPixels = CountReflectionColor(frame, boxMirror, 0);
                    size_t greenPixels = CountReflectionColor(frame, pillarMirror, 1);
                    report << "Red mirror pixels: " << redPixels << "\nGreen mirror pixels: " << greenPixels << "\nSSR GPU ms: " << reflection.GetGpuTimeMs() << '\n';
                    RequireSsr(redPixels > 500, "Box reflection is missing at its mirrored center");
                    RequireSsr(greenPixels > 500, "Pillar reflection is missing at its mirrored center");
                    RequireSsr(CountReflectionColor(frame, camera.WorldToScreen({-4, -5, 3.5f}), 0) > 100,
                        "Box reflection does not extend above its base");
                    RequireSsr(CountReflectionColor(frame, camera.WorldToScreen({4, -8, 9}), 1) > 100,
                        "Pillar reflection does not extend above its base");
                }
                if (modeIndex == 7) {
                    report << "Legacy thin rail pixels: " << CountReflectionColor(frame, camera.WorldToScreen({0, -6, 14.9f}), 2)
                        << "\nLegacy GPU ms: " << reflection.GetGpuTimeMs() << '\n';
                }
            }
            // Compare jittered sequences from identical scene inputs with history ON/OFF.
            ScreenSpaceReflection rawReflection; rawReflection.Initialize();
            settings.isEnabled = true;
            settings.shouldUseHierarchicalDepth = true;
            settings.shouldUseTemporalHistory = true;
            settings.shouldBlurReflection = false;
            settings.debugMode = ScreenSpaceReflectionDebugMode::Reflection;
            reflection.SetSettings(settings); reflection.ResetHistory();
            ScreenSpaceReflectionSettings rawSettings = settings;
            rawSettings.shouldUseTemporalHistory = false;
            rawReflection.SetSettings(rawSettings);
            std::vector<uint8_t> previousTemporal;
            std::vector<uint8_t> previousRaw;
            uint64_t temporalVariation = 0;
            uint64_t rawVariation = 0;
            uint64_t historyDifference = 0;
            for (uint32_t frameIndex = 0; frameIndex < 20; ++frameIndex) {
                Vector2 jitter = {0.00065f, 0.0012f};
                if (frameIndex % 2 == 0) { jitter = {-0.00065f, -0.0012f}; }
                camera.SetProjectionJitter(jitter); camera.Update();
                if (frameIndex == 16) {
                    box.SetTranslate({-10, 3, 5});
                    camera.SetProjectionJitter({});
                }
                if (frameIndex == 17) { camera.ResetMotionHistory(); }
                box.Update(); floor.Update(); pillar.Update(); rail.Update();
                post.UpdateCameraInputs(&camera);
                TextureManager::GetInstance()->FlushUploads(); SrvManager::GetInstance()->PreDraw();
                post.PreDrawDepth(); offscreen.PreDraw(post.GetDepthDSVHandle());
                motion.BeginFrame(); Object3dManager::GetInstance()->PreDraw();
                floor.Draw(); box.Draw(); pillar.Draw(); rail.Draw();
                motion.EndFrame(post.GetDepthDSVHandle());
                offscreen.PostDraw(); post.PostDrawDepth(); dxCommon->PreDraw();
                ScreenSpaceReflectionInputs inputs;
                inputs.colorSrv = offscreen.GetSrvHandleGPU(); inputs.depthSrv = post.GetDepthSrv();
                inputs.normalSrv = offscreen.GetNormalSrvHandleGPU(); inputs.motionVectorSrv = motion.GetSrvHandle(); inputs.camera = &camera;
                if (frameIndex >= 18) { inputs.sceneRevision = 1; }
                if (frameIndex == 19) { inputs.motionVectorSrv = {}; }
                auto rawResult = rawReflection.Draw(inputs);
                auto temporalResult = reflection.Draw(inputs);
                dxCommon->SetBackBufferRenderTarget(dxCommon->GetDSVHandle());
                copy->SetPostEffectType(PostEffectType::Copy); copy->Draw(temporalResult, inputs.depthSrv, inputs.normalSrv);
                auto temporalFrame = CaptureSsrFrame(dxCommon, directory / ("temporal-" + std::to_string(frameIndex) + ".png"));
                reflection.ReadCompleted(); rawReflection.ReadCompleted();
                if (frameIndex == 15) {
                    report << "Temporal GPU ms: " << reflection.GetGpuTimeMs() << "\nRaw GPU ms: " << rawReflection.GetGpuTimeMs() << '\n';
                }
                SrvManager::GetInstance()->PreDraw(); dxCommon->PreDraw();
                copy->Draw(rawResult, inputs.depthSrv, inputs.normalSrv);
                auto rawFrame = CaptureSsrFrame(dxCommon, directory / ("raw-" + std::to_string(frameIndex) + ".png"));
                if (frameIndex >= 4 && frameIndex < 16) {
                    temporalVariation += ReflectionDifference(temporalFrame, previousTemporal);
                    rawVariation += ReflectionDifference(rawFrame, previousRaw);
                    historyDifference += ReflectionDifference(temporalFrame, rawFrame);
                }
                if (frameIndex == 16) {
                    RequireSsr(CountReflectionColor(temporalFrame, boxMirror, 0) < 20, "Moving box left a reflection trail");
                }
                if (frameIndex == 17 || frameIndex == 18 || frameIndex == 19) {
                    RequireSsr(ReflectionDifference(temporalFrame, rawFrame) == 0, "Invalidated history was reused");
                }
                previousTemporal = std::move(temporalFrame); previousRaw = std::move(rawFrame);
            }
            report << "Raw sequence variation: " << rawVariation << "\nTemporal sequence variation: " << temporalVariation
                << "\nHistory difference: " << historyDifference << '\n';
            RequireSsr(historyDifference > 1000, "Temporal resolve never used valid history");
            RequireSsr(temporalVariation < rawVariation, "Temporal resolve did not reduce jitter variation");
            RequireSsr(!reflection.HasHistory(), "Missing motion vectors must invalidate history");
            camera.SetProjectionJitter({});
            // Compare material roughness at identical geometry and camera settings.
            box.SetTranslate({-4, 3, 5}); box.Update();
            settings.shouldUseTemporalHistory = false;
            settings.debugMode = ScreenSpaceReflectionDebugMode::None;
            const float kRoughnessValues[] = {0.08f, 0.18f, 0.75f};
            uint64_t blurDifferences[3] = {};
            for (uint32_t roughnessIndex = 0; roughnessIndex < 3; ++roughnessIndex) {
                std::vector<uint8_t> sharpFrame;
                floor.SetSurfaceProperties(kRoughnessValues[roughnessIndex], 0, 0.65f); floor.Update();
                for (uint32_t blurIndex = 0; blurIndex < 2; ++blurIndex) {
                    settings.shouldBlurReflection = blurIndex != 0; reflection.SetSettings(settings);
                    post.UpdateCameraInputs(&camera);
                    TextureManager::GetInstance()->FlushUploads(); SrvManager::GetInstance()->PreDraw();
                    post.PreDrawDepth(); offscreen.PreDraw(post.GetDepthDSVHandle());
                    motion.BeginFrame(); Object3dManager::GetInstance()->PreDraw();
                    floor.Draw(); box.Draw(); pillar.Draw(); rail.Draw(); motion.EndFrame(post.GetDepthDSVHandle());
                    offscreen.PostDraw(); post.PostDrawDepth(); dxCommon->PreDraw();
                    ScreenSpaceReflectionInputs inputs;
                    inputs.colorSrv = offscreen.GetSrvHandleGPU(); inputs.depthSrv = post.GetDepthSrv();
                    inputs.normalSrv = offscreen.GetNormalSrvHandleGPU(); inputs.camera = &camera;
                    auto result = reflection.Draw(inputs);
                    dxCommon->SetBackBufferRenderTarget(dxCommon->GetDSVHandle());
                    copy->SetPostEffectType(PostEffectType::Copy); copy->Draw(result, inputs.depthSrv, inputs.normalSrv);
                    std::string name = "sharp-";
                    if (blurIndex != 0) { name = "blur-"; }
                    auto frame = CaptureSsrFrame(dxCommon, directory / (name + std::to_string(roughnessIndex) + ".png"));
                    reflection.ReadCompleted();
                    report << name << roughnessIndex << " GPU ms: " << reflection.GetGpuTimeMs() << '\n';
                    if (blurIndex == 0) { sharpFrame = std::move(frame); }
                    else {
                        blurDifferences[roughnessIndex] = ReflectionDifference(frame, sharpFrame);
                        RequireUnchangedForeground(frame, sharpFrame);
                    }
                }
                report << "Roughness " << kRoughnessValues[roughnessIndex] << " blur difference: " << blurDifferences[roughnessIndex] << '\n';
            }
            RequireSsr(blurDifferences[0] == 0, "Minimum material roughness must preserve sharp reflections");
            RequireSsr(blurDifferences[1] > 1000, "Ice roughness did not soften reflections");
            RequireSsr(blurDifferences[2] > blurDifferences[1], "Increasing roughness did not strengthen reflection blur");
            dxCommon->WaitForGPU();
        }
        if (info) {
            for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
                SIZE_T size = 0; info->GetMessage(index, nullptr, &size);
                std::vector<uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                info->GetMessage(index, message, &size);
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) { Logger::Log(message->pDescription); throw std::runtime_error("SSR D3D12 validation failed"); }
            }
        }
        std::ofstream(directory / "result.txt") << "PASS: direct/ambient composition and controls, ice ambient gain, material roughness blur, foreground edges preserved, temporal jitter reduction, default ON, no D3D12 errors\n";
        FinalizeSsrValidation(); return 0;
    } catch (const std::exception& error) {
        std::ofstream(directory / "result.txt") << "FAIL: " << error.what();
        Logger::Log(error.what()); Logger::Flush(); FinalizeSsrValidation(); return 1;
    }
}
