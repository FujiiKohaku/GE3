#pragma once
#include "Engine/math/ValueValidation.h"
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
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/Shadow/ShadowMaterialSettings.h"
#include "Engine/MotionVector/MotionVectorRenderer.h"

#include "../Animation/PlayAnimation.h"
#include "Engine/math/object3Dstruct.h"
class Object3dManager;
class Model;
class BoxCollider;
class ShadowMapRenderer;
class DxrRenderer;
class Object3d {
public:
    // ===============================
    // メンバ関数
    // ===============================
    void Initialize(Object3dManager* object3DManager);
    void Update();
    void Draw();
    void SubmitRaytracing(DxrRenderer& renderer) const;
    bool IsVisible(const Camera& camera) const;
    void SetFrustumCullingEnabled(bool isEnabled) { shouldUseFrustumCulling_ = isEnabled; }
    void SetRaytracingEnabled(bool isEnabled) { isRaytracingEnabled_ = isEnabled; }
    void QueueMotionVectors();
    void ResetMotionHistory() { motionHistory_.Reset(); }
    void SetMotionVectorShader(const std::wstring& shaderPath) { motionVectorShaderPath_ = shaderPath; ResetMotionHistory(); }
    void SetNormalMap(const std::string& filePath, float strength = 0.3f, bool flipY = false);
    void SetNormalMapStrength(float strength);
    void SetMetallicRoughnessMap(const std::string& filePath);
    bool SetAlphaCutoff(float alphaCutoff);
    void DrawShadow(ShadowMapRenderer& renderer, bool opaqueTransparentShadow = false);
    bool SetShadowMaterial(const ShadowMaterialSettings& settings);
    void SetShadowBoundsPadding(float boundsPadding);
    float GetModelBoundsRadius() const;
    void SetCastShadow(bool enabled) { castShadow_ = enabled; }
    void SetReceiveShadow(bool enabled) { receiveShadow_ = enabled; }
    bool GetCastShadow() const { return castShadow_; }
    bool GetReceiveShadow() const { return receiveShadow_; }
    ~Object3d();
    static ModelData LoadModeFile(const std::string& directoryPath, const std::string filename);
    // static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    // setter
    void SetModel(Model* model) { model_ = model; ResetMotionHistory(); }
    void SetSurfaceProperties(float roughness, float metallic, float specularStrength);
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
    bool SetRoughness(float value) { if (!materialData_ || !std::isfinite(value) || value < 0 || value > 1.0f) return false; materialData_->roughness = value; return true; }
    bool SetMetallic(float value) { if (!materialData_ || !std::isfinite(value) || value < 0 || value > 1.0f) return false; materialData_->metallic = value; return true; }
    bool SetSpecularStrength(float value) { if (!materialData_ || !std::isfinite(value) || value < 0 || value > 2.0f) return false; materialData_->specularStrength = value; return true; }
    bool SetOpacity(float value) { if (!materialData_ || !std::isfinite(value) || value < 0 || value > 1) return false; materialData_->color.w = value; return true; }
    bool SetUvTransformElement(uint32_t row, uint32_t column, float value) { if (!materialData_ || row >= 4 || column >= 4 || !std::isfinite(value)) return false; materialData_->uvTransform.m[row][column] = value; return true; }
    const Material* GetMaterial() const { return materialData_; }
    bool SetShininess(float value) { if (!materialData_ || !std::isfinite(value) || value < 0.0f) return false; materialData_->shininess = value; return true; }
    bool SetUvTransform(const Matrix4x4& value) { if (!materialData_ || !ValueValidation::IsFinite(value)) return false; materialData_->uvTransform = value; return true; }
    // ----------------
    // Material 操作
    // ----------------
    void SetColor(const Vector4& color)
    {
        if (materialData_) {
            materialData_->color = color;
        }
    }

    void SetEnableLighting(bool enable);
    void SetShadingMode(MaterialShadingMode mode);
    void SetMaterial(const std::string& materialFolderPath);
    void SetPixelShaderPath(const std::string& pixelShaderPath)
    {
        pixelShaderPath_ = pixelShaderPath;
    }
    const std::string& GetPixelShaderPath() const { return pixelShaderPath_; }

