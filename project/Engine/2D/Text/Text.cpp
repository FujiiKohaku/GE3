#include "Text.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/2D/Text/Utf8Utility.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cassert>
#include <cstring>

struct Text::TextLine {
    std::vector<uint32_t> codepoints;
    float width = 0.0f;
};

void Text::Initialize(const std::string& fontPath)
{
    fontHandle_ = FontManager::GetInstance()->LoadFont(fontPath);
    CreateConstantBuffers();
    geometryDirty_ = true;
}

void Text::Update()
{
    assert(fontHandle_ != 0);
    FontManager::GetInstance()->EnsureGlyphs(fontHandle_, text_);
    if (geometryDirty_) {
        BuildGeometry();
        geometryDirty_ = false;
    }
    UpdateTransform();
    UpdateAppearance();
}

void Text::Draw()
{
    if (indexCount_ == 0) {
        return;
    }

    TextRenderer::GetInstance()->Draw(
        vertexBufferView_,
        indexBufferView_,
        indexCount_,
        transformResource_->GetGPUVirtualAddress(),
        appearanceResource_->GetGPUVirtualAddress(),
        FontManager::GetInstance()->GetTextureKey(fontHandle_));
}

void Text::SetText(const std::string& text)
{
    if (text_ == text) {
        return;
    }
    text_ = text;
    geometryDirty_ = true;
}

void Text::SetFontSize(float fontSize)
{
    if (fontSize <= 0.0f) {
        fontSize = 1.0f;
    }
    fontSize_ = fontSize;
    geometryDirty_ = true;
}

void Text::SetMaxWidth(float maxWidth)
{
    if (maxWidth < 0.0f) {
        maxWidth = 0.0f;
    }
    maxWidth_ = maxWidth;
    geometryDirty_ = true;
}

void Text::SetLetterSpacing(float letterSpacing)
{
    letterSpacing_ = letterSpacing;
    geometryDirty_ = true;
}

void Text::SetLineSpacing(float lineSpacing)
{
    lineSpacing_ = lineSpacing;
    geometryDirty_ = true;
}

void Text::SetHorizontalAlignment(TextHorizontalAlignment alignment)
{
    horizontalAlignment_ = alignment;
    geometryDirty_ = true;
}

void Text::CreateConstantBuffers()
{
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();

    transformResource_ =
        dxCommon->CreateBufferResource(sizeof(TextTransformConstants));
    transformResource_->SetName(L"Text::TransformCB");
    transformResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformData_));
    transformData_->WVP = MatrixMath::MakeIdentity4x4();

    appearanceResource_ =
        dxCommon->CreateBufferResource(sizeof(TextAppearanceConstants));
    appearanceResource_->SetName(L"Text::AppearanceCB");
    appearanceResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&appearanceData_));
    UpdateAppearance();
}

