#pragma once

#include "Engine/Math/MathStruct.h"
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl.h>

class Camera;
class EnemyBullet;

class EnemyBulletTrailRenderer {
public:
    void Initialize();
    void Draw(const Camera& camera, const std::vector<std::unique_ptr<EnemyBullet>>& bullets);

private:
    struct Vertex {
        Vector3 position;
        float side;
        Vector4 color;
    };

    void AppendTrail(const EnemyBullet& bullet, const Vector3& cameraPosition, const Vector3& cameraRight);
    void ReserveUploadBuffer(size_t requiredBytes);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer_;
    std::byte* uploadData_ = nullptr;
    size_t uploadCapacityBytes_ = 0;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
};
