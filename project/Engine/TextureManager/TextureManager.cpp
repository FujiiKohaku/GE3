#include "TextureManager.h"
#include <format>

std::unique_ptr<TextureManager> TextureManager::instance = nullptr;
// ImGui縺ｧ0逡ｪ繧剃ｽｿ逕ｨ縺吶ｋ縺溘ａ縲・逡ｪ縺九ｉ菴ｿ逕ｨ
uint32_t TextureManager::kSRVIndexTop = 1;

//=================================================================
// 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ蜿門ｾ暦ｼ医す繝ｳ繧ｰ繝ｫ繝医Φ・・
//=================================================================
TextureManager* TextureManager::GetInstance()
{
    if (!instance) {
        instance = std::make_unique<TextureManager>(ConstructorKey());
    }
    return instance.get();
}

//=================================================================
// 邨ゆｺ・・逅・ｼ医Γ繝｢繝ｪ隗｣謾ｾ・・
//=================================================================
void TextureManager::Finalize()
{
    if (!instance) {
        return;
    }

    textureDatas.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;

    instance.reset();
}

//=================================================================
// 蛻晄悄蛹門・逅・
//=================================================================
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // SRV縺ｮ謨ｰ繧偵≠繧峨°縺倥ａ遒ｺ菫・
    textureDatas.reserve(SrvManager::kMaxSRVCount);
}

//=================================================================
// 繝・け繧ｹ繝√Ε縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
//=================================================================
void TextureManager::LoadTexture(const std::string& filePath)
{
    // 遨ｺ譁・ｭ励メ繧ｧ繝・け・・nimation_Skin 逕ｨ・・
    if (filePath.empty()) {
        return;
    }

    // 隱ｭ縺ｿ霎ｼ縺ｿ貂医∩繝・け繧ｹ繝√Ε繧呈､懃ｴ｢
    if (textureDatas.contains(filePath)) {
        return; // 縺吶〒縺ｫ隱ｭ縺ｿ霎ｼ縺ｾ繧後※縺・ｋ縺ｪ繧我ｽ輔ｂ縺励↑縺・
    }

    // 繝・け繧ｹ繝√Ε荳企剞繝√ぉ繝・け
    assert(srvManager_->CanAllocate());

    // WIC邨檎罰縺ｧ逕ｻ蜒上ｒ隱ｭ縺ｿ霎ｼ繧
    DirectX::ScratchImage image {};
    std::wstring filePathW = StringUtility::ConvertString(filePath);

    HRESULT hr;

    if (filePathW.ends_with(L".dds")) {
        hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    assert(SUCCEEDED(hr));

    // 繝溘ャ繝励・繝・・逕滓・
    DirectX::ScratchImage mipImages {};

    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        mipImages = std::move(image); // 蝨ｧ邵ｮ繝・け繧ｹ繝√Ε縺ｯ繝溘ャ繝励・繝・・縺後☆縺ｧ縺ｫ逕滓・縺輔ｌ縺ｦ縺・ｋ縺薙→縺悟､壹＞縺ｮ縺ｧ縲√◎縺ｮ縺ｾ縺ｾ菴ｿ逕ｨ
    } else {
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    }

    assert(SUCCEEDED(hr));

    // 繝・け繧ｹ繝√Ε繝・・繧ｿ繧定ｿｽ蜉縺励※譖ｸ縺崎ｾｼ繧
    TextureData& textureData = textureDatas[filePath];

    // 諠・ｱ繧定ｨ倬鹸
    textureData.metadata = mipImages.GetMetadata();

    // 繝・け繧ｹ繝√Ε繝ｪ繧ｽ繝ｼ繧ｹ逕滓・
    textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // 繧ｳ繝槭Φ繝蛾∽ｿ｡
    dxCommon_->GetCommandList()->Close();
    ID3D12CommandList* lists[] = { dxCommon_->GetCommandList() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);

    // SRV繧､繝ｳ繝・ャ繧ｯ繧ｹ險育ｮ・
    uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;
    Logger::Log(std::format("Texture Loaded: {}, SRV Index: {}", filePath, srvIndex));

    // CPU/GPU繝上Φ繝峨Ν蜿門ｾ・
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    // SRV險ｭ螳・
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (textureData.metadata.IsCubemap()) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = UINT_MAX;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    }


    dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

    // GPU蠕・ｩ滂ｼ・・蛻ｩ逕ｨ貅門ｙ
    dxCommon_->WaitForGPU();
    dxCommon_->GetCommandAllocator()->Reset();
    dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

    Logger::Log(std::format("srvIndex = {}", srvIndex));
}

//=================================================================
// 繝輔ぃ繧､繝ｫ繝代せ縺九ｉ繝・け繧ｹ繝√Ε繧､繝ｳ繝・ャ繧ｯ繧ｹ繧貞叙蠕・
//=================================================================
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    if (filePath.empty()) {
        return textureDatas.at("resources/Textures/white.png").srvIndex;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvIndex;
    }

    return textureDatas.at("resources/Textures/white.png").srvIndex;
}

//=================================================================
// GPU繝上Φ繝峨Ν蜿門ｾ・
//=================================================================
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
    if (filePath.empty()) {
        return textureDatas.at("resources/Textures/white.png").srvHandleGPU;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvHandleGPU;
    }

    return textureDatas.at("resources/Textures/white.png").srvHandleGPU;
}
//=================================================================
// 繝｡繧ｿ繝・・繧ｿ蜿門ｾ・
//=================================================================
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
    if (filePath.empty()) {
        return textureDatas.at("resources/Textures/white.png").metadata;
    }

    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).metadata;
    }

    return textureDatas.at("resources/Textures/white.png").metadata;
}
TextureManager::TextureManager(ConstructorKey)
{
}
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureHandle) const
{
    return srvManager_->GetGPUDescriptorHandle(textureHandle);
}