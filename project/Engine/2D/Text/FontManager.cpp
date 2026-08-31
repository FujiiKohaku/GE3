#include "FontManager.h"

#include "Engine/Logger/Logger.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Utf8Utility.h"
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "externals/imgui/imstb_truetype.h"

namespace {
constexpr uint32_t kAtlasWidth = 2048;
constexpr uint32_t kAtlasHeight = 2048;
constexpr uint32_t kCellSize = 64;
constexpr uint32_t kCellPadding = 6;
constexpr float kBasePixelHeight = 48.0f;
constexpr uint32_t kFallbackCodepoint = 0x3Fu;
}

struct FontManager::FontEntry {
    std::string sourcePath;
    std::string textureKey;
    std::vector<uint8_t> fontBytes;
    stbtt_fontinfo fontInfo {};
    float scale = 1.0f;
    float ascent = 0.0f;
    float lineHeight = kBasePixelHeight;
    std::vector<uint8_t> atlasAlpha;
    std::unordered_map<uint32_t, FontGlyph> glyphs;
    uint32_t nextCell = 0;
    bool textureRegistered = false;
    bool atlasDirty = false;
};

std::unique_ptr<FontManager> FontManager::instance_ = nullptr;

FontManager* FontManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<FontManager>(ConstructorKey());
    }
    return instance_.get();
}

void FontManager::Finalize()
{
    instance_.reset();
}

FontManager::FontManager(ConstructorKey)
{
}

FontManager::~FontManager() = default;

FontHandle FontManager::LoadFont(const std::string& fontPath)
{
    const std::string cacheKey = NormalizePath(fontPath);
    std::unordered_map<std::string, FontHandle>::const_iterator cached =
        pathCache_.find(cacheKey);
    if (cached != pathCache_.end()) {
        return cached->second;
    }

    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        const std::string message = "Font file is missing: " + fontPath;
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        const std::string message = "Font file is empty: " + fontPath;
        Logger::Error(message);
        throw std::runtime_error(message);
    }
    file.seekg(0, std::ios::beg);

    std::unique_ptr<FontEntry> entry = std::make_unique<FontEntry>();
    entry->sourcePath = fontPath;
    entry->textureKey = "__font_atlas__/" + cacheKey;
    entry->fontBytes.resize(static_cast<size_t>(fileSize));
    if (!file.read(
            reinterpret_cast<char*>(entry->fontBytes.data()),
            fileSize)) {
        const std::string message = "Failed to read font file: " + fontPath;
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    const int fontOffset = stbtt_GetFontOffsetForIndex(entry->fontBytes.data(), 0);
    if (fontOffset < 0 ||
        stbtt_InitFont(&entry->fontInfo, entry->fontBytes.data(), fontOffset) == 0) {
        const std::string message = "Failed to initialize font: " + fontPath;
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    entry->scale = stbtt_ScaleForPixelHeight(&entry->fontInfo, kBasePixelHeight);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&entry->fontInfo, &ascent, &descent, &lineGap);
    entry->ascent = static_cast<float>(ascent) * entry->scale;
    entry->lineHeight =
        static_cast<float>(ascent - descent + lineGap) * entry->scale;
    entry->atlasAlpha.resize(
        static_cast<size_t>(kAtlasWidth) * static_cast<size_t>(kAtlasHeight),
        0);

    for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        AddGlyph(*entry, codepoint);
    }
    UploadAtlas(*entry);

    const FontHandle handle = nextHandle_;
    nextHandle_++;
    fonts_.emplace(handle, std::move(entry));
    pathCache_.emplace(cacheKey, handle);
    return handle;
}

