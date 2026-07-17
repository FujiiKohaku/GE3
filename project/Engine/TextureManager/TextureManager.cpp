#include "TextureManager.h"
#include <chrono>
#include <cstring>
#include <format>

std::unique_ptr<TextureManager> TextureManager::instance = nullptr;
// ImGui縺ｧ0逡ｪ繧剃ｽｿ逕ｨ縺吶ｋ縺溘ａ縲・逡ｪ縺九ｉ菴ｿ逕ｨ

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

    instance->FlushUploads();

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
    if (FAILED(hr)) {
        Logger::Error(std::format("Texture load failed: {}, HRESULT:{}", filePath, static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

    RegisterTexture(filePath, image);
}

void TextureManager::LoadTextureFromMemory(
    const std::string& textureKey,
    const uint8_t* data,
    size_t dataSize)
{
    if (textureKey.empty() || data == nullptr || dataSize == 0) {
        return;
    }
    if (textureDatas.contains(textureKey)) {
        return;
    }

    DirectX::ScratchImage image {};
    HRESULT hr = DirectX::LoadFromDDSMemory(data, dataSize, DirectX::DDS_FLAGS_NONE, nullptr, image);
    if (FAILED(hr)) {
        image.Release();
        hr = DirectX::LoadFromWICMemory(data, dataSize, DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    if (FAILED(hr)) {
        Logger::Error(std::format("Embedded texture load failed: {}, HRESULT:{}", textureKey, static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

    RegisterTexture(textureKey, image);
}

void TextureManager::LoadTextureFromBGRA(
    const std::string& textureKey,
    const uint8_t* data,
    size_t width,
    size_t height)
{
    if (textureKey.empty() || data == nullptr || width == 0 || height == 0) {
        return;
    }
    if (textureDatas.contains(textureKey)) {
        return;
    }

    DirectX::ScratchImage image {};
    HRESULT hr = image.Initialize2D(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, width, height, 1, 1);
    if (FAILED(hr)) {
        Logger::Error(std::format("Embedded texture initialization failed: {}, HRESULT:{}", textureKey, static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

    const DirectX::Image* destinationImage = image.GetImage(0, 0, 0);
    size_t sourceRowPitch = width * sizeof(uint32_t);
    for (size_t row = 0; row < height; ++row) {
        std::memcpy(
            destinationImage->pixels + destinationImage->rowPitch * row,
            data + sourceRowPitch * row,
            sourceRowPitch);
    }

    RegisterTexture(textureKey, image);
}

void TextureManager::RegisterTexture(const std::string& textureKey, DirectX::ScratchImage& image)
{
    assert(srvManager_ != nullptr);
    assert(srvManager_->CanAllocate());

    DirectX::ScratchImage mipImages {};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
        mipImages = std::move(image);
    } else {
        HRESULT hr = DirectX::GenerateMipMaps(
            image.GetImages(),
            image.GetImageCount(),
            image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB,
            0,
            mipImages);
        if (FAILED(hr)) {
            Logger::Error(std::format("Texture mipmap generation failed: {}, HRESULT:{}", textureKey, static_cast<unsigned long>(hr)));
            assert(false);
            return;
        }
    }

    TextureData& textureData = textureDatas[textureKey];
    textureData.metadata = mipImages.GetMetadata();
    textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource =
        dxCommon_->UploadTextureData(textureData.resource, mipImages);
    pendingUploadResources_.push_back(std::move(intermediateResource));

    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

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
    Logger::Log(std::format("Texture Loaded: {}, SRV Index: {}", textureKey, textureData.srvIndex));
}

void TextureManager::FlushUploads()
{
    if (pendingUploadResources_.empty()) {
        return;
    }

    if (dxCommon_ == nullptr) {
        Logger::Error("Texture upload flush failed: DirectXCommon is not initialized.");
        return;
    }

#ifdef _DEBUG
    const std::chrono::steady_clock::time_point beginTime =
        std::chrono::steady_clock::now();
#endif

    const size_t uploadCount = pendingUploadResources_.size();
    HRESULT hr = dxCommon_->GetCommandList()->Close();
    if (FAILED(hr)) {
        Logger::Error(
            std::format(
                "Texture upload flush failed while closing the command list. HRESULT:{}",
                static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

    ID3D12CommandList* commandLists[] = { dxCommon_->GetCommandList() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);
    dxCommon_->WaitForGPU();

    // GPU転送完了後にアップロード用リソースを解放する。
    pendingUploadResources_.clear();

    hr = dxCommon_->GetCommandAllocator()->Reset();
    if (FAILED(hr)) {
        Logger::Error(
            std::format(
                "Texture upload flush failed while resetting the command allocator. HRESULT:{}",
                static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

    hr = dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);
    if (FAILED(hr)) {
        Logger::Error(
            std::format(
                "Texture upload flush failed while resetting the command list. HRESULT:{}",
                static_cast<unsigned long>(hr)));
        assert(false);
        return;
    }

#ifdef _DEBUG
    const long long elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - beginTime)
            .count();
    Logger::Log(
        std::format(
            "[TextureUpload] Flushed {} textures in {} ms",
            uploadCount,
            elapsedMilliseconds));
#endif
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
