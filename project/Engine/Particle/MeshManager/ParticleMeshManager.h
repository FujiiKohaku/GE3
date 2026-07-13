#pragma once
#include "ParticleBoardMesh.h"
#include "ParticleRingMesh.h"
#include "ParticleCylinderMesh.h"
#include "ParticleBoxMesh.h"
#include "ParticleSphereMesh.h"
#include "ParticleConeMesh.h"
#include <memory>

class DirectXCommon;

class ParticleMeshManager {
public:
    enum class ParticleMeshType {
        Board,
        Ring,
        Cylinder,
        Box,
        Sphere,
        Cone,
    };

public:
    void Initialize(DirectXCommon* dxCommon);

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(ParticleMeshType meshType) const;
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(ParticleMeshType meshType) const;
    uint32_t GetIndexCount(ParticleMeshType meshType) const;

private:
    std::unique_ptr<ParticleBoardMesh> boardMesh_;
    std::unique_ptr<ParticleRingMesh> ringMesh_;
    std::unique_ptr<ParticleCylinderMesh> cylinderMesh_;
    std::unique_ptr<ParticleBoxMesh> boxMesh_;
    std::unique_ptr<ParticleSphereMesh> sphereMesh_;
    std::unique_ptr<ParticleConeMesh> coneMesh_;
};