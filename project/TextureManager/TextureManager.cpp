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
void TextureManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    // SRVの数をあらかじめ確保
    textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

//=================================================================
// テクスチャの読み込み
//=================================================================
void TextureManager::LoadTexture(const std::string& filePath)
{
    // すでに読み込み済みならスキップ
    auto it = std::find_if(
        textureDatas.begin(),
        textureDatas.end(),
        [&](TextureData& textureData) { return textureData.filePath == filePath; });
    if (it != textureDatas.end()) {
        return;
    }

    // テクスチャ上限チェック
    assert(textureDatas.size() + kSRVIndexTop < DirectXCommon::kMaxSRVCount);

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

    // 新しいテクスチャデータを追加
    textureDatas.resize(textureDatas.size() + 1);
    TextureData& textureData = textureDatas.back();

    // 情報を記録
    textureData.filePath = filePath;
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
    textureData.srvHandleCPU = dxCommon_->GetCPUDescriptorHandle(
        dxCommon_->GetSRVDescriptorHeap(), dxCommon_->GetSRVDescriptorSize(), srvIndex);
    textureData.srvHandleGPU = dxCommon_->GetGPUDescriptorHandle(
        dxCommon_->GetSRVDescriptorHeap(), dxCommon_->GetSRVDescriptorSize(), srvIndex);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);

    dxCommon_->GetDevice()->CreateShaderResourceView(
        textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

    // GPU待機＆再利用準備
    dxCommon_->WaitForGPU();
    dxCommon_->GetCommandAllocator()->Reset();
    dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

    Logger::Log(std::format("srvIndex = {}", srvIndex));
}

//=================================================================
// ファイルパスからテクスチャインデックスを取得
//=================================================================
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    auto it = std::find_if(
        textureDatas.begin(),
        textureDatas.end(),
        [&](TextureData& textureData) { return textureData.filePath == filePath; });

    if (it != textureDatas.end()) {
        uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas.begin(), it)) + kSRVIndexTop;
        return textureIndex;
    }

    assert(0); // 未ロードのファイル
    return 0;
}

//=================================================================
// GPUハンドル取得
//=================================================================
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 範囲外アクセス防止（kSRVIndexTop考慮）
    assert(textureIndex >= kSRVIndexTop);
    assert(textureIndex - kSRVIndexTop < textureDatas.size());

    // 実際のテクスチャデータを取得
    TextureData& textureData = textureDatas[textureIndex - kSRVIndexTop];

    return textureData.srvHandleGPU;
}

//=================================================================
// メタデータ取得
//=================================================================
const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t textureIndex)
{
    // 範囲外アクセス防止
    assert(textureIndex >= kSRVIndexTop);
    assert(textureIndex - kSRVIndexTop < textureDatas.size());

    // 実際のテクスチャデータを参照
    TextureData& textureData = textureDatas[textureIndex - kSRVIndexTop];
    return textureData.metadata;
}

