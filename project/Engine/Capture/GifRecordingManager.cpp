#include "GifRecordingManager.h"
#include "GifWriter.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/Logger/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
constexpr int kWidth = 640, kHeight = 360, kFps = 15;
constexpr int kDurationOptions[] = {5, 10, 15, 30};
constexpr size_t kQueueLimit = 8;
void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("GIF capture GPU operation failed"); }
}
std::unique_ptr<GifRecordingManager> GifRecordingManager::instance_;
GifRecordingManager* GifRecordingManager::GetInstance() {
    if (!instance_) instance_.reset(new GifRecordingManager());
    return instance_.get();
}
void GifRecordingManager::Finalize() { instance_.reset(); }
GifRecordingManager::~GifRecordingManager() {
    StopRecording();
    if (worker_.joinable()) worker_.join();
}
void GifRecordingManager::Initialize(DirectXCommon* dx, WinApp* window) {
    dx_ = dx;
    notification_.Initialize("resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf");
    (void)window;
    notification_.SetPosition({static_cast<float>(WinApp::kClientWidth) - 24.0f, 58.0f});
    notification_.SetAnchorPoint({1.0f, 0.0f});
    notification_.SetFontSize(24.0f);
    notification_.SetColor({0.475f, 0.902f, 0.702f, 1.0f});
    notification_.SetOutlineColor({0, 0, 0, 1});
    notification_.SetOutlineWidth(1.0f);
    initialized_ = true;
}
double GifRecordingManager::Elapsed() const { return std::chrono::duration<double>(Clock::now() - started_).count(); }
void GifRecordingManager::Notify(const std::string& message) {
    notification_.SetText(message);
    notifyUntil_ = Clock::now() + std::chrono::seconds(4);
    Logger::Log(message);
}
void GifRecordingManager::ChangeRecordingDuration(int direction) {
    if (!initialized_) return;
    if (recording_ || worker_.joinable()) {
        Notify("GIF duration locked while recording / saving");
        return;
    }
    const auto current = std::find(std::begin(kDurationOptions), std::end(kDurationOptions), recordingSeconds_);
    const int index = static_cast<int>(current - std::begin(kDurationOptions));
    recordingSeconds_ = kDurationOptions[std::clamp(index + (direction < 0 ? -1 : 1), 0, 3)];
    Notify("GIF duration: " + std::to_string(recordingSeconds_) + "s (F7 - / F8 + / F9 record)");
}
void GifRecordingManager::ToggleRecording() {
    if (!initialized_) return;
    if (recording_) { StopRecording(); Notify("GIF saving..."); return; }
    // A joinable worker is joined only after it reports completion in Update.
    if (worker_.joinable()) { Notify("GIF saving..."); return; }
    try {
        auto directory = std::filesystem::current_path() / "generated" / "Captures";
        std::filesystem::create_directories(directory);
        auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto path = directory / ("Gameplay_" + std::to_string(stamp) + ".gif");
        { std::lock_guard lock(mutex_); queue_.clear(); stopping_ = false; finished_ = false; workerResult_.clear(); captureError_.clear(); endTimeCs_ = 0; }
        activeSeconds_ = recordingSeconds_;
        started_ = Clock::now(); nextCapture_ = 0;
        worker_ = std::thread(&GifRecordingManager::Encode, this, path);
        recording_ = true;
        Notify("GIF recording (F9: stop / max " + std::to_string(activeSeconds_) + "s)");
    } catch (const std::exception& e) { Notify(std::string("GIF failed: ") + e.what()); }
}
void GifRecordingManager::StopRecording() {
    if (!recording_) return;
    recording_ = false;
    { std::lock_guard lock(mutex_); stopping_ = true;
      endTimeCs_ = std::clamp(static_cast<int>(std::lround(Elapsed() * 100)), 1, activeSeconds_ * 100); }
    ready_.notify_one();
}
void GifRecordingManager::FailCapture(const std::string& message) {
    { std::lock_guard lock(mutex_); captureError_ = message; }
    StopRecording();
    Notify("GIF failed: " + message);
}
void GifRecordingManager::Update() {
    if (!initialized_) return;
    bool done;
    { std::lock_guard lock(mutex_); done = finished_; }
    if (done && worker_.joinable()) {
        recording_ = false;
        worker_.join();
        Notify(workerResult_);
    }
    if (recording_ && Elapsed() >= activeSeconds_) { StopRecording(); Notify("GIF saving..."); }
    if (recording_ || worker_.joinable() || Clock::now() < notifyUntil_) notification_.Update();
}
void GifRecordingManager::DrawNotification() {
    if (!initialized_ || (!recording_ && !worker_.joinable() && Clock::now() >= notifyUntil_)) return;
    TextRenderer::GetInstance()->PreDraw(); notification_.Draw();
}
void GifRecordingManager::PrepareCapture() {
    if (!recording_ || prepared_) return;
    const double elapsed = Elapsed();
    if (elapsed >= activeSeconds_) { StopRecording(); Notify("GIF saving..."); return; }
    if (elapsed < nextCapture_) return;
    { std::lock_guard lock(mutex_); if (queue_.size() >= kQueueLimit || finished_) return; }
    try {
        auto* source = dx_->GetCurrentBackBuffer();
        if (!source) throw std::runtime_error("GIF back buffer unavailable");
        const auto desc = source->GetDesc();
        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
            desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
            throw std::runtime_error("GIF back buffer format unsupported");
        if (!readback_ || sourceWidth_ != desc.Width || sourceHeight_ != desc.Height || sourceFormat_ != desc.Format) {
            readback_.Reset();
            dx_->GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &readbackSize_);
            D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer{};
            buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width = readbackSize_;
            buffer.Height = 1; buffer.DepthOrArraySize = 1; buffer.MipLevels = 1;
            buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            Check(dx_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)));
            sourceWidth_ = static_cast<UINT>(desc.Width); sourceHeight_ = desc.Height; sourceFormat_ = desc.Format;
        }
        auto* commands = dx_->GetCommandList();
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = source; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commands->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION from{}; from.pResource = source; from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION to{}; to.pResource = readback_.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = footprint_;
        commands->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); commands->ResourceBarrier(1, &barrier);
        captureTimeCs_ = static_cast<int>(std::lround(elapsed * 100));
        nextCapture_ = (std::floor(elapsed * kFps) + 1) / kFps;
        prepared_ = true;
    } catch (const std::exception& e) { FailCapture(e.what()); }
}
void GifRecordingManager::CompleteCapture() {
    if (!prepared_) return;
    prepared_ = false;
    void* mapped = nullptr;
    try {
        // Allocate before Map so allocation failure never leaves the resource mapped.
        Frame frame; frame.timeCs = captureTimeCs_; frame.rgba.resize(kWidth * kHeight * 4);
        D3D12_RANGE range{0, static_cast<SIZE_T>(readbackSize_)};
        Check(readback_->Map(0, &range, &mapped));
        auto* pixels = static_cast<const unsigned char*>(mapped) + footprint_.Offset;
        const bool bgra = sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM || sourceFormat_ == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        // Area averaging supports any back-buffer resolution, preserving aspect ratio with letterboxing.
        const double scale = (std::min)(double(kWidth) / sourceWidth_, double(kHeight) / sourceHeight_);
        const int w = (std::max)(1, int(sourceWidth_ * scale)), h = (std::max)(1, int(sourceHeight_ * scale));
        const int left = (kWidth - w) / 2, top = (kHeight - h) / 2;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const UINT x0 = x * sourceWidth_ / w, y0 = y * sourceHeight_ / h;
            const UINT x1 = (std::min)(sourceWidth_, (std::max)(x0 + 1, UINT((x + 1) * sourceWidth_ / w)));
            const UINT y1 = (std::min)(sourceHeight_, (std::max)(y0 + 1, UINT((y + 1) * sourceHeight_ / h)));
            unsigned int sum[3]{}; unsigned int count = 0;
            for (UINT sy = y0; sy < y1; ++sy) for (UINT sx = x0; sx < x1; ++sx) {
                const auto* p = pixels + sy * footprint_.Footprint.RowPitch + sx * 4;
                sum[0] += p[bgra ? 2 : 0]; sum[1] += p[1]; sum[2] += p[bgra ? 0 : 2]; ++count;
            }
            auto* out = frame.rgba.data() + ((y + top) * kWidth + x + left) * 4;
            for (int c = 0; c < 3; ++c) out[c] = static_cast<unsigned char>(sum[c] / count);
            out[3] = 255;
        }
        D3D12_RANGE written{0, 0}; readback_->Unmap(0, &written); mapped = nullptr;
        { std::lock_guard lock(mutex_); if (!finished_) queue_.push_back(std::move(frame)); }
        ready_.notify_one();
    } catch (const std::exception& e) {
        if (mapped) { D3D12_RANGE written{0, 0}; readback_->Unmap(0, &written); }
        FailCapture(e.what());
    }
}
void GifRecordingManager::Encode(std::filesystem::path path) {
    GifWriter gif;
    std::string result;
    try {
        gif.Begin(path, kWidth, kHeight);
        std::vector<unsigned char> previous;
        int previousTime = 0;
        for (;;) {
            Frame frame;
            int end = 0;
            { std::unique_lock lock(mutex_);
              ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
              if (queue_.empty()) { end = endTimeCs_; }
              else { frame = std::move(queue_.front()); queue_.pop_front(); }
            }
            if (frame.rgba.empty()) {
                { std::lock_guard lock(mutex_); if (!captureError_.empty()) throw std::runtime_error(captureError_); }
                if (previous.empty()) throw std::runtime_error("No GIF frames captured");
                gif.AddFrame(previous, static_cast<uint16_t>((std::max)(1, end - previousTime)));
                break;
            }
            if (!previous.empty()) gif.AddFrame(previous, static_cast<uint16_t>((std::max)(1, frame.timeCs - previousTime)));
            else frame.timeCs = 0;
            previous.resize(kWidth * kHeight);
            static constexpr int dither[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
            for (int y = 0; y < kHeight; ++y) for (int x = 0; x < kWidth; ++x) {
                const auto* p = frame.rgba.data() + (y * kWidth + x) * 4;
                const int offset = (dither[(y % 4) * 4 + x % 4] - 8) * 2;
                previous[y * kWidth + x] = GifWriter::PaletteIndex(p[0] + offset, p[1] + offset, p[2] + offset);
            }
            previousTime = frame.timeCs;
        }
        gif.Finish();
        result = "GIF saved: " + path.filename().string();
    } catch (const std::exception& e) {
        gif.Abort();
        std::error_code ec; std::filesystem::remove(path, ec);
        result = std::string("GIF failed: ") + e.what();
    }
    { std::lock_guard lock(mutex_); workerResult_ = std::move(result); finished_ = true; queue_.clear(); }
}
