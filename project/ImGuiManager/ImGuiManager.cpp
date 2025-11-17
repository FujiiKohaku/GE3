#include "ImGuiManager.h"
ImGuiManager::ImGuiManager()
{
}
ImGuiManager::~ImGuiManager()
{
}
void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager)
{
    // 保存
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    //  SRVマネージャからSRV確保
    uint32_t srvIndex = srvManager_->Allocate();

    //ImGuiコンテキスト生成
    ImGui::CreateContext();

    // Win32側の初期化 
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // スタイル（好きなやつ）
    ImGui::StyleColorsClassic();

    // DirectX12側の初期化
    ImGui_ImplDX12_Init(
        dxCommon_->GetDevice(),
        static_cast<int>(dxCommon_->GetSwapChainResourcesNum()),
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvManager_->GetDescriptorHeap(),
        srvManager_->GetCPUDescriptorHandle(srvIndex),
        srvManager_->GetGPUDescriptorHandle(srvIndex));
}

void ImGuiManager::Update()
{
    // Update ImGui state
}
void ImGuiManager::Draw()
{
    // Render ImGui elements
}
void ImGuiManager::Finalize()
{
    // Cleanup ImGui resources
}