#pragma once

// ImGuiManager.hをインクルードすることでIMGUIの各種hがまとめてインクルードするようにする
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif // USE_IMGUI
#include "Engine/Renderer/DirectXCommon/DirectXCommon.h"
#include "Engine/Resource/Srv/SrvManager.h"
#include "Engine/Core/WinApp/WinApp.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <wrl.h>
class ImGuiManager {
public:
    // ===== Singleton Access =====
    static ImGuiManager* GetInstance();

    // ===== Main APIs =====
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    static void Finalize();

    void Begin();
    void End();
    void Draw();

private:
    // ===== Singleton Only =====
    static std::unique_ptr<ImGuiManager> instance_;
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class ImGuiManager;
    };
    explicit ImGuiManager(ConstructorKey);
    ~ImGuiManager() = default;

private:
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};