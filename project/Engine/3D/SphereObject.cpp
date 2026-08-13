#include "SphereObject.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/TextureManager/TextureManager.h"
#include <cassert>
#include <numbers>
// ================================

// ================================
void SphereObject::Initialize(DirectXCommon* dxCommon, int subdivision, float radius)
{
    dxCommon_ = dxCommon;

    // ----------------

    // ----------------
    vertexCount_ = subdivision * subdivision * 6;
    vertices_.resize(vertexCount_);

    // 逅・ｽ謎ｽ懈・
    GenerateSphereVertices(vertices_.data(), subdivision, radius);

    // ----------------
    // VertexBuffer
    // ----------------
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertexCount_);

    VertexData* vbData = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&vbData);
    memcpy(vbData, vertices_.data(), sizeof(VertexData) * vertexCount_);
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * vertexCount_;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // ----------------
    // Transform CB
    // ----------------
    transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    transformResource_->Map(0, nullptr, (void**)&transformData_);

    // ----------------
    // Material CB
    // ----------------
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, (void**)&materialData_);

    materialData_->color = { 1, 1, 1, 1 };
    materialData_->enableLighting = false;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

  

    SetTexture("resources/Textures/uvChecker.png");
}

// ================================

// ================================
void SphereObject::Update(Camera* camera)
{
    camera_ = camera;

    Matrix4x4 world = MatrixMath::MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    Matrix4x4 vp = camera->GetViewProjectionMatrix();

    transformData_->World = world;
    transformData_->WVP = MatrixMath::Multiply(world, vp);
    Matrix4x4 inv = MatrixMath::Inverse(world);
    transformData_->WorldInverseTranspose = MatrixMath::Transpose(inv);
}

// ================================
// 謠冗判
// ================================
void SphereObject::Draw(ID3D12GraphicsCommandList* cmd)
{

    cmd->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());
    //  cmd->SetGraphicsRootConstantBufferView(3, lightResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(2, textureSrvHandle_);
    cmd->SetGraphicsRootConstantBufferView(4, camera_->GetGPUAddress());
    cmd->IASetVertexBuffers(0, 1, &vertexBufferView_);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(vertexCount_, 1, 0, 0);
}
void SphereObject::GenerateSphereVertices(VertexData* vertices, int kSubdivision, float radius)
{
    // 邨悟ｺｦ(360)
    const float kLonEvery = static_cast<float>(std::numbers::pi_v<float> * 2.0f) / kSubdivision;

    const float kLatEvery = static_cast<float>(std::numbers::pi_v<float>) / kSubdivision;

    for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {

        float lat = -static_cast<float>(std::numbers::pi_v<float>) / 2.0f + kLatEvery * latIndex;

        for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

            float lon = kLonEvery * lonIndex;


            Vector3 nA {
                cosf(lat) * cosf(lon),
                sinf(lat),
                cosf(lat) * sinf(lon)
            };

            Vector3 nB {
                cosf(lat + kLatEvery) * cosf(lon),
                sinf(lat + kLatEvery),
                cosf(lat + kLatEvery) * sinf(lon)
            };

            Vector3 nC {
                cosf(lat) * cosf(lon + kLonEvery),
                sinf(lat),
                cosf(lat) * sinf(lon + kLonEvery)
            };

            Vector3 nD {
                cosf(lat + kLatEvery) * cosf(lon + kLonEvery),
                sinf(lat + kLatEvery),
                cosf(lat + kLatEvery) * sinf(lon + kLonEvery)
            };


            VertexData vertA {
                radius * nA.x, radius * nA.y, radius * nA.z, 1.0f,
                { float(lonIndex) / kSubdivision,
                    1.0f - float(latIndex) / kSubdivision },
                nA
            };

            VertexData vertB {
                radius * nB.x, radius * nB.y, radius * nB.z, 1.0f,
                { float(lonIndex) / kSubdivision,
                    1.0f - float(latIndex + 1) / kSubdivision },
                nB
            };

            VertexData vertC {
                radius * nC.x, radius * nC.y, radius * nC.z, 1.0f,
                { float(lonIndex + 1) / kSubdivision,
                    1.0f - float(latIndex) / kSubdivision },
                nC
            };

            VertexData vertD {
                radius * nD.x, radius * nD.y, radius * nD.z, 1.0f,
                { float(lonIndex + 1) / kSubdivision,
                    1.0f - float(latIndex + 1) / kSubdivision },
                nD
            };


            uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

            vertices[startIndex + 0] = vertA;
            vertices[startIndex + 1] = vertB;
            vertices[startIndex + 2] = vertC;
            vertices[startIndex + 3] = vertC;
            vertices[startIndex + 4] = vertB;
            vertices[startIndex + 5] = vertD;
        }
    }
}

void SphereObject::SetTexture(const std::string& filePath)
{

    TextureManager::GetInstance()->LoadTexture(filePath);


    textureSrvHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU(filePath);
}
SphereObject::~SphereObject()
{
    if (transformResource_) {
        transformResource_->Unmap(0, nullptr);
    }

    if (materialResource_) {
        materialResource_->Unmap(0, nullptr);
    }
}