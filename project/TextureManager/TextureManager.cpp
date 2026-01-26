#include "TextureManager.h"
#include <format>

TextureManager* TextureManager::instance = nullptr;
// ImGuiで0番を使用するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

//=================================================================
// インスタンス取得（シングルトン）
//=================================================================
TextureManager* TextureManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new TextureManager();
    }
    return instance;
}

//=================================================================
// 終了処理（メモリ解放）
//=================================================================
void TextureManager::Finalize()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

//=================================================================
// 初期化処理
//=================================================================
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // SRVの数をあらかじめ確保
    textureDatas.reserve(SrvManager::kMaxSRVCount);
}

//=================================================================
// テクスチャの読み込み
//=================================================================
void TextureManager::LoadTexture(const std::string& filePath)
{
    // 空文字チェック（Animation_Skin 用）
    if (filePath.empty()) {
        return;
    }


    //読み込み済みテクスチャを検索
    if (textureDatas.contains(filePath)) {
        return; // すでに読み込まれているなら何もしない
    }

    // テクスチャ上限チェック
    assert(srvManager_->CanAllocate());

    // WIC経由で画像を読み込む
    DirectX::ScratchImage image {};
    std::wstring filePathW = StringUtility::ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    // ミップマップ生成
    DirectX::ScratchImage mipImages {};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

// テクスチャデータを追加して書き込む
    TextureData& textureData = textureDatas[filePath];

    // 情報を記録
    textureData.metadata = mipImages.GetMetadata();

    // テクスチャリソース生成
    textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // コマンド送信
    dxCommon_->GetCommandList()->Close();
    ID3D12CommandList* lists[] = { dxCommon_->GetCommandList() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);

    // SRVインデックス計算
    uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;
    Logger::Log(std::format("Texture Loaded: {}, SRV Index: {}", filePath, srvIndex));

    // CPU/GPUハンドル取得
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);

    dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

    // GPU待機＆再利用準備
    dxCommon_->WaitForGPU();
    dxCommon_->GetCommandAllocator()->Reset();
    dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

    Logger::Log(std::format("srvIndex = {}", srvIndex));
}

//=================================================================
// ファイルパスからテクスチャインデックスを取得
//=================================================================
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) {
    if (filePath.empty()) {
        return textureDatas.at("resources/white.png").srvIndex;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvIndex;
    }

    return textureDatas.at("resources/white.png").srvIndex;
}
 

//=================================================================
// GPUハンドル取得
//=================================================================
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {
    if (filePath.empty()) {
        return textureDatas.at("resources/white.png").srvHandleGPU;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvHandleGPU;
    }

    return textureDatas.at("resources/white.png").srvHandleGPU;
}
//=================================================================
// メタデータ取得
//=================================================================
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
    if (filePath.empty()) {
        return textureDatas.at("resources/white.png").metadata;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).metadata;
    }

    return textureDatas.at("resources/white.png").metadata;
}

