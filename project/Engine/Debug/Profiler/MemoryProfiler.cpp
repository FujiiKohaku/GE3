#include "MemoryProfiler.h"

#ifdef _DEBUG
#include <Windows.h>
#include <psapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/SrvManager/SrvManager.h"

#pragma comment(lib, "psapi.lib")

void MemoryProfiler::Initialize()
{
    cpuMemoryBytes_ = 0;
    gpuMemoryBytes_ = 0;
    uploadHeapBytes_ = 0;
    defaultHeapBytes_ = 0;
    textureMemoryBytes_ = 0;
    modelMemoryBytes_ = 0;
    audioMemoryBytes_ = 0;
    srvCount_ = 0;
    descriptorHeapUsageRate_ = 0.0f;
}

void MemoryProfiler::Update()
{
    // 1. CPU物理メモリ使用量を取得 (WorkingSetSize)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        cpuMemoryBytes_ = pmc.WorkingSetSize;
    }

    // 2. GPU専用メモリ使用量を取得 (DXGI LOCAL SEGMENT)
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    if (device) {
        LUID luid = device->GetAdapterLuid();
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))) {
                Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
                if (SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3)))) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
                    if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo))) {
                        gpuMemoryBytes_ = memoryInfo.CurrentUsage;
                    }
                }
            }
        }
    }

    // 3. 各リソースメモリの概算
    // ※実稼働しているテクスチャやモデル、オーディオの概算値（またはPaging状況）を表示
    textureMemoryBytes_ = gpuMemoryBytes_ / 2; // 専用VRAMの約50%をテクスチャとみなす
    modelMemoryBytes_ = gpuMemoryBytes_ / 4;   // 専用VRAMの約25%をモデル(Vertex/IndexBuffer)とみなす
    audioMemoryBytes_ = cpuMemoryBytes_ / 10;  // CPUメモリの約10%をオーディオデータとみなす

    defaultHeapBytes_ = textureMemoryBytes_ + modelMemoryBytes_;
    uploadHeapBytes_ = defaultHeapBytes_ / 3;

    // 4. SRV使用量とDescriptorHeap使用率の算出
    auto* srvManager = SrvManager::GetInstance();
    if (srvManager) {
        srvCount_ = srvManager->GetAllocatedCount();
        if (SrvManager::kMaxSRVCount > 0) {
            descriptorHeapUsageRate_ = static_cast<float>(srvCount_) / static_cast<float>(SrvManager::kMaxSRVCount);
        }
    }
}
#endif
