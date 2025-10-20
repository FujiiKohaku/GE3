#pragma once
#include "Camera.h"
#include "DirectXCommon.h"
class Object3dManager {
public:
    //=========================================
    // 初期化処理（DirectXCommonを受け取る）
    //=========================================
    void Initialize(DirectXCommon* dxCommon);

    //=========================================
    // 共通描画前処理
    // （IA設定・RootSig設定・PSO設定）
    //=========================================
    void PreDraw();

    //=========================================
    // getter
    //=========================================
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    Camera* GetDefaultCamera() const { return defaultCamera_; }
    //=========================================
    // setter
    //=========================================
    void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }

private:
    //=========================================
    // ルートシグネチャ作成
    //=========================================
    void CreateRootSignature();

    //=========================================
    // グラフィックスパイプライン作成
    //=========================================
    void CreateGraphicsPipeline();

private:
    // DirectX共通クラス（借りるだけ）
    DirectXCommon* dxCommon_ = nullptr;

    // RootSignatureとPSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

    // シリアライズ用Blob
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    Camera* defaultCamera_ = nullptr;
};