    void SetVertexShaderPath(const std::string& vertexShaderPath)
    {
        vertexShaderPath_ = vertexShaderPath;
    }
    const std::string& GetVertexShaderPath() const { return vertexShaderPath_; }
    // Four floats at VS b1. Their meaning is defined by the selected shader.
    void SetVertexShaderParameters(const Vector4& parameters)
    {
        vertexShaderParameters_ = parameters;
    }

    // Transparent objects use alpha blending without writing depth by default.
    // The caller must draw them after opaque objects, from back to front.
    void SetTransparent(bool transparent) { transparent_ = transparent; }
    bool IsTransparent() const { return transparent_; }
    // Opaque Object3d surfaces, plus non-depth-writing DanmakuBullet / Unlit surfaces.
    void SetFogEnabled(bool enabled) { fogEnabled_ = enabled; fogWarningReported_ = false; }
    bool IsFogEnabled() const { return fogEnabled_; }
    // Depth writing is an approximation for nearly opaque, sorted surfaces.
    void SetTransparentDepthWrite(bool enable) { transparentDepthWrite_ = enable; }

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
            if (std::isfinite(strength) && strength >= 0.0f) { materialData_->environmentCoefficient = strength; }
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

    void SetCollisionDamage(int damage) { collisionDamage_ = damage > 0 ? damage : 1; }
    int GetCollisionDamage() const { return collisionDamage_; }

    void SetCollider(BoxCollider* collider)
    {
        collider_ = collider;
        if (collider_ != nullptr) {
            colliderOffset_ = collider_->GetCenter() - transform.translate;
            collider_->SetRotation(transform.rotate);
        }
    }
    BoxCollider* GetCollider() const { return collider_; }
    size_t GetColliderCount() const
    {
        if (collider_ == nullptr) { return 0; }
        return 1 + additionalColliders_.size();
    }
    BoxCollider* GetCollider(size_t index) const
    {
        if (index == 0) { return collider_; }
        return additionalColliders_.at(index - 1).collider;
    }
    void AddCollider(BoxCollider* collider)
    {
        if (collider == nullptr) { return; }
        if (collider_ == nullptr) { SetCollider(collider); return; }
        additionalColliders_.push_back({ collider, collider->GetCenter() - transform.translate });
        collider->SetRotation(transform.rotate);
    }
    void ClearColliders() { collider_ = nullptr; additionalColliders_.clear(); }

private:
    struct ColliderAttachment {
        BoxCollider* collider;
        Vector3 offset;
    };
    std::vector<ColliderAttachment> additionalColliders_;
    friend class Object3dManager;
    bool IsRaytracingEligible() const;
    bool shouldUseFrustumCulling_ = false;
    bool isRaytracingEnabled_ = true;
    MotionVectorHistory motionHistory_;
    std::wstring motionVectorShaderPath_;
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
    EulerTransform transform {};
    EulerTransform cameraTransform {};

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
    Matrix4x4 worldMatrix_ {};
    //  D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureHandle_ {};
    std::string environmentTextureFilePath_;
    std::string name_ = "Object[nameNull]";
    std::string modelFilePath_;
    std::string pixelShaderPath_ =
        "resources/Shaders/Object3D/Unlit/Render.PS.hlsl";
    std::string vertexShaderPath_ = "resources/Shaders/Object3D/Object3d.VS.hlsl";
    Vector4 vertexShaderParameters_ = {};
    ShadowMaterialSettings shadowMaterial_;
    bool castShadow_ = false;
    bool receiveShadow_ = false;
    bool transparent_ = false;
    bool transparentDepthWrite_ = false;
    bool fogEnabled_ = true;
    bool fogWarningReported_ = false;

    LevelData::ObjectData::GimmickData gimmick_ {};
    Vector3 baseTranslate_ = { 0.0f, 0.0f, 0.0f };
    float gimmickTime_ = 0.0f;
    int collisionDamage_ = 1;
    BoxCollider* collider_ = nullptr;
    Vector3 colliderOffset_ = { 0.0f, 0.0f, 0.0f };
    std::string normalMapTextureKey_;
    std::string metallicRoughnessTextureKey_;
};
