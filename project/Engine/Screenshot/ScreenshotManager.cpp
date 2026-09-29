#include "ScreenshotManager.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Winapp/WinApp.h"
#include "DirectXTex/DirectXTex.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <d3d12.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <wincodec.h>

std::unique_ptr<ScreenshotManager> ScreenshotManager::instance_ = nullptr;

ScreenshotManager* ScreenshotManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<ScreenshotManager>(ConstructorKey {});
    }
    return instance_.get();
}

void ScreenshotManager::Finalize()
{
    instance_.reset();
}

ScreenshotManager::ScreenshotManager(ConstructorKey)
{
}

void ScreenshotManager::Initialize(DirectXCommon* dxCommon, WinApp* winApp)
{
    if (dxCommon == nullptr || winApp == nullptr) {
        Logger::Error("ScreenshotManager initialization failed");
        return;
    }

    dxCommon_ = dxCommon;
    winApp_ = winApp;

    notificationText_.Initialize(
        "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
    notificationText_.SetText("Screenshot copied");
    notificationText_.SetPosition({
        static_cast<float>(winApp_->GetClientWidth()) - 24.0f,
        24.0f
    });
    notificationText_.SetAnchorPoint({ 1.0f, 0.0f });
    notificationText_.SetFontSize(24.0f);
    notificationText_.SetColor({ 0.475f, 0.902f, 0.702f, 1.0f });
    notificationText_.SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    notificationText_.SetOutlineWidth(1.0f);
    notificationText_.SetShadowOffset({ 2.0f, 2.0f });

    initialized_ = true;
}

void ScreenshotManager::Update()
{
    if (!initialized_) {
        return;
    }

    if (notificationTimer_ > 0.0f) {
        notificationTimer_ -= TimeManager::GetInstance()->GetDeltaTime();
        if (notificationTimer_ < 0.0f) {
            notificationTimer_ = 0.0f;
        }
        notificationText_.Update();
    }
}

void ScreenshotManager::RequestCapture(bool savePng)
{
    if (!initialized_ || captureRequested_ || capturePrepared_) {
        return;
    }

    captureRequested_ = true;
    savePngRequested_ = savePng;
}

void ScreenshotManager::DrawNotification()
{
    if (!initialized_ || notificationTimer_ <= 0.0f) {
        return;
    }

    TextRenderer::GetInstance()->PreDraw();
    notificationText_.Draw();
}

void ScreenshotManager::PrepareCapture()
{
    if (!initialized_ || !captureRequested_) {
        return;
    }

    ID3D12Resource* backBuffer = dxCommon_->GetCurrentBackBuffer();
    if (backBuffer == nullptr || !CreateReadbackResource(backBuffer)) {
        captureRequested_ = false;
        savePngRequested_ = false;
        ShowFailureNotification();
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    D3D12_RESOURCE_BARRIER toCopySource {};
    toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopySource.Transition.pResource = backBuffer;
    toCopySource.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &toCopySource);

    D3D12_TEXTURE_COPY_LOCATION sourceLocation {};
    sourceLocation.pResource = backBuffer;
    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    sourceLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION destinationLocation {};
    destinationLocation.pResource = readbackResource_.Get();
    destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destinationLocation.PlacedFootprint.Offset = 0;
    destinationLocation.PlacedFootprint.Footprint.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    destinationLocation.PlacedFootprint.Footprint.Width = width_;
    destinationLocation.PlacedFootprint.Footprint.Height = height_;
    destinationLocation.PlacedFootprint.Footprint.Depth = 1;
    destinationLocation.PlacedFootprint.Footprint.RowPitch = rowPitch_;

    commandList->CopyTextureRegion(
        &destinationLocation,
        0,
        0,
        0,
        &sourceLocation,
        nullptr);

    D3D12_RESOURCE_BARRIER toRenderTarget {};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Transition.pResource = backBuffer;
    toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &toRenderTarget);

    captureRequested_ = false;
    capturePrepared_ = true;
}

