#pragma once
#include "ImGuiManager.h"
#include "MathStruct.h"
#include "MatrixMath.h"
#include "WinApp.h"


class Camera {
public:
    // デフォルトコンストラクタ宣言
    Camera();

    void DebugUpdate();
    void Initialize();
    // 更新
    void Update();
    // ===============================
    // setter（外部から値を設定）
    // ===============================
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetFovY(float fovY) { fovY_ = fovY; }
    void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
    void SetNearClip(float nearClip) { nearClip_ = nearClip; }
    void SetFarClip(float farClip) { farClip_ = farClip; }

    // ===============================
    // getter（外部から値を取得）
    // ===============================

    // 各行列
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    // ViewProjection（合成済み行列）
    const Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
    // 各種Transform情報
    const Vector3& GetRotate() const { return transform_.rotate; }
    const Vector3& GetTranslate() const { return transform_.translate; }
    Vector3& GetTranslate() { return transform_.translate; }
    Vector3& GetRotate() { return transform_.rotate; }
    // 各種プロジェクション設定値
    float GetFovY() const { return fovY_; }
    float GetAspectRatio() const { return aspectRatio_; }
    float GetNearClip() const { return nearClip_; }
    float GetFarClip() const { return farClip_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const
    {
        return cameraResource_->GetGPUVirtualAddress();
    }

private:
    struct CameraForGPU {
        Vector3 worldPosition;
    };

    EulerTransform transform_;
    Matrix4x4 worldMatrix_;
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;
    // プロジェクション計算用パラメータ
    float fovY_ = 0.45f; // 垂直方向の視野角
    float aspectRatio_ = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
    float nearClip_ = 0.1f; // ニアクリップ距離
    float farClip_ = 100.0f; // ファークリップ距離

    // GPU用
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;


};
