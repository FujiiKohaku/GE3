#include "GPUProfiler.h"

#ifdef _DEBUG
#include "Engine/DirectXCommon/DirectXCommon.h"

void GPUProfiler::Initialize()
{
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    ID3D12CommandQueue* queue = DirectXCommon::GetInstance()->GetCommandQueue();
    if (!device || !queue) return;

    // クエリヒープの作成
    D3D12_QUERY_HEAP_DESC heapDesc {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = kMaxSections * 2;
    heapDesc.NodeMask = 0;
    HRESULT hr = device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_));
    if (FAILED(hr)) return;

    // リードバックバッファの作成
    D3D12_HEAP_PROPERTIES heapProps {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resDesc {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Alignment = 0;
    resDesc.Width = kMaxSections * 2 * sizeof(uint64_t);
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackBuffer_));
    if (FAILED(hr)) return;

    // 周波数の取得
    queue->GetTimestampFrequency(&frequency_);
    if (frequency_ == 0) {
        frequency_ = 1;
    }

    nameToIndex_.clear();
    times_.clear();
    activeSectionCount_ = 0;
}

void GPUProfiler::Finalize()
{
    queryHeap_.Reset();
    readbackBuffer_.Reset();
}

void GPUProfiler::Begin(const std::string& name, ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !queryHeap_) return;

    auto it = nameToIndex_.find(name);
    uint32_t index = 0;
    if (it == nameToIndex_.end()) {
        if (activeSectionCount_ >= kMaxSections) return;
        index = activeSectionCount_++;
        nameToIndex_[name] = index;
    } else {
        index = it->second;
    }

    commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index * 2);
}

void GPUProfiler::End(const std::string& name, ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !queryHeap_) return;

    auto it = nameToIndex_.find(name);
    if (it == nameToIndex_.end()) return;

    uint32_t index = it->second;
    commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index * 2 + 1);
}

void GPUProfiler::Resolve(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !queryHeap_ || !readbackBuffer_ || activeSectionCount_ == 0) return;

    commandList->ResolveQueryData(
        queryHeap_.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        activeSectionCount_ * 2,
        readbackBuffer_.Get(),
        0);
}

void GPUProfiler::Readback()
{
    if (!readbackBuffer_ || activeSectionCount_ == 0) return;

    void* mappedData = nullptr;
    D3D12_RANGE readRange { 0, activeSectionCount_ * 2 * sizeof(uint64_t) };
    HRESULT hr = readbackBuffer_->Map(0, &readRange, &mappedData);
    if (FAILED(hr)) return;

    uint64_t* timestamps = static_cast<uint64_t*>(mappedData);

    for (const auto& pair : nameToIndex_) {
        const std::string& name = pair.first;
        uint32_t index = pair.second;

        uint64_t start = timestamps[index * 2];
        uint64_t end = timestamps[index * 2 + 1];

        if (end >= start) {
            uint64_t delta = end - start;
            times_[name] = (static_cast<float>(delta) / static_cast<float>(frequency_)) * 1000.0f; // msに変換
        } else {
            times_[name] = 0.0f;
        }
    }

    D3D12_RANGE writeRange { 0, 0 };
    readbackBuffer_->Unmap(0, &writeRange);
}

float GPUProfiler::GetTimeMs(const std::string& name) const
{
    auto it = times_.find(name);
    if (it != times_.end()) {
        return it->second;
    }
    return 0.0f;
}
#endif
