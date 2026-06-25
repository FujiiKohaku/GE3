#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "Engine/Logger/Logger.h"
#include <filesystem>
#include <string>

namespace {
constexpr float kImGuiFontSize = 19.0f;
const char* kNotoSansJpFontPaths[] = {
    "Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "project/Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../../Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
    "../../../project/Noto_Sans_JP/static/NotoSansJP-Regular.ttf",
};

void LogNotoSansJpMissing()
{
    std::string message = "Noto Sans JP font was not found. Searched paths:";

    for (const char* fontPath : kNotoSansJpFontPaths) {
        message += " ";
        message += fontPath;
    }

    Logger::Error(message);
}

void RegisterNotoSansJpFont(ImGuiIO& io)
{
    for (const char* fontPath : kNotoSansJpFontPaths) {
        if (!std::filesystem::exists(fontPath)) {
            continue;
        }

        ImFont* font = io.Fonts->AddFontFromFileTTF(
            fontPath,
            kImGuiFontSize,
            nullptr,
            io.Fonts->GetGlyphRangesJapanese());

        if (font != nullptr) {
            io.FontDefault = font;
            Logger::Log(std::string("Loaded ImGui font: ") + fontPath);
            return;
        }

        Logger::Error(std::string("Failed to load ImGui font file: ") + fontPath);
        return;
    }

    LogNotoSansJpMissing();
}
}
#endif

std::unique_ptr<ImGuiManager> ImGuiManager::instance_;

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

    //  SRVマネージャからSRV確保
    uint32_t srvIndex = srvManager_->Allocate();

    // ImGuiコンテキスト生成
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Win32側の初期化
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // スタイル（好きなやつ）
    ImGui::StyleColorsClassic();

    RegisterNotoSansJpFont(io);

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

ImGuiManager::ImGuiManager(ConstructorKey)
{
}
