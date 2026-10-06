#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Camera/Camera.h"
#include <map>
#include <vector>

struct MotionVectorSettings {
    bool isEnabled = true;
    bool isDebugVisible = false;
};

struct MotionVectorHistory {
    Matrix4x4 previousWorldViewProjection = {};
    Vector4 previousParameters = {};
    uint64_t frameId = 0;
    uint64_t cameraHistoryId = 0;
    const Camera* camera = nullptr;
    void Reset() { frameId = 0; camera = nullptr; }
};

// Captures only geometry actually submitted by the scene's normal draw.
class MotionVectorRenderer {
public:
    ~MotionVectorRenderer();
    void Initialize();
    void BeginFrame();
    void EndFrame(D3D12_CPU_DESCRIPTOR_HANDLE depthHandle);
    void DrawDebug();
    void ResetHistory();
    void SetSettings(const MotionVectorSettings& settings) { settings_ = settings; }
    const MotionVectorSettings& GetSettings() const { return settings_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const;
    ID3D12Resource* GetTexture() const { return texture_.Get(); }
    static MotionVectorRenderer* GetActive() { return active_; }
    bool HasHistory(const MotionVectorHistory& history, const Camera& camera) const;
    bool IsQueued(const MotionVectorHistory& history) const { return history.frameId == frameId_; }
    void CommitHistory(MotionVectorHistory& history, const Matrix4x4& world, const Camera& camera, const Vector4& parameters);
    void Queue(const D3D12_VERTEX_BUFFER_VIEW& currentVertices,
        const D3D12_VERTEX_BUFFER_VIEW& previousVertices,
        const D3D12_INDEX_BUFFER_VIEW& indices, uint32_t vertexCount, uint32_t indexCount,
        const Matrix4x4& world, const Camera& camera, MotionVectorHistory& history,
        const std::wstring& shaderPath, const Vector4& parameters, bool isDoubleSided = false);
    void QueueVertexHistoryCopy(ID3D12Resource* currentVertices, ID3D12Resource* previousVertices);
private:
    struct DrawEntry {
        D3D12_VERTEX_BUFFER_VIEW vertices[2] = {};
        D3D12_INDEX_BUFFER_VIEW indices = {};
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        Matrix4x4 currentWorldViewProjection = {};
        Matrix4x4 previousWorldViewProjection = {};
        Vector4 parameters = {};
        Vector4 previousParameters = {};
        std::wstring shaderPath;
        bool isDoubleSided = false;
        Vector2 jitterNdc = {};
    };
    struct VertexHistoryCopy {
        ID3D12Resource* currentVertices = nullptr;
        ID3D12Resource* previousVertices = nullptr;
    };
    ID3D12PipelineState* GetPipeline(const std::wstring& shaderPath, bool isDoubleSided);
    inline static MotionVectorRenderer* active_ = nullptr;
    inline static uint64_t nextFrameId_ = 1;
    uint64_t frameId_ = 0;
    uint64_t previousFrameId_ = 0;
    MotionVectorSettings settings_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> debugRoot_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> debugPipeline_;
    std::map<std::pair<std::wstring, bool>, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription_ = {};
    uint32_t srvIndex_ = UINT_MAX;
    std::vector<DrawEntry> draws_;
    std::vector<VertexHistoryCopy> copies_;
};
