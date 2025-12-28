#include "ImGuiManager.h"
ImGuiManager* ImGuiManager::instance = nullptr;

ImGuiManager* ImGuiManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ImGuiManager();
    }
    return instance;
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI

    // 後始末
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    delete instance;
    instance = nullptr;
}

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager)
{
#ifdef USE_IMGUI

    // 保存
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    //  SRVマネージャからSRV確保
    uint32_t srvIndex = srvManager_->Allocate();

    // ImGuiコンテキスト生成
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
#endif
}

// ImGuiフレーム開始
void ImGuiManager::Begin()
{
#ifdef USE_IMGUI

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

// ImGuiフレーム終了
void ImGuiManager::End()
{
#ifdef USE_IMGUI
    ImGui::Render();
#endif
}

void ImGuiManager::Update()
{
}
void ImGuiManager::Draw()
{
#ifdef USE_IMGUI

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    // デスクリプタヒープの配列をセットするコマンド
    ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    // 描画コマンドを発行
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif //  USE_IMGUI
}