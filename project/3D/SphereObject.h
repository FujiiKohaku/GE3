#pragma once
#include "Camera.h"
#include "DirectXCommon.h"
#include "Object3DStruct.h"
#include <vector>
#include <wrl.h>

// ================================
// SphereObject
//  ・1球 = 1描画オブジェクト
//  ・Transform / Material / Light 対応
// ================================

class SphereObject {
   

public:
    // ----------------
    // 初期化
    // ----------------
    void Initialize(
        DirectXCommon* dxCommon,
        int subdivision,
        float radius);

    // ----------------
    // 更新
    // ----------------
    void Update(const Camera* camera);

    // ----------------
    // 描画
    // ----------------
    void Draw(ID3D12GraphicsCommandList* cmd);

    // ----------------
    // Transform 操作
    // ----------------
    void SetTranslate(const Vector3& t) { transform_.translate = t; }
    void SetRotate(const Vector3& r) { transform_.rotate = r; }
    void SetScale(const Vector3& s) { transform_.scale = s; }

    // ----------------
    // Material 操作
    // ----------------
    void SetColor(const Vector4& color)
    {
        if (materialData_) {
            materialData_->color = color;
        }
    }

void SetEnableLighting(bool enable)
    {
        if (materialData_) {
            if (enable) {
                materialData_->enableLighting = 1;
            } else {
                materialData_->enableLighting = 0;
            }
        }
    }


    // ----------------
    // Light 操作
    // ----------------
    void SetLightDirection(const Vector3& dir)
    {
        if (lightData_) {
            lightData_->direction = dir;
        }
    }

    void SetLightIntensity(float intensity)
    {
        if (lightData_) {
            lightData_->intensity = intensity;
        }
    }
    // テクスチャ操作
    void SetTexture(const std::string& filePath);

private:
    // ----------------
    // 内部処理
    // ----------------
    void GenerateSphereVertices(VertexData* vertices, int subdivision, float radius);

private:
    // 外部参照

    DirectXCommon* dxCommon_ = nullptr;

    // Transform

    Transform transform_ {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformationMatrix* transformData_ = nullptr;

    // Material

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    // Light

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    // Vertex

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    std::vector<VertexData> vertices_;
    uint32_t vertexCount_ = 0;

    // テクスチャSRVハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle_ {};
};
