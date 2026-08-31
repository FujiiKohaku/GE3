#include "ImGuiManager.h"
std::unique_ptr<ImGuiManager> ImGuiManager::instance_;

#ifdef USE_IMGUI
namespace {
void AllocateImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    auto* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager != nullptr);
    assert(srvManager->CanAllocate());

    const uint32_t srvIndex = srvManager->Allocate();
    *outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
    *outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
}

void FreeImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE)
{
    SrvManager* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager != nullptr);
    bool wasFreed = srvManager->FreeByCPUHandle(cpuHandle);
    assert(wasFreed);
}
}
#endif

ImGuiManager* ImGuiManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<ImGuiManager>(ConstructorKey());
    }
    return instance_.get();
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    instance_.reset();
}

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager)
{
#ifdef USE_IMGUI

    // 保存
    winApp_ = winApp;
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // ImGuiコンテキスト生成
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Win32側の初期化
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // スタイル（好きなやつ）
    ImGui::StyleColorsClassic();

    // DirectX12側の初期化
    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = dxCommon_->GetDevice();
    initInfo.CommandQueue = dxCommon_->GetCommandQueue();
    initInfo.NumFramesInFlight = static_cast<int>(dxCommon_->GetSwapChainResourcesNum());
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    initInfo.SrvDescriptorHeap = srvManager_->GetDescriptorHeap();
    initInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
    initInfo.UserData = srvManager_;
    ImGui_ImplDX12_Init(&initInfo);
#endif
}

// ImGuiフレーム開始
void ImGuiManager::Begin()
{
#ifdef USE_IMGUI

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
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

ImGuiManager::ImGuiManager(ConstructorKey)
{
}
