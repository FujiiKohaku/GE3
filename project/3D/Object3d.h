#pragma once
#include "Camera.h"
#include "DebugCamera.h"
#include "Matrix4x4.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
class Object3dManager;
class Model;
class Object3d {
public:
    // ===============================
    // 構造体
    // ===============================
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = 0;
    };

    // 頂点、マテリアル関連
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };
    // 変換情報
    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    // ===============================
    // メンバ関数
    // ===============================
    void Initialize(Object3dManager* object3DManager);
    void Update();
    void Draw();

    static ModelData LoadObjFile(const std::string& directoryPath, const std::string filename);
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    // setter
    void SetModel(Model* model) { model_ = model; }
    // === setter ===
    void SetScale(const Vector3& scale) { transform.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform.translate = translate; }
    void SetModel(const std::string& filePath);
    void SetCamera(Camera* camera) { camera_ = camera; }
    // === getter ===
    const Vector3& GetScale() const { return transform.scale; }
    const Vector3& GetRotate() const { return transform.rotate; }
    const Vector3& GetTranslate() const { return transform.translate; }
    DirectionalLight* GetLight() { return directionalLightData; }
    Object3d::Material* GetMaterial() { return materialData_; }

private:
    // ===============================
    // メンバ変数
    // ===============================
    Object3dManager* object3dManager_ = nullptr;

    Model* model_ = nullptr;
    // バッファ系
    /*  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;*/
     Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;

    /*   D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};*/
    /*   Material* materialData = nullptr;*/
    TransformationMatrix* transformationMatrixData = nullptr;
    DirectionalLight* directionalLightData = nullptr;
    Material* materialData_ = nullptr;
    // Transform
    Transform transform;
    Transform cameraTransform;

    // カメラ
    Camera* camera_ = nullptr;
    // モデル
    // ModelData modelData;
};