void FontManager::EnsureGlyphs(FontHandle handle, const std::string& text)
{
    FontEntry& entry = GetEntry(handle);
    const std::vector<uint32_t> codepoints = Utf8Utility::Decode(text);
    bool addedGlyph = false;
    for (uint32_t codepoint : codepoints) {
        if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
            continue;
        }
        if (entry.glyphs.contains(codepoint)) {
            continue;
        }
        if (AddGlyph(entry, codepoint)) {
            addedGlyph = true;
        }
    }

    if (addedGlyph) {
        entry.atlasDirty = true;
    }
}

void FontManager::FlushAtlasUpdates()
{
    for (std::pair<const FontHandle, std::unique_ptr<FontEntry>>& font : fonts_) {
        if (!font.second->atlasDirty) {
            continue;
        }
        UploadAtlas(*font.second);
        font.second->atlasDirty = false;
    }
}

const FontGlyph& FontManager::GetGlyph(FontHandle handle, uint32_t codepoint) const
{
    const FontEntry& entry = GetEntry(handle);
    std::unordered_map<uint32_t, FontGlyph>::const_iterator glyph =
        entry.glyphs.find(codepoint);
    if (glyph != entry.glyphs.end()) {
        return glyph->second;
    }
    return entry.glyphs.at(kFallbackCodepoint);
}

float FontManager::GetKerning(FontHandle handle, uint32_t left, uint32_t right) const
{
    const FontEntry& entry = GetEntry(handle);
    const int kerning = stbtt_GetCodepointKernAdvance(
        &entry.fontInfo,
        static_cast<int>(left),
        static_cast<int>(right));
    return static_cast<float>(kerning) * entry.scale;
}

float FontManager::GetBasePixelHeight(FontHandle handle) const
{
    GetEntry(handle);
    return kBasePixelHeight;
}

float FontManager::GetAscent(FontHandle handle) const
{
    return GetEntry(handle).ascent;
}

float FontManager::GetLineHeight(FontHandle handle) const
{
    return GetEntry(handle).lineHeight;
}

const std::string& FontManager::GetTextureKey(FontHandle handle) const
{
    return GetEntry(handle).textureKey;
}

Vector2 FontManager::GetAtlasSize(FontHandle handle) const
{
    GetEntry(handle);
    return {
        static_cast<float>(kAtlasWidth),
        static_cast<float>(kAtlasHeight)
    };
}

FontManager::FontEntry& FontManager::GetEntry(FontHandle handle)
{
    std::unordered_map<FontHandle, std::unique_ptr<FontEntry>>::iterator iterator =
        fonts_.find(handle);
    assert(iterator != fonts_.end());
    return *iterator->second;
}

const FontManager::FontEntry& FontManager::GetEntry(FontHandle handle) const
{
    std::unordered_map<FontHandle, std::unique_ptr<FontEntry>>::const_iterator iterator =
        fonts_.find(handle);
    assert(iterator != fonts_.end());
    return *iterator->second;
}

