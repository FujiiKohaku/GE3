#include "PreviewCapture.h"

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "DirectXTex/DirectXTex.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <wincodec.h>

namespace {
void CheckCapture(HRESULT result, const char* message)
{
    if (FAILED(result)) {
        throw std::runtime_error(message);
    }
}
}

void PreviewCapture::Initialize(DirectXCommon* dx)
{
    dx_ = dx;
}

std::filesystem::path PreviewCapture::NewPath(const char* extension) const
{
    std::filesystem::path directory = "runtime/captures/ModelPreview";
    std::filesystem::create_directories(directory);
    auto tick = std::chrono::system_clock::now().time_since_epoch();
    auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(tick).count();
    return directory / ("preview_" + std::to_string(stamp) + extension);
}

void PreviewCapture::RequestPng()
{
    pngRequested_ = true;
}

void PreviewCapture::StartGif(int framesPerSecond, int frameCount)
{
    if (recording_) {
        return;
    }
    try {
        framesPerSecond_ = std::clamp(framesPerSecond, 5, 30);
        frameCount_ = std::clamp(frameCount, 2, 600);
        frameIndex_ = 0;
        gifPath_ = NewPath(".gif");
        gif_.Begin(gifPath_, 640, 360);
        recording_ = true;
        status_ = "Recording GIF...";
    } catch (const std::exception& error) {
        recording_ = false;
        status_ = error.what();
        gif_.Abort();
    }
}

float PreviewCapture::FrameSeconds() const
{
    return 1.0f / static_cast<float>(framesPerSecond_);
}

void PreviewCapture::StopGif()
{
    if (!recording_) {
        return;
    }
    recording_ = false;
    try {
        gif_.Finish();
        status_ = "Saved: " + std::filesystem::absolute(gifPath_).string();
    } catch (const std::exception& error) {
        status_ = error.what();
    }
}

void PreviewCapture::EnsureReadback(ID3D12Resource* source)
{
    if (readback_) {
        return;
    }
    D3D12_RESOURCE_DESC description = source->GetDesc();
    dx_->GetDevice()->GetCopyableFootprints(&description, 0, 1, 0,
        &footprint_, nullptr, nullptr, &readbackSize_);
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer {};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = readbackSize_;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    CheckCapture(dx_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)),
        "Could not allocate preview capture buffer");
}

void PreviewCapture::Prepare()
{
    if (!pngRequested_ && !recording_) {
        return;
    }
    try {
        ID3D12Resource* source = dx_->GetCurrentBackBuffer();
        EnsureReadback(source);
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = source;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        auto* commands = dx_->GetCommandList();
        commands->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION from {};
        from.pResource = source;
        from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION to {};
        to.pResource = readback_.Get();
        to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint = footprint_;
        commands->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commands->ResourceBarrier(1, &barrier);
        prepared_ = true;
    } catch (const std::exception& error) {
        StopGif();
        pngRequested_ = false;
        status_ = error.what();
    }
}

void PreviewCapture::Complete()
{
    if (!prepared_) {
        return;
    }
    prepared_ = false;
    void* mapped = nullptr;
    D3D12_RANGE range { 0, static_cast<SIZE_T>(readbackSize_) };
    HRESULT result = readback_->Map(0, &range, &mapped);
    if (FAILED(result)) {
        StopGif();
        pngRequested_ = false;
        status_ = "Could not read capture buffer";
        return;
    }
    const auto* pixels = static_cast<const std::uint8_t*>(mapped);
    UINT width = footprint_.Footprint.Width;
    UINT height = footprint_.Footprint.Height;
    UINT pitch = footprint_.Footprint.RowPitch;
    try {
        if (pngRequested_) {
            DirectX::Image image {};
            image.width = width;
            image.height = height;
            image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
            image.rowPitch = pitch;
            image.slicePitch = static_cast<std::size_t>(pitch) * height;
            image.pixels = const_cast<std::uint8_t*>(pixels);
            auto path = NewPath(".png");
            CheckCapture(DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE,
                GUID_ContainerFormatPng, path.c_str()), "Could not save PNG");
            status_ = "Saved: " + std::filesystem::absolute(path).string();
        }
        if (recording_) {
            std::vector<std::uint8_t> indices(640 * 360);
            // Box downsample, then ordered dithering before fixed-palette quantization.
            static constexpr int dither[16] = {
                0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5
            };
            for (UINT y = 0; y < 360; ++y) {
                for (UINT x = 0; x < 640; ++x) {
                    UINT sourceX = x * width / 640;
                    UINT sourceY = y * height / 360;
                    UINT nextX = (std::min)(sourceX + 1, width - 1);
                    UINT nextY = (std::min)(sourceY + 1, height - 1);
                    int channels[3] {};
                    for (int channel = 0; channel < 3; ++channel) {
                        channels[channel] = (pixels[sourceY * pitch + sourceX * 4 + channel] +
                            pixels[sourceY * pitch + nextX * 4 + channel] +
                            pixels[nextY * pitch + sourceX * 4 + channel] +
                            pixels[nextY * pitch + nextX * 4 + channel]) / 4;
                    }
                    int offset = (dither[(y % 4) * 4 + x % 4] - 8) * 2;
                    indices[y * 640 + x] = PreviewGif::PaletteIndex(
                        channels[0] + offset, channels[1] + offset, channels[2] + offset);
                }
            }
            // Distribute GIF's centisecond rounding so 15/30fps keep their duration.
            int beginTime = frameIndex_ * 100 / framesPerSecond_;
            int endTime = (frameIndex_ + 1) * 100 / framesPerSecond_;
            gif_.AddFrame(indices, static_cast<std::uint16_t>(endTime - beginTime));
            ++frameIndex_;
            if (frameIndex_ >= frameCount_) {
                StopGif();
            }
        }
    } catch (const std::exception& error) {
        StopGif();
        status_ = error.what();
    }
    pngRequested_ = false;
    D3D12_RANGE written { 0, 0 };
    readback_->Unmap(0, &written);
}