void ScreenshotManager::CompleteCapture()
{
    if (!initialized_ || !capturePrepared_ || readbackResource_ == nullptr) {
        return;
    }

    void* mappedData = nullptr;
    D3D12_RANGE readRange {};
    readRange.Begin = 0;
    readRange.End = static_cast<SIZE_T>(readbackSize_);
    HRESULT result = readbackResource_->Map(0, &readRange, &mappedData);
    if (FAILED(result) || mappedData == nullptr) {
        Logger::Error("Screenshot readback mapping failed");
        capturePrepared_ = false;
        savePngRequested_ = false;
        readbackResource_.Reset();
        ShowFailureNotification();
        return;
    }

    const std::uint8_t* sourcePixels =
        static_cast<const std::uint8_t*>(mappedData);
    const bool clipboardSucceeded = CopyToClipboard(sourcePixels);

    bool pngSucceeded = true;
    if (savePngRequested_) {
        pngSucceeded = SavePng(sourcePixels);
    }

    D3D12_RANGE writtenRange {};
    writtenRange.Begin = 0;
    writtenRange.End = 0;
    readbackResource_->Unmap(0, &writtenRange);

    if (clipboardSucceeded && pngSucceeded) {
        ShowSuccessNotification();
    } else {
        ShowFailureNotification();
    }

    capturePrepared_ = false;
    savePngRequested_ = false;
    readbackResource_.Reset();
}

bool ScreenshotManager::CreateReadbackResource(ID3D12Resource* backBuffer)
{
    const D3D12_RESOURCE_DESC backBufferDescription = backBuffer->GetDesc();
    width_ = static_cast<std::uint32_t>(backBufferDescription.Width);
    height_ = backBufferDescription.Height;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
    UINT rowCount = 0;
    UINT64 rowSize = 0;
    UINT64 totalSize = 0;
    dxCommon_->GetDevice()->GetCopyableFootprints(
        &backBufferDescription,
        0,
        1,
        0,
        &footprint,
        &rowCount,
        &rowSize,
        &totalSize);

    if (width_ == 0 || height_ == 0 || totalSize == 0) {
        Logger::Error("Screenshot back buffer size is invalid");
        return false;
    }

    rowPitch_ = footprint.Footprint.RowPitch;
    readbackSize_ = totalSize;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDescription {};
    bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDescription.Width = totalSize;
    bufferDescription.Height = 1;
    bufferDescription.DepthOrArraySize = 1;
    bufferDescription.MipLevels = 1;
    bufferDescription.SampleDesc.Count = 1;
    bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDescription,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackResource_));
    if (FAILED(result)) {
        Logger::Error("Screenshot readback resource creation failed");
        return false;
    }

    readbackResource_->SetName(L"Screenshot Readback Buffer");
    return true;
}

