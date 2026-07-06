#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#ifdef _DEBUG
class GPUProfiler {
public:
    void Initialize();
    void Finalize();

    void Begin(const std::string& name, ID3D12GraphicsCommandList* commandList);
    void End(const std::string& name, ID3D12GraphicsCommandList* commandList);
    void Resolve(ID3D12GraphicsCommandList* commandList);
    void Readback();

    float GetTimeMs(const std::string& name) const;
    const std::unordered_map<std::string, float>& GetTimes() const { return times_; }

private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_ = nullptr;
    
    std::unordered_map<std::string, uint32_t> nameToIndex_;
    std::unordered_map<std::string, float> times_;
    
    uint64_t frequency_ = 1;
    static constexpr uint32_t kMaxSections = 8; // 最大8項目まで測定（クエリ数 16）
    uint32_t activeSectionCount_ = 0;
};
#else
class GPUProfiler {
public:
    void Initialize() {}
    void Finalize() {}
    void Begin(const std::string&, ID3D12GraphicsCommandList*) {}
    void End(const std::string&, ID3D12GraphicsCommandList*) {}
    void Resolve(ID3D12GraphicsCommandList*) {}
    void Readback() {}
};
#endif
