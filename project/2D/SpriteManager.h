#pragma once
#include "DirectXCommon.h"
#include <memory>
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
    static void Finalize();


private:
    //----------------------------------------------
    // Singleton化関連
    //----------------------------------------------
    // デフォルトコンストラクタを削除
    static std::unique_ptr<SpriteManager> instance_;
   
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;

public:
    // コンストラクタを渡すための鍵
    // Passkey
    class ConstructorKey {
        ConstructorKey() = default;
        friend class SpriteManager;
    };
    explicit SpriteManager(ConstructorKey);
    ~SpriteManager() = default;

private:
    //----------------------------------------------
    // 内部処理（※元コードそのまま）
    //----------------------------------------------
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    // DirectX共通クラス（借りるだけ）
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;

    // Blob
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
};