void Text::BuildGeometry()
{
    FontManager* fontManager = FontManager::GetInstance();
    const std::vector<uint32_t> codepoints = Utf8Utility::Decode(text_);
    if (codepoints.empty()) {
        indexCount_ = 0;
        measuredSize_ = { 0.0f, 0.0f };
        return;
    }

    const float scale =
        fontSize_ / fontManager->GetBasePixelHeight(fontHandle_);
    const FontGlyph& spaceGlyph = fontManager->GetGlyph(fontHandle_, ' ');

    std::vector<TextLine> lines;
    TextLine currentLine;
    uint32_t previousCodepoint = 0;
    bool hasPreviousCodepoint = false;

    for (uint32_t codepoint : codepoints) {
        if (codepoint == '\r') {
            continue;
        }
        if (codepoint == '\n') {
            lines.push_back(currentLine);
            currentLine = TextLine();
            hasPreviousCodepoint = false;
            continue;
        }

        float leading = 0.0f;
        float advance = 0.0f;
        if (codepoint == '\t') {
            advance = spaceGlyph.advance * scale * 4.0f;
        } else {
            const FontGlyph& glyph = fontManager->GetGlyph(fontHandle_, codepoint);
            advance = glyph.advance * scale;
            if (hasPreviousCodepoint) {
                leading += letterSpacing_;
                leading += fontManager->GetKerning(
                    fontHandle_,
                    previousCodepoint,
                    codepoint) * scale;
            }
        }

        if (maxWidth_ > 0.0f &&
            !currentLine.codepoints.empty() &&
            currentLine.width + leading + advance > maxWidth_) {
            lines.push_back(currentLine);
            currentLine = TextLine();
            leading = 0.0f;
            hasPreviousCodepoint = false;
        }

        currentLine.width += leading + advance;
        currentLine.codepoints.push_back(codepoint);
        if (codepoint == '\t') {
            hasPreviousCodepoint = false;
        } else {
            previousCodepoint = codepoint;
            hasPreviousCodepoint = true;
        }
    }
    lines.push_back(currentLine);

    float maximumLineWidth = 0.0f;
    for (const TextLine& line : lines) {
        if (line.width > maximumLineWidth) {
            maximumLineWidth = line.width;
        }
    }

    float layoutWidth = maximumLineWidth;
    if (maxWidth_ > 0.0f) {
        layoutWidth = maxWidth_;
    }
    const float lineHeight = fontManager->GetLineHeight(fontHandle_) * scale;
    measuredSize_.x = layoutWidth;
    measuredSize_.y = lineHeight * static_cast<float>(lines.size());
    if (lines.size() > 1) {
        measuredSize_.y += lineSpacing_ * static_cast<float>(lines.size() - 1);
    }

    std::vector<TextVertexData> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(codepoints.size() * 4);
    indices.reserve(codepoints.size() * 6);

    const float ascent = fontManager->GetAscent(fontHandle_) * scale;
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const TextLine& line = lines[lineIndex];
        float lineOffsetX = 0.0f;
        if (horizontalAlignment_ == TextHorizontalAlignment::Center) {
            lineOffsetX = (layoutWidth - line.width) * 0.5f;
        } else if (horizontalAlignment_ == TextHorizontalAlignment::Right) {
            lineOffsetX = layoutWidth - line.width;
        }

        float cursorX = lineOffsetX;
        const float baseline =
            ascent + static_cast<float>(lineIndex) * (lineHeight + lineSpacing_);
        previousCodepoint = 0;
        hasPreviousCodepoint = false;

        for (uint32_t codepoint : line.codepoints) {
            if (codepoint == '\t') {
                cursorX += spaceGlyph.advance * scale * 4.0f;
                hasPreviousCodepoint = false;
                continue;
            }

            if (hasPreviousCodepoint) {
                cursorX += letterSpacing_;
                cursorX += fontManager->GetKerning(
                    fontHandle_,
                    previousCodepoint,
                    codepoint) * scale;
            }

            const FontGlyph& glyph = fontManager->GetGlyph(fontHandle_, codepoint);
            if (glyph.size.x > 0.0f && glyph.size.y > 0.0f) {
                const float left = cursorX + glyph.bearing.x * scale;
                const float top = baseline + glyph.bearing.y * scale;
                const float right = left + glyph.size.x * scale;
                const float bottom = top + glyph.size.y * scale;
                const uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

                vertices.push_back({ { left, top, 0.0f, 1.0f }, glyph.uvMin });
                vertices.push_back({
                    { right, top, 0.0f, 1.0f },
                    { glyph.uvMax.x, glyph.uvMin.y }
                });
                vertices.push_back({
                    { left, bottom, 0.0f, 1.0f },
                    { glyph.uvMin.x, glyph.uvMax.y }
                });
                vertices.push_back({ { right, bottom, 0.0f, 1.0f }, glyph.uvMax });

                indices.push_back(vertexOffset + 0);
                indices.push_back(vertexOffset + 1);
                indices.push_back(vertexOffset + 2);
                indices.push_back(vertexOffset + 1);
                indices.push_back(vertexOffset + 3);
                indices.push_back(vertexOffset + 2);
            }

            cursorX += glyph.advance * scale;
            previousCodepoint = codepoint;
            hasPreviousCodepoint = true;
        }
    }

    if (vertices.empty()) {
        indexCount_ = 0;
        return;
    }

    CreateOrResizeGeometryBuffers(vertices.size(), indices.size());
    std::memcpy(
        vertexData_,
        vertices.data(),
        vertices.size() * sizeof(TextVertexData));
    std::memcpy(
        indexData_,
        indices.data(),
        indices.size() * sizeof(uint32_t));
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void Text::CreateOrResizeGeometryBuffers(size_t vertexCount, size_t indexCount)
{
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();

    if (vertexCount > vertexCapacity_) {
        if (vertexResource_ && vertexData_) {
            vertexResource_->Unmap(0, nullptr);
        }
        vertexCapacity_ = vertexCount;
        vertexResource_ = dxCommon->CreateBufferResource(
            vertexCapacity_ * sizeof(TextVertexData));
        vertexResource_->SetName(L"Text::VertexBuffer");
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes =
            static_cast<UINT>(vertexCapacity_ * sizeof(TextVertexData));
        vertexBufferView_.StrideInBytes = sizeof(TextVertexData);
    }

    if (indexCount > indexCapacity_) {
        if (indexResource_ && indexData_) {
            indexResource_->Unmap(0, nullptr);
        }
        indexCapacity_ = indexCount;
        indexResource_ = dxCommon->CreateBufferResource(
            indexCapacity_ * sizeof(uint32_t));
        indexResource_->SetName(L"Text::IndexBuffer");
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes =
            static_cast<UINT>(indexCapacity_ * sizeof(uint32_t));
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }
}

