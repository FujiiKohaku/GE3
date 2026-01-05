#pragma once

// ImGuiManager.hをインクルードすることでIMGUIの各種hがまとめてインクルードするようにする
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#endif // USE_IMGUI
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "WinApp.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class ImGuiManager {
public:
    // ===== Singleton Access =====
    static ImGuiManager* GetInstance();

    static ImGuiManager* instance;
    // ===== Main APIs =====
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    static void Finalize();

    void Begin();
    void End();
    void Draw();

private:
    // ===== Singleton Only =====
    ImGuiManager() = default;
    ~ImGuiManager() = default;
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

private:
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};