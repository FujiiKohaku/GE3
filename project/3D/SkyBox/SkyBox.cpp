#include "SkyBox.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include <cassert>
#include "TextureManager.h"
void SkyBox::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    assert(dxCommon_);

    CreateBox();

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertexData_.size());
    assert(vertexResource_);

    VertexData* mappedVertexData = nullptr;
    HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData));
    assert(SUCCEEDED(hr));
    assert(mappedVertexData != nullptr);

    for (uint32_t index = 0; index < static_cast<uint32_t>(vertexData_.size()); ++index) {
        mappedVertexData[index] = vertexData_[index];
    }

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertexData_.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indexData_.size());
    assert(indexResource_);

    uint32_t* mappedIndexData = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndexData));
    assert(SUCCEEDED(hr));
    assert(mappedIndexData != nullptr);

    for (uint32_t index = 0; index < static_cast<uint32_t>(indexData_.size()); ++index) {
        mappedIndexData[index] = indexData_[index];
    }

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexData_.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    assert(transformResource_);

    hr = transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
    assert(SUCCEEDED(hr));
    assert(transformData_ != nullptr);
}

void SkyBox::Update(Camera* camera)
{
    assert(camera);
    camera_ = camera;

    Vector3 cameraPosition = camera_->GetTranslate();

    Matrix4x4 worldMatrix = MatrixMath::MakeTranslateMatrix(cameraPosition);
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    transformData_->World = worldMatrix;
    transformData_->WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);

    Matrix4x4 inverseWorldMatrix = MatrixMath::Inverse(worldMatrix);
    transformData_->WorldInverseTranspose = MatrixMath::Transpose(inverseWorldMatrix);
}

void SkyBox::Draw(ID3D12GraphicsCommandList* commandList)
{
    assert(commandList != nullptr);

    commandList->SetGraphicsRootConstantBufferView(0, transformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(textureHandle_));
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawIndexedInstanced(static_cast<UINT>(indexData_.size()), 1, 0, 0, 0);
}

void SkyBox::SetTexture(const std::string& textureFilePath)
{
    textureFilePath_ = textureFilePath;
    textureHandle_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

void SkyBox::CreateBox()
{
    vertexData_[0].position = { 1.0f, 1.0f, 1.0f, 1.0f };
    vertexData_[1].position = { 1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[2].position = { 1.0f, -1.0f, 1.0f, 1.0f };
    vertexData_[3].position = { 1.0f, -1.0f, -1.0f, 1.0f };

    vertexData_[4].position = { -1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[5].position = { -1.0f, 1.0f, 1.0f, 1.0f };
    vertexData_[6].position = { -1.0f, -1.0f, -1.0f, 1.0f };
    vertexData_[7].position = { -1.0f, -1.0f, 1.0f, 1.0f };

    vertexData_[8].position = { -1.0f, 1.0f, 1.0f, 1.0f };
    vertexData_[9].position = { 1.0f, 1.0f, 1.0f, 1.0f };
    vertexData_[10].position = { -1.0f, -1.0f, 1.0f, 1.0f };
    vertexData_[11].position = { 1.0f, -1.0f, 1.0f, 1.0f };

    vertexData_[12].position = { 1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[13].position = { -1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[14].position = { 1.0f, -1.0f, -1.0f, 1.0f };
    vertexData_[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };

    vertexData_[16].position = { -1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[17].position = { 1.0f, 1.0f, -1.0f, 1.0f };
    vertexData_[18].position = { -1.0f, 1.0f, 1.0f, 1.0f };
    vertexData_[19].position = { 1.0f, 1.0f, 1.0f, 1.0f };

    vertexData_[20].position = { -1.0f, -1.0f, 1.0f, 1.0f };
    vertexData_[21].position = { 1.0f, -1.0f, 1.0f, 1.0f };
    vertexData_[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
    vertexData_[23].position = { 1.0f, -1.0f, -1.0f, 1.0f };

    indexData_[0] = 0;
    indexData_[1] = 1;
    indexData_[2] = 2;
    indexData_[3] = 2;
    indexData_[4] = 1;
    indexData_[5] = 3;

    indexData_[6] = 4;
    indexData_[7] = 5;
    indexData_[8] = 6;
    indexData_[9] = 6;
    indexData_[10] = 5;
    indexData_[11] = 7;

    indexData_[12] = 8;
    indexData_[13] = 9;
    indexData_[14] = 10;
    indexData_[15] = 10;
    indexData_[16] = 9;
    indexData_[17] = 11;

    indexData_[18] = 12;
    indexData_[19] = 13;
    indexData_[20] = 14;
    indexData_[21] = 14;
    indexData_[22] = 13;
    indexData_[23] = 15;

    indexData_[24] = 16;
    indexData_[25] = 17;
    indexData_[26] = 18;
    indexData_[27] = 18;
    indexData_[28] = 17;
    indexData_[29] = 19;

    indexData_[30] = 20;
    indexData_[31] = 21;
    indexData_[32] = 22;
    indexData_[33] = 22;
    indexData_[34] = 21;
    indexData_[35] = 23;
}