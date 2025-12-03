#pragma once
#include "DirectXCommon.h"

// ===============================================
// SpriteManager（Singleton版）
// ===============================================
class SpriteManager {
public:
    //==============================================
    // インスタンス取得
    //==============================================
    static SpriteManager* GetInstance();

    //==============================================
    // 初期化処理
    //==============================================
    void Initialize(DirectXCommon* dxCommon);

    //==============================================
    // 描画開始（RootSig/PSO設定）
    //==============================================
    void PreDraw();

    //==============================================
    // Getter
    //==============================================
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    // 終了処理
    void Finalize();

private:
    //----------------------------------------------
    // Singleton化関連
    //----------------------------------------------
    SpriteManager();
    ~SpriteManager();

public:

    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;

private:
    //----------------------------------------------
    // 内部処理（※元コードそのまま）
    //----------------------------------------------
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    // DirectX共通クラス（借りるだけ）
    DirectXCommon* dxCommon_ = nullptr;

    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    // PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

    // Blob
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
};
