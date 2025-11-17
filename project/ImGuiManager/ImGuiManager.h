#pragma once
#include "DirectXCommon.h"
#include "WinApp.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "SrvManager.h"
class ImGuiManager {
public:
    ImGuiManager();
    ~ImGuiManager();
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw();
    void Finalize();

private:
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};
