#pragma once

#include "Engine/Math/MathStruct.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

class WaterPillarRenderer {
public:
    void Initialize(Camera* camera);
    void Update(float deltaTime);
    void PreDraw();
    void Draw(const Vector3& position, const Vector3& scale, const Vector4& color);

private:
    struct Vertex {
        Vector4 position;
        Vector3 normal;
        Vector3 attributes;
    };

    struct Constants {
        Matrix4x4 viewProjection;
        Vector4 cameraPositionAndTime;
        Vector4 color;
        Vector4 pillarPositionAndRadius;
        Vector4 heightAndShape;
        std::array<float, 32> padding {};
    };
    static_assert(sizeof(Constants) == 256);

    void CreateMesh(uint32_t divisions);
    void CreateRootSignature();
    void CreatePipelineState();

    static constexpr uint32_t kMaxPillars = 32;
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
    Constants* constants_ = nullptr;
    uint32_t indexCount_ = 0;
    uint32_t drawIndex_ = 0;
    float time_ = 0.0f;
};
