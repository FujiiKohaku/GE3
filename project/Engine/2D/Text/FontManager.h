#pragma once

#include "Engine/math/MathStruct.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

using FontHandle = uint32_t;

struct FontGlyph {
    Vector2 uvMin = { 0.0f, 0.0f };
    Vector2 uvMax = { 0.0f, 0.0f };
    Vector2 size = { 0.0f, 0.0f };
    Vector2 bearing = { 0.0f, 0.0f };
    float advance = 0.0f;
};

class FontManager {
public:
    static FontManager* GetInstance();
    static void Finalize();

    FontHandle LoadFont(const std::string& fontPath);
    void EnsureGlyphs(FontHandle handle, const std::string& text);
    void FlushAtlasUpdates();

    const FontGlyph& GetGlyph(FontHandle handle, uint32_t codepoint) const;
    float GetKerning(FontHandle handle, uint32_t left, uint32_t right) const;
    float GetBasePixelHeight(FontHandle handle) const;
    float GetAscent(FontHandle handle) const;
    float GetLineHeight(FontHandle handle) const;
    const std::string& GetTextureKey(FontHandle handle) const;
    Vector2 GetAtlasSize(FontHandle handle) const;

    class ConstructorKey {
        ConstructorKey() = default;
        friend class FontManager;
    };

    explicit FontManager(ConstructorKey);
    ~FontManager();

private:
    struct FontEntry;

    FontEntry& GetEntry(FontHandle handle);
    const FontEntry& GetEntry(FontHandle handle) const;
    bool AddGlyph(FontEntry& entry, uint32_t codepoint);
    void UploadAtlas(FontEntry& entry);
    std::string NormalizePath(const std::string& fontPath) const;

private:
    static std::unique_ptr<FontManager> instance_;
    std::unordered_map<FontHandle, std::unique_ptr<FontEntry>> fonts_;
    std::unordered_map<std::string, FontHandle> pathCache_;
    FontHandle nextHandle_ = 1;
};
