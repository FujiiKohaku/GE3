#include "ParticleMeshManager.h"

void ParticleMeshManager::Initialize(DirectXCommon* dxCommon)
{
    boardMesh_ = std::make_unique<ParticleBoardMesh>();
    boardMesh_->Initialize(dxCommon);

    ringMesh_ = std::make_unique<ParticleRingMesh>();
    ringMesh_->Initialize(dxCommon);
    
    cylinderMesh_ = std::make_unique<ParticleCylinderMesh>();
    cylinderMesh_->Initialize(dxCommon);

}
const D3D12_VERTEX_BUFFER_VIEW& ParticleMeshManager::GetVertexBufferView(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetVertexBufferView();
    }

    if (meshType == ParticleMeshType::Cylinder) {
        return cylinderMesh_->GetVertexBufferView();
    }

    return boardMesh_->GetVertexBufferView();
}

const D3D12_INDEX_BUFFER_VIEW& ParticleMeshManager::GetIndexBufferView(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetIndexBufferView();
    }

    if (meshType == ParticleMeshType::Cylinder) {
        return cylinderMesh_->GetIndexBufferView();
    }

    return boardMesh_->GetIndexBufferView();
}

uint32_t ParticleMeshManager::GetIndexCount(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetIndexCount();
    }

    if (meshType == ParticleMeshType::Cylinder) {
        return cylinderMesh_->GetIndexCount();
    }

    return boardMesh_->GetIndexCount();
}