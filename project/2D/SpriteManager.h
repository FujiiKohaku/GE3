#pragma once
#include "DirectXCommon.h"

// ===============================================
// SpriteManagerクラス
//  スプライト（2D描画）の共通処理を管理
// ===============================================
class SpriteManager {
public:
    // ===============================
    // 初期化処理
    // DirectXCommonを受け取り、
    // ルートシグネチャとPSOを作成する
    // ===============================
    void Initialize(DirectXCommon* dxCommon);

    // ===============================
    // 描画前準備
    // トポロジ・ルートシグネチャ・PSO設定
    // ===============================
    void PreDraw();

    // ===============================
    // Getter
    // ===============================
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    // ===============================
    // 内部関数（初期化補助）
    // ===============================

    // ルートシグネチャの作成
    void CreateRootSignature();

    // グラフィックスパイプラインの作成
    void CreateGraphicsPipeline();

private:
    // ===============================
    // メンバ変数
    // ===============================

    // DirectX共通クラス（借りて使うだけ）
    DirectXCommon* dxCommon_ = nullptr;

    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    // パイプラインステートオブジェクト（PSO）
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;

    // シリアライズ・エラー出力用
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
};