bool FontManager::AddGlyph(FontEntry& entry, uint32_t codepoint)
{
    if (entry.glyphs.contains(codepoint)) {
        return false;
    }
    if (stbtt_FindGlyphIndex(&entry.fontInfo, static_cast<int>(codepoint)) == 0 &&
        codepoint != kFallbackCodepoint) {
        return false;
    }

    const uint32_t cellsPerRow = kAtlasWidth / kCellSize;
    const uint32_t cellCapacity = cellsPerRow * (kAtlasHeight / kCellSize);
    if (entry.nextCell >= cellCapacity) {
        const std::string message =
            "Font atlas is full: " + entry.sourcePath;
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    int width = 0;
    int height = 0;
    int xOffset = 0;
    int yOffset = 0;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(
        &entry.fontInfo,
        0.0f,
        entry.scale,
        static_cast<int>(codepoint),
        &width,
        &height,
        &xOffset,
        &yOffset);

    const int usableCellSize = static_cast<int>(kCellSize - kCellPadding * 2);
    if (width > usableCellSize || height > usableCellSize) {
        if (bitmap != nullptr) {
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        const std::string message =
            "Font glyph is larger than an atlas cell. Codepoint: " +
            std::to_string(codepoint);
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    const uint32_t cellX = entry.nextCell % cellsPerRow;
    const uint32_t cellY = entry.nextCell / cellsPerRow;
    const uint32_t destinationX = cellX * kCellSize + kCellPadding;
    const uint32_t destinationY = cellY * kCellSize + kCellPadding;

    if (bitmap != nullptr) {
        for (int row = 0; row < height; ++row) {
            const size_t destinationOffset =
                static_cast<size_t>(destinationY + static_cast<uint32_t>(row)) * kAtlasWidth +
                destinationX;
            const size_t sourceOffset =
                static_cast<size_t>(row) * static_cast<size_t>(width);
            for (int column = 0; column < width; ++column) {
                entry.atlasAlpha[destinationOffset + static_cast<size_t>(column)] =
                    bitmap[sourceOffset + static_cast<size_t>(column)];
            }
        }
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    int advanceWidth = 0;
    int leftSideBearing = 0;
    stbtt_GetCodepointHMetrics(
        &entry.fontInfo,
        static_cast<int>(codepoint),
        &advanceWidth,
        &leftSideBearing);

    FontGlyph glyph;
    if (width > 0 && height > 0) {
        const uint32_t paddedLeft = destinationX - kCellPadding;
        const uint32_t paddedTop = destinationY - kCellPadding;
        const uint32_t paddedRight =
            destinationX + static_cast<uint32_t>(width) + kCellPadding;
        const uint32_t paddedBottom =
            destinationY + static_cast<uint32_t>(height) + kCellPadding;
        glyph.uvMin = {
            static_cast<float>(paddedLeft) / static_cast<float>(kAtlasWidth),
            static_cast<float>(paddedTop) / static_cast<float>(kAtlasHeight)
        };
        glyph.uvMax = {
            static_cast<float>(paddedRight) / static_cast<float>(kAtlasWidth),
            static_cast<float>(paddedBottom) / static_cast<float>(kAtlasHeight)
        };
        glyph.size = {
            static_cast<float>(width + static_cast<int>(kCellPadding * 2)),
            static_cast<float>(height + static_cast<int>(kCellPadding * 2))
        };
        glyph.bearing = {
            static_cast<float>(xOffset - static_cast<int>(kCellPadding)),
            static_cast<float>(yOffset - static_cast<int>(kCellPadding))
        };
    }
    glyph.advance = static_cast<float>(advanceWidth) * entry.scale;
    entry.glyphs.emplace(codepoint, glyph);
    entry.nextCell++;
    return true;
}

void FontManager::UploadAtlas(FontEntry& entry)
{
    std::vector<uint8_t> bgra;
    bgra.resize(
        static_cast<size_t>(kAtlasWidth) * static_cast<size_t>(kAtlasHeight) * 4);
    for (size_t pixelIndex = 0; pixelIndex < entry.atlasAlpha.size(); ++pixelIndex) {
        const size_t colorIndex = pixelIndex * 4;
        bgra[colorIndex + 0] = 255;
        bgra[colorIndex + 1] = 255;
        bgra[colorIndex + 2] = 255;
        bgra[colorIndex + 3] = entry.atlasAlpha[pixelIndex];
    }

    if (entry.textureRegistered) {
        TextureManager::GetInstance()->UpdateTextureFromBGRA(
            entry.textureKey,
            bgra.data(),
            kAtlasWidth,
            kAtlasHeight);
    } else {
        TextureManager::GetInstance()->LoadTextureFromBGRA(
            entry.textureKey,
            bgra.data(),
            kAtlasWidth,
            kAtlasHeight);
        entry.textureRegistered = true;
    }
}

std::string FontManager::NormalizePath(const std::string& fontPath) const
{
    std::string normalized =
        std::filesystem::path(fontPath).lexically_normal().generic_string();
    for (char& character : normalized) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return normalized;
}
