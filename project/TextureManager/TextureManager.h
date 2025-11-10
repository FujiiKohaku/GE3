#pragma once
#include "DirectXCommon.h"
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"
#include "SrvManager.h"
#include <algorithm> // std::find_if
#include <cassert> // assert
#include <string> // std::string
#include <unordered_map>
#include <vector> // std::vector
#include <wrl.h> // ComPtr
//==================================================================
//  TextureManagerクラス
//  テクスチャの読み込み・管理を行うシングルトンクラス
//==================================================================
class TextureManager {
public:
    //==================================================================
    //  シングルトン関連
    //==================================================================
    // インスタンス取得（最初の呼び出し時に自動生成）
    static TextureManager* GetInstance();
    // 終了処理（メモリ解放）
    void Finalize();

    //==================================================================
    //  初期化・読み込み
    //==================================================================
    // 初期化（DirectXCommonを受け取る）
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    // テクスチャ読み込み（同名ファイルは二重読み込みしない）
    void LoadTexture(const std::string& filePath);

    //==================================================================
    //  情報取得系
    //==================================================================
    // ファイルパスからSRVインデックスを取得
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);

    // ファイルパスからGPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

     // メタデータを取得
    const DirectX::TexMetadata& GetMetaData(const std::string& filepath);

private:
    //==================================================================
    //  シングルトン制御
    //==================================================================
    static TextureManager* instance;
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    //==================================================================
    //  内部構造体
    //==================================================================
    // テクスチャ1枚分のデータ
    struct TextureData {
        DirectX::TexMetadata metadata; // メタデータ（幅・高さなど）
        Microsoft::WRL::ComPtr<ID3D12Resource> resource; // テクスチャリソース
        uint32_t srvIndex; // SRVインデックス
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU {}; // CPUハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU {}; // GPUハンドル
    };

    //==================================================================
    //  メンバ変数
    //==================================================================
    // テクスチャデータ
    std::unordered_map<std::string, TextureData> textureDatas;

    DirectXCommon* dxCommon_ = nullptr; // DX共通クラスの参照

    // SRV管理
    static uint32_t kSRVIndexTop; // SRVインデックスの開始番号（0番はImGui用）
    static const uint32_t kMaxSRVCount = 512; // 最大テクスチャ数（任意に設定可能）

    SrvManager* srvManager_ = nullptr; // SRVマネージャーの参照


};
