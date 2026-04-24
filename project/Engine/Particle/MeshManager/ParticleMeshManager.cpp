#include "ParticleMeshManager.h"

void ParticleMeshManager::Initialize(DirectXCommon* dxCommon)
{
    boardMesh_ = std::make_unique<ParticleBoardMesh>();
    boardMesh_->Initialize(dxCommon);

    ringMesh_ = std::make_unique<ParticleRingMesh>();
    ringMesh_->Initialize(dxCommon);
}

const D3D12_VERTEX_BUFFER_VIEW& ParticleMeshManager::GetVertexBufferView(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetVertexBufferView();
    }

    return boardMesh_->GetVertexBufferView();
}

const D3D12_INDEX_BUFFER_VIEW& ParticleMeshManager::GetIndexBufferView(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetIndexBufferView();
    }

    return boardMesh_->GetIndexBufferView();
}

uint32_t ParticleMeshManager::GetIndexCount(ParticleMeshType meshType) const
{
    if (meshType == ParticleMeshType::Ring) {
        return ringMesh_->GetIndexCount();
    }

    return boardMesh_->GetIndexCount();
}