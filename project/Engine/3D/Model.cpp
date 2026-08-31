#include "Model.h"
#include "ModelCommon.h"
#include "Engine/TextureManager/TextureManager.h"
#include <cassert>
#include <cstring>

// ===============================================
// モデル初期化処理
// ===============================================
void Model::Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename)
{
    modelCommon_ = modelCommon;
    modelData_ = Object3d::LoadModeFile(directorypath, filename);
    CreateMeshResources();
}

void Model::Initialize(ModelCommon* modelCommon, const ModelData& modelData)
{
    modelCommon_ = modelCommon;
    modelData_ = modelData;
    CreateMeshResources();
}

void Model::CreateMeshResources()
{
    auto* dx = modelCommon_->GetDxCommon();

    for (MeshPrimitive& primitive : modelData_.primitives) {
        assert(!primitive.vertices.empty());

        // ===== Vertex Buffer =====
        primitive.vertexResource = dx->CreateBufferResource(sizeof(VertexData) * primitive.vertices.size());

        primitive.vertexResource->SetName(L"MeshPrimitive::VertexBuffer");

        VertexData* vtx = nullptr;
        primitive.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
        std::memcpy(vtx,
            primitive.vertices.data(),
            sizeof(VertexData) * primitive.vertices.size());

        primitive.vbView.BufferLocation = primitive.vertexResource->GetGPUVirtualAddress();
        primitive.vbView.SizeInBytes = UINT(sizeof(VertexData) * primitive.vertices.size());
        primitive.vbView.StrideInBytes = sizeof(VertexData);

        // ===== Index Buffer=====
        if (!primitive.indices.empty()) {
            primitive.indexResource = dx->CreateBufferResource(sizeof(uint32_t) * primitive.indices.size());

            uint32_t* idx = nullptr;
            primitive.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&idx));
            std::memcpy(idx,primitive.indices.data(),sizeof(uint32_t) * primitive.indices.size());

            primitive.ibView.BufferLocation = primitive.indexResource->GetGPUVirtualAddress();
            primitive.ibView.SizeInBytes = UINT(sizeof(uint32_t) * primitive.indices.size());
            primitive.ibView.Format = DXGI_FORMAT_R32_UINT;
        }
    }

    if (modelData_.materials.empty()) {
        modelData_.materials.push_back(MaterialData {});
    }

    for (MaterialData& material : modelData_.materials) {
        if (material.textureFilePath.empty()) {
            material.textureFilePath = "resources/Textures/BaseColor_Cube.png";
        }
        TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
    }
}

// ===============================================
// モデル描画処理
// ===============================================
void Model::Draw()
{
    auto* commandList = modelCommon_->GetDxCommon()->GetCommandList();

    for (const MeshPrimitive& primitive : modelData_.primitives) {

        // topology
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // vertex buffer
        commandList->IASetVertexBuffers(0, 1, &primitive.vbView);

        const MaterialData& material = GetMaterial(primitive.materialIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath);
        commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

        // draw
        if (!primitive.indices.empty()) {
            commandList->IASetIndexBuffer(&primitive.ibView);
            commandList->DrawIndexedInstanced(UINT(primitive.indices.size()), 1, 0, 0, 0);
        } else {
            commandList->DrawInstanced(UINT(primitive.vertices.size()), 1, 0, 0);
        }
    }
}

void Model::SetTexture(const std::string& filePath, uint32_t materialIndex)
{
    if (materialIndex >= modelData_.materials.size()) {
        modelData_.materials.resize(materialIndex + 1);
    }

    MaterialData& material = modelData_.materials[materialIndex];
    material.textureFilePath = filePath;
    TextureManager::GetInstance()->LoadTexture(filePath);
}

const std::string& Model::GetTexture(uint32_t materialIndex) const
{
    return GetMaterial(materialIndex).textureFilePath;
}

const MaterialData& Model::GetMaterial(uint32_t materialIndex) const
{
    assert(!modelData_.materials.empty());
    if (materialIndex >= modelData_.materials.size()) {
        return modelData_.materials[0];
    }
    return modelData_.materials[materialIndex];
}
