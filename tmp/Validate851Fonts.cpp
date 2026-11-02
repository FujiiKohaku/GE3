#define STB_TRUETYPE_IMPLEMENTATION
#include "externals/imgui/imstb_truetype.h"
#include "Engine/2D/Text/Utf8Utility.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 3) { return 2; }
    std::ifstream fontFile(arguments[1], std::ios::binary);
    std::vector<unsigned char> fontBytes{ std::istreambuf_iterator<char>(fontFile), {} };
    if (fontBytes.empty()) { return 2; }
    stbtt_fontinfo fontInfo{};
    const int fontOffset = stbtt_GetFontOffsetForIndex(fontBytes.data(), 0);
    if (fontOffset < 0 || !stbtt_InitFont(&fontInfo, fontBytes.data(), fontOffset)) { return 2; }
    std::ifstream textFile(arguments[2], std::ios::binary);
    std::string text{ std::istreambuf_iterator<char>(textFile), {} };
    std::set<uint32_t> codepoints;
    for (uint32_t codepoint : Utf8Utility::Decode(text)) {
        if (codepoint >= 32 && codepoint != 0xFEFF) { codepoints.insert(codepoint); }
    }
    const float scale = stbtt_ScaleForPixelHeight(&fontInfo, 48.0f);
    bool hasFailed = false;
    int maxWidthPixels = 0;
    int maxHeightPixels = 0;
    for (uint32_t codepoint : codepoints) {
        const int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, static_cast<int>(codepoint));
        if (codepoint != 32 && codepoint != 0x3000 && stbtt_IsGlyphEmpty(&fontInfo, glyphIndex)) {
            std::cout << "Empty U+" << std::hex << codepoint << std::dec << '\n';
            hasFailed = true;
        }
        if (stbtt_FindGlyphIndex(&fontInfo, static_cast<int>(codepoint)) == 0) {
            std::cout << "Missing U+" << std::hex << codepoint << std::dec << '\n';
            hasFailed = true;
            continue;
        }
        int widthPixels = 0;
        int heightPixels = 0;
        int offsetXPixels = 0;
        int offsetYPixels = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, 0.0f, scale,
            static_cast<int>(codepoint), &widthPixels, &heightPixels, &offsetXPixels, &offsetYPixels);
        maxWidthPixels = (std::max)(maxWidthPixels, widthPixels);
        maxHeightPixels = (std::max)(maxHeightPixels, heightPixels);
        if (widthPixels > 52 || heightPixels > 52) { hasFailed = true; }
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    std::cout << arguments[1] << ": " << codepoints.size() << " glyphs, max bitmap "
        << maxWidthPixels << 'x' << maxHeightPixels << '\n';
    if (hasFailed) { return 1; }
    return 0;
}
