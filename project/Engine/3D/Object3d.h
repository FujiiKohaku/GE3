#pragma once
#include "Engine/Camera/Camera.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/math/MatrixMath.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
#include "Engine/LevelEditor/LevelData.h"

#include "../Animation/PlayAnimation.h"
#include "Engine/math/object3Dstruct.h"
class Object3dManager;
class Model;
class BoxCollider;
class Object3d {
public:
    // ===============================
    // メンバ関数
    // ===============================
    void Initialize(Object3dManager* object3DManager);
    void Update();
    void Draw();
    ~Object3d();
    static ModelData LoadModeFile(const std::string& directoryPath, const std::string filename);
    // static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    // setter
    void SetModel(Model* model) { model_ = model; }
    // === setter ===
    void SetScale(const Vector3& scale) { transform.scale = scale; useCustomWorldMatrix_ = false; }
    void SetRotate(const Vector3& rotate) { transform.rotate = rotate; useQuaternionRotation_ = false; useCustomWorldMatrix_ = false; }
    void SetRotateQuaternion(const Quaternion& rotate) { quaternionRotation_ = rotate; useQuaternionRotation_ = true; useCustomWorldMatrix_ = false; }
    void SetTranslate(const Vector3& translate) { transform.translate = translate; useCustomWorldMatrix_ = false; }
    void SetCustomWorldMatrix(const Matrix4x4& matrix) { customWorldMatrix_ = matrix; useCustomWorldMatrix_ = true; }
    void SetModel(const std::string& filePath);
    void SetCamera(Camera* camera) { camera_ = camera; }

    // === getter ===
    const Vector3& GetScale() const { return transform.scale; }
    const Vector3& GetRotate() const { return transform.rotate; }
    const Vector3& GetTranslate() const { return transform.translate; }
    // DirectionalLight* GetLight() { return directionalLightData; }
    Material* GetMaterial() { return materialData_; }
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

    void SetAnimation(PlayAnimation* anim);
    const Node& GetRootNode() const;
    const Matrix4x4& GetWorldMatrix() const
    {
        return worldMatrix_;
    }
    void SetEnvironmentTextureFilePath(const std::string& filePath)
    {
        environmentTextureFilePath_ = filePath;
        TextureManager::GetInstance()->LoadTexture(filePath);
    }
    void SetEnableEnvironmentMap(bool enable)
    {
        if (materialData_) {
            materialData_->enableEnvironmentMap = enable;
        }
    }

    void SetEnvironmentMapStrength(float strength)
    {
        if (materialData_) {
            materialData_->environmentCoefficient = strength;
        }
    }
    void SetName(const std::string& name)
    {
        name_ = name;
    }

    const std::string& GetName() const
    {
        return name_;
    }

    const std::string& GetModelFilePath() const
    {
        return modelFilePath_;
    }

    void SetGimmick(const LevelData::ObjectData::GimmickData& gimmick)
    {
        gimmick_ = gimmick;
        baseTranslate_ = transform.translate;
        gimmickTime_ = 0.0f;
    }
    const LevelData::ObjectData::GimmickData& GetGimmick() const { return gimmick_; }

    void SetCollider(BoxCollider* collider) { collider_ = collider; }
    BoxCollider* GetCollider() const { return collider_; }

private:
    // ===============================
    // メンバ変数
    // ===============================
    Object3dManager* object3dManager_ = nullptr;
    static Node ReadNode(aiNode* node);
    Model* model_ = nullptr;
    // バッファ系
    /*  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;*/
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;

    /*   D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};*/
    /*   Material* materialData = nullptr;*/
    TransformationMatrix* transformationMatrixData = nullptr;
    //  DirectionalLight* directionalLightData = nullptr;
    Material* materialData_ = nullptr;
    // Transform
    EulerTransform transform;
    EulerTransform cameraTransform;

    Quaternion quaternionRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool useQuaternionRotation_ = false;

    Matrix4x4 customWorldMatrix_ = {};
    bool useCustomWorldMatrix_ = false;

    // カメラ
    Camera* camera_ = nullptr;
    // モデル
    // ModelData modelData;
    PlayAnimation* animation_ = nullptr;

    // World
    Matrix4x4 worldMatrix_;
    //  D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureHandle_ {};
    std::string environmentTextureFilePath_;
    std::string name_ = "Object[nameNull]";
    std::string modelFilePath_;

    LevelData::ObjectData::GimmickData gimmick_ {};
    Vector3 baseTranslate_ = { 0.0f, 0.0f, 0.0f };
    float gimmickTime_ = 0.0f;
    BoxCollider* collider_ = nullptr;
};