bool ScreenshotManager::CopyToClipboard(const std::uint8_t* sourcePixels)
{
    if (!OpenClipboard(winApp_->GetHwnd())) {
        Logger::Error("Could not open the Windows clipboard");
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        Logger::Error("Could not clear the Windows clipboard");
        return false;
    }

    const SIZE_T pixelDataSize =
        static_cast<SIZE_T>(width_) * static_cast<SIZE_T>(height_) * 4;
    const SIZE_T allocationSize = sizeof(BITMAPV5HEADER) + pixelDataSize;
    HGLOBAL clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, allocationSize);
    if (clipboardMemory == nullptr) {
        CloseClipboard();
        Logger::Error("Could not allocate screenshot clipboard memory");
        return false;
    }

    void* clipboardData = GlobalLock(clipboardMemory);
    if (clipboardData == nullptr) {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        Logger::Error("Could not lock screenshot clipboard memory");
        return false;
    }

    BITMAPV5HEADER* header = static_cast<BITMAPV5HEADER*>(clipboardData);
    std::memset(header, 0, sizeof(BITMAPV5HEADER));
    header->bV5Size = sizeof(BITMAPV5HEADER);
    header->bV5Width = static_cast<LONG>(width_);
    header->bV5Height = -static_cast<LONG>(height_);
    header->bV5Planes = 1;
    header->bV5BitCount = 32;
    header->bV5Compression = BI_BITFIELDS;
    header->bV5SizeImage = static_cast<DWORD>(pixelDataSize);
    header->bV5RedMask = 0x00FF0000;
    header->bV5GreenMask = 0x0000FF00;
    header->bV5BlueMask = 0x000000FF;
    header->bV5AlphaMask = 0xFF000000;
    header->bV5CSType = LCS_sRGB;

    std::uint8_t* destinationPixels =
        static_cast<std::uint8_t*>(clipboardData) + sizeof(BITMAPV5HEADER);
    for (std::uint32_t y = 0; y < height_; ++y) {
        const std::uint8_t* sourceRow = sourcePixels +
            static_cast<std::size_t>(y) * rowPitch_;
        std::uint8_t* destinationRow = destinationPixels +
            static_cast<std::size_t>(y) * width_ * 4;
        for (std::uint32_t x = 0; x < width_; ++x) {
            const std::size_t sourceOffset = static_cast<std::size_t>(x) * 4;
            destinationRow[sourceOffset + 0] = sourceRow[sourceOffset + 2];
            destinationRow[sourceOffset + 1] = sourceRow[sourceOffset + 1];
            destinationRow[sourceOffset + 2] = sourceRow[sourceOffset + 0];
            destinationRow[sourceOffset + 3] = 255;
        }
    }

    GlobalUnlock(clipboardMemory);
    if (SetClipboardData(CF_DIBV5, clipboardMemory) == nullptr) {
        GlobalFree(clipboardMemory);
        CloseClipboard();
        Logger::Error("Could not set screenshot clipboard data");
        return false;
    }

    CloseClipboard();
    Logger::Log("Screenshot copied to clipboard");
    return true;
}

bool ScreenshotManager::SavePng(const std::uint8_t* sourcePixels)
{
    const std::filesystem::path pngPath = CreatePngPath();
    if (pngPath.empty()) {
        return false;
    }

    DirectX::Image image {};
    image.width = width_;
    image.height = height_;
    image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    image.rowPitch = rowPitch_;
    image.slicePitch = static_cast<std::size_t>(rowPitch_) * height_;
    image.pixels = const_cast<std::uint8_t*>(sourcePixels);

    HRESULT result = DirectX::SaveToWICFile(
        image,
        DirectX::WIC_FLAGS_NONE,
        GUID_ContainerFormatPng,
        pngPath.c_str());
    if (FAILED(result)) {
        Logger::Error("Could not save screenshot PNG");
        return false;
    }

    Logger::Log(
        "Screenshot PNG saved: " + pngPath.string());
    return true;
}

std::filesystem::path ScreenshotManager::CreatePngPath() const
{
    const std::filesystem::path directory =
        std::filesystem::current_path() / "Screenshots";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        Logger::Error("Could not create Screenshots directory");
        return {};
    }

    const std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    const std::time_t currentTime =
        std::chrono::system_clock::to_time_t(now);
    std::tm localTime {};
    if (localtime_s(&localTime, &currentTime) != 0) {
        Logger::Error("Could not create screenshot timestamp");
        return {};
    }

    const long long milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;

    std::wstringstream fileName;
    fileName << L"Screenshot_"
             << std::put_time(&localTime, L"%Y%m%d_%H%M%S")
             << L"_"
             << std::setw(3)
             << std::setfill(L'0')
             << milliseconds
             << L".png";
    return directory / fileName.str();
}

void ScreenshotManager::ShowSuccessNotification()
{
    notificationText_.SetText("Screenshot copied");
    notificationText_.SetColor({ 0.475f, 0.902f, 0.702f, 1.0f });
    notificationTimer_ = kNotificationDuration;
}

void ScreenshotManager::ShowFailureNotification()
{
    notificationText_.SetText("Screenshot failed");
    notificationText_.SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
    notificationTimer_ = kNotificationDuration;
}
