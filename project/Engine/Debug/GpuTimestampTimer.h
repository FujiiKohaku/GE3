#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <cstdint>

// ReadCompleted is called only after the submitted command list's fence completes.
class GpuTimestampTimer {
public:
    void Initialize() {
#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS) || defined(KOHAKU_RENDER_TESTS)
        auto* dx = DirectXCommon::GetInstance();
        D3D12_QUERY_HEAP_DESC description = {};
        description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        description.Count = 2;
        if (FAILED(dx->GetDevice()->CreateQueryHeap(&description, IID_PPV_ARGS(&queryHeap_)))) { return; }
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        auto buffer = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * 2);
        if (FAILED(dx->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)))) { return; }
        if (FAILED(dx->GetCommandQueue()->GetTimestampFrequency(&frequency_)) || frequency_ == 0) { return; }
        isAvailable_ = true;
#endif
    }
    void Begin() {
        hasSample_ = false;
        if (!isAvailable_) { return; }
        DirectXCommon::GetInstance()->GetCommandList()->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    }
    void End() {
        if (!isAvailable_) { return; }
        auto* commandList = DirectXCommon::GetInstance()->GetCommandList();
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
        commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, readback_.Get(), 0);
        hasPendingSample_ = true;
    }
    void ReadCompleted() {
        if (!hasPendingSample_) { return; }
        void* data = nullptr;
        D3D12_RANGE range = {0, sizeof(uint64_t) * 2};
        if (FAILED(readback_->Map(0, &range, &data))) { return; }
        const auto* timestamps = static_cast<const uint64_t*>(data);
        if (timestamps[1] >= timestamps[0]) {
            durationMs_ = static_cast<double>(timestamps[1] - timestamps[0]) * 1000.0 / static_cast<double>(frequency_);
            hasSample_ = true;
        }
        D3D12_RANGE written = {0, 0};
        readback_->Unmap(0, &written);
        hasPendingSample_ = false;
    }
    void ResetSample() { hasSample_ = false; durationMs_ = 0; }
    bool IsAvailable() const { return isAvailable_; }
    bool HasSample() const { return hasSample_; }
    double GetDurationMs() const { return durationMs_; }
private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    uint64_t frequency_ = 0;
    double durationMs_ = 0;
    bool isAvailable_ = false;
    bool hasPendingSample_ = false;
    bool hasSample_ = false;
};