void Text::UpdateTransform()
{
    float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    if (clientWidth <= 0.0f) {
        clientWidth = static_cast<float>(WinApp::kClientWidth);
    }
    if (clientHeight <= 0.0f) {
        clientHeight = static_cast<float>(WinApp::kClientHeight);
    }

    const Vector3 localScale = { 1.0f, 1.0f, 1.0f };
    const Vector3 localRotation = { 0.0f, 0.0f, 0.0f };
    const Vector3 localTranslate = {
        -anchorPoint_.x * measuredSize_.x,
        -anchorPoint_.y * measuredSize_.y,
        0.0f
    };
    const Matrix4x4 localMatrix = MatrixMath::MakeAffineMatrix(
        localScale,
        localRotation,
        localTranslate);

    const Vector3 objectScale = { 1.0f, 1.0f, 1.0f };
    const Vector3 objectRotation = { 0.0f, 0.0f, rotation_ };
    const Vector3 objectTranslate = { position_.x, position_.y, 0.0f };
    const Matrix4x4 objectMatrix = MatrixMath::MakeAffineMatrix(
        objectScale,
        objectRotation,
        objectTranslate);
    const Matrix4x4 worldMatrix = MatrixMath::Multiply(localMatrix, objectMatrix);
    const Matrix4x4 orthographicMatrix = MatrixMath::MakeOrthographicMatrix(
        0.0f,
        0.0f,
        clientWidth,
        clientHeight,
        0.0f,
        100.0f);
    transformData_->WVP = MatrixMath::Multiply(worldMatrix, orthographicMatrix);
}

void Text::UpdateAppearance()
{
    if (appearanceData_ == nullptr || fontHandle_ == 0) {
        return;
    }
    appearanceData_->color = color_;
    appearanceData_->outlineColor = outlineColor_;
    appearanceData_->shadowColor = shadowColor_;
    appearanceData_->atlasSize = FontManager::GetInstance()->GetAtlasSize(fontHandle_);
    appearanceData_->shadowOffset = shadowOffset_;
    appearanceData_->outlineWidth = outlineWidth_;
    appearanceData_->padding[0] = 0.0f;
    appearanceData_->padding[1] = 0.0f;
    appearanceData_->padding[2] = 0.0f;
}

Text::~Text()
{
    if (vertexResource_ && vertexData_) {
        vertexResource_->Unmap(0, nullptr);
    }
    if (indexResource_ && indexData_) {
        indexResource_->Unmap(0, nullptr);
    }
    if (transformResource_ && transformData_) {
        transformResource_->Unmap(0, nullptr);
    }
    if (appearanceResource_ && appearanceData_) {
        appearanceResource_->Unmap(0, nullptr);
    }
}
