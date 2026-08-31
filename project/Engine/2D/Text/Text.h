#pragma once

#include "FontManager.h"
#include "TextStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

enum class TextHorizontalAlignment {
    Left,
    Center,
    Right
};

class Text {
public:
    Text() = default;
    ~Text();

    void Initialize(const std::string& fontPath);
    void Update();
    void Draw();

    void SetText(const std::string& text);
    const std::string& GetText() const { return text_; }

    void SetPosition(const Vector2& position) { position_ = position; }
    const Vector2& GetPosition() const { return position_; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetFontSize(float fontSize);
    float GetFontSize() const { return fontSize_; }
    void SetColor(const Vector4& color) { color_ = color; }
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
    void SetMaxWidth(float maxWidth);
    void SetLetterSpacing(float letterSpacing);
    void SetLineSpacing(float lineSpacing);
    void SetHorizontalAlignment(TextHorizontalAlignment alignment);
    void SetOutlineColor(const Vector4& color) { outlineColor_ = color; }
    void SetOutlineWidth(float width) { outlineWidth_ = width; }
    void SetShadowColor(const Vector4& color) { shadowColor_ = color; }
    void SetShadowOffset(const Vector2& offset) { shadowOffset_ = offset; }

    const Vector2& Measure() const { return measuredSize_; }

private:
    struct TextLine;

    void CreateConstantBuffers();
    void BuildGeometry();
    void CreateOrResizeGeometryBuffers(size_t vertexCount, size_t indexCount);
    void UpdateTransform();
    void UpdateAppearance();

private:
    FontHandle fontHandle_ = 0;
    std::string text_;
    Vector2 position_ = { 0.0f, 0.0f };
    Vector2 anchorPoint_ = { 0.0f, 0.0f };
    Vector2 measuredSize_ = { 0.0f, 0.0f };
    float rotation_ = 0.0f;
    float fontSize_ = 32.0f;
    float maxWidth_ = 0.0f;
    float letterSpacing_ = 0.0f;
    float lineSpacing_ = 0.0f;
    TextHorizontalAlignment horizontalAlignment_ = TextHorizontalAlignment::Left;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 outlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector4 shadowColor_ = { 0.0f, 0.0f, 0.0f, 0.6f };
    float outlineWidth_ = 0.0f;
    Vector2 shadowOffset_ = { 0.0f, 0.0f };
    bool geometryDirty_ = true;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    TextVertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    size_t vertexCapacity_ = 0;
    size_t indexCapacity_ = 0;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};
    uint32_t indexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TextTransformConstants* transformData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> appearanceResource_;
    TextAppearanceConstants* appearanceData_ = nullptr;
};
