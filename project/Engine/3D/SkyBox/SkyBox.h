#pragma once
#include "Engine/Math/MathStruct.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

class DirectXCommon;
class Camera;

class SkyBox {
public:
    struct VertexData {
        std::array<float, 4> position;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
    };

public:
    void Initialize(DirectXCommon* dxCommon);
    void Update(Camera* camera);
    void Draw(ID3D12GraphicsCommandList* commandList);

    void CreateBox();
    void SetTexture(const std::string& textureFilePath);

private:
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;

    std::array<VertexData, 24> vertexData_ {};
    std::array<uint32_t, 36> indexData_ {};

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};

    TransformationMatrix* transformData_ = nullptr;

    std::string textureFilePath_;
    uint32_t textureHandle_ = 0;
};