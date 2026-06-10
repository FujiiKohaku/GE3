#pragma once

#include <string>
#include <vector>

class UIEditorTextureLoader {
public:
    static bool LoadPngRGBA(
        const std::wstring& filePath,
        std::vector<unsigned char>* pixels,
        int* width,
        int* height);

    static std::string WideToUtf8(const std::wstring& text);
    static std::wstring Utf8ToWide(const std::string& text);
};
