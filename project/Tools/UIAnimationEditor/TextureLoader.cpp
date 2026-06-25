#include "TextureLoader.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl.h>

#pragma comment(lib, "windowscodecs.lib")

bool UIEditorTextureLoader::LoadPngRGBA(const std::wstring& filePath,std::vector<unsigned char>* pixels,int* width,int* height)
{
    if (pixels == nullptr) {
        return false;
    }
    if (width == nullptr) {
        return false;
    }
    if (height == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));

    if (FAILED(result)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(filePath.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);

    if (FAILED(result)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frameDecode;
    result = decoder->GetFrame(0, &frameDecode);

    if (FAILED(result)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(&converter);

    if (FAILED(result)) {
        return false;
    }

    result = converter->Initialize(frameDecode.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0.0f,WICBitmapPaletteTypeCustom);

    if (FAILED(result)) {
        return false;
    }

    UINT imageWidth = 0;
    UINT imageHeight = 0;
    result = converter->GetSize(&imageWidth, &imageHeight);

    if (FAILED(result)) {
        return false;
    }

    UINT stride = imageWidth * 4;
    UINT bufferSize = stride * imageHeight;
    pixels->resize(bufferSize);

    result = converter->CopyPixels(nullptr, stride, bufferSize, pixels->data());

    if (FAILED(result)) {
        pixels->clear();
        return false;
    }

    *width = static_cast<int>(imageWidth);
    *height = static_cast<int>(imageHeight);

    return true;
}

std::string UIEditorTextureLoader::WideToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return "";
    }

    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);

    if (requiredSize <= 0) {
        return "";
    }

    std::string result;
    result.resize(static_cast<std::size_t>(requiredSize - 1));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], requiredSize, nullptr, nullptr);

    return result;
}

std::wstring UIEditorTextureLoader::Utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return L"";
    }

    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);

    if (requiredSize <= 0) {
        return L"";
    }

    std::wstring result;
    result.resize(static_cast<std::size_t>(requiredSize - 1));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], requiredSize);

    return result;
}
