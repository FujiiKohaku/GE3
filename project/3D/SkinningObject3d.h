#pragma once
#include "../3D/SkinCluster.h"
#include "../Animation/PlayAnimation.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "MatrixMath.h"
#include "Model.h"
#include "Object3DStruct.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
class SkinningObject3dManager;
class Model;
class SkinningObject3d {
public:
    // ===============================
    // メンバ関数
    // ===============================
    void Initialize(SkinningObject3dManager* skinningObject3DManager);
    void Update();
    void Draw();

    // setter
    void SetModel(Model* model) { model_ = model; }

    void SetCamera(Camera* camera) { camera_ = camera; }
    // === setter ===
    void SetScale(const Vector3& scale)
    {
        baseTransform_.scale = scale;
    }

    void SetRotate(const Vector3& rotate)
    {
        baseTransform_.rotate = rotate;
    }

    void SetTranslate(const Vector3& translate)
    {
        baseTransform_.translate = translate;
    }

    // === getter ===
    const Vector3& GetScale() const
    {
        return baseTransform_.scale;
    }

    const Vector3& GetRotate() const
    {
        return baseTransform_.rotate;
    }

    const Vector3& GetTranslate() const
    {
        return baseTransform_.translate;
    }

    // DirectionalLight* GetLight() { return directionalLightData; }
    Material* GetMaterial() { return materialData_; }
    SkinCluster::SkinClusterData& GetSkinCluster()
    {
        return skinClusterData_;
    }
    void SetAnimation(PlayAnimation* animation)
    {
        playAnimation_ = animation;
    }

    PlayAnimation* GetAnimation() const
    {
        return playAnimation_;
    }
    const Node& GetRootNode() const
    {
        assert(model_);
        return model_->GetModelData().rootNode;
    }
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

    const Matrix4x4& GetWorldMatrix() const
    {
        return worldMatrix_;
    }

private:
    // ===============================
    // メンバ変数
    // ===============================
    SkinningObject3dManager* skinningObject3dManager_ = nullptr;

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
    EulerTransform cameraTransform;
    EulerTransform baseTransform_;
    EulerTransform animTransform_;
    // カメラ
    Camera* camera_ = nullptr;
    // モデル
    // ModelData modelData;
    PlayAnimation* animation_ = nullptr;

    // World
    Matrix4x4 worldMatrix_;

    // SkinCluster skinCluster_;
    SkinCluster::SkinClusterData skinClusterData_;
    PlayAnimation* playAnimation_;
};
