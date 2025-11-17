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
    ImGuiManager();
    ~ImGuiManager();
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Finalize();

    // ImGui受付開始
    void Begin();
    // Imgui受付終了
    void End();
    // 画面への描画
    void Draw();

private:
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};
