#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Math/Object3DStruct.h"
#include <tuple>
#include <map>
#include <vector>

struct MotionVectorSettings {
    bool isEnabled = true;
    bool isDebugVisible = false;
};

struct MotionVectorHistory {
    Matrix4x4 previousWorldViewProjection = {};
    Matrix4x4 previousWorldView = {};
    Matrix4x4 previousNormalTransform = {};
    uint32_t surfaceId = 0;
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
    void ResizeSceneTargets();
    void BeginFrame();
    void EndFrame(D3D12_CPU_DESCRIPTOR_HANDLE depthHandle);
    void DrawDebug();
    void ResetHistory();
    void SetSettings(const MotionVectorSettings& settings) { settings_ = settings; }
    const MotionVectorSettings& GetSettings() const { return settings_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const;
    ID3D12Resource* GetTexture() const { return texture_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetReprojectionSrv() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetPreviousReprojectionSrv() const;
    ID3D12Resource* GetReprojectionTexture() const { return reprojectionTextures_[reprojectionIndex_].Get(); }
    uint64_t GetReprojectionAllocationBytes() const { return reprojectionAllocationBytes_; }
    static MotionVectorRenderer* GetActive() { return active_; }
    bool HasHistory(const MotionVectorHistory& history, const Camera& camera) const;
    bool IsQueued(const MotionVectorHistory& history) const { return history.frameId == frameId_; }
    void CommitHistory(MotionVectorHistory& history, const Matrix4x4& world, const Camera& camera, const Vector4& parameters);
    void Queue(const D3D12_VERTEX_BUFFER_VIEW& currentVertices,
        const D3D12_VERTEX_BUFFER_VIEW& previousVertices,
        const D3D12_INDEX_BUFFER_VIEW& indices, uint32_t vertexCount, uint32_t indexCount,
        const Matrix4x4& world, const Camera& camera, MotionVectorHistory& history,
        const std::wstring& shaderPath, const Vector4& parameters, bool isDoubleSided = false,
        const Material* material = nullptr, D3D12_GPU_DESCRIPTOR_HANDLE textureSrv = {});
    void QueueVertexHistoryCopy(ID3D12Resource* currentVertices, ID3D12Resource* previousVertices);
private:
    struct DrawEntry {
        D3D12_VERTEX_BUFFER_VIEW vertices[2] = {};
        D3D12_INDEX_BUFFER_VIEW indices = {};
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        Matrix4x4 currentWorldViewProjection = {};
        Matrix4x4 previousWorldViewProjection = {};
        Matrix4x4 previousWorldView = {};
        Matrix4x4 previousNormalTransform = {};
        uint32_t surfaceId = 0;
        Vector4 parameters = {};
        Vector4 previousParameters = {};
        std::wstring shaderPath;
        bool isDoubleSided = false;
        Vector2 jitterNdc = {};
        bool hasPreviousGeometry = false;
        Material material = {};
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrv = {};
    };
    struct VertexHistoryCopy {
        ID3D12Resource* currentVertices = nullptr;
        ID3D12Resource* previousVertices = nullptr;
    };
    ID3D12PipelineState* GetPipeline(const std::wstring& shaderPath, bool isDoubleSided, bool isAlphaMasked = false);
    inline static MotionVectorRenderer* active_ = nullptr;
    inline static uint64_t nextFrameId_ = 1;
    inline static uint32_t nextSurfaceId_ = 1;
    uint64_t frameId_ = 0;
    uint64_t previousFrameId_ = 0;
    MotionVectorSettings settings_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> debugRoot_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> debugPipeline_;
    std::map<std::tuple<std::wstring, bool, bool>, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialUpload_;
    Microsoft::WRL::ComPtr<ID3D12Resource> reprojectionUpload_;
    Microsoft::WRL::ComPtr<ID3D12Resource> reprojectionTextures_[2];
    uint32_t reprojectionSrvIndices_[2] = {UINT_MAX, UINT_MAX};
    uint32_t reprojectionIndex_ = 0;
    uint64_t reprojectionAllocationBytes_ = 0;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription_ = {};
    uint32_t srvIndex_ = UINT_MAX;
    std::vector<DrawEntry> draws_;
    std::vector<VertexHistoryCopy> copies_;
};
