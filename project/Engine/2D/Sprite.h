#pragma once

#include "Engine/TextureManager/TextureManager.h"
#include "Engine/math/SpriteStruct.h"
#include "SpriteMeshManager.h"
#include "SpriteRenderManager.h"
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

class SpriteManager;

class Sprite {
public:
    Sprite() = default;
    ~Sprite();

    void Initialize(SpriteManager* spriteManager, std::string textureFilePath);
    void Update();
    void Draw();

    const Vector2& GetPosition() const { return position_; }
    void SetPosition(const Vector2& position) { position_ = position; }

    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    const Vector4& GetColor() const { return materialData_->color; }
    void SetColor(const Vector4& color) { materialData_->color = color; }

    const Vector2& GetSize() const { return size_; }
    void SetSize(const Vector2& size) { size_ = size; }

    const Vector2& GetAnchorPoint() const { return anchorPoint_; }
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

    void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
    void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
    bool GetIsFlipX() const { return isFlipX_; }
    bool GetIsFlipY() const { return isFlipY_; }

    void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }
    void SetTextureSize(const Vector2& size) { textureSize_ = size; }
    const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
    const Vector2& GetTextureSize() const { return textureSize_; }

    void SetShaderPaths(const std::string& vertexShaderPath, const std::string& pixelShaderPath);
    void SetVertexShaderPath(const std::string& vertexShaderPath);
    void SetPixelShaderPath(const std::string& pixelShaderPath);
    void SetBlendMode(BlendMode blendMode);
    void SetMaterial(const std::string& shaderFolderPath);
    const std::string& GetMaterialName() const { return materialName_; }
    const std::string& GetMaterialFolderPath() const { return materialFolderPath_; }
    const SpriteGraphicsPipelineDesc& GetPipelineDesc() const { return pipelineDesc_; }

    void SetQuadMesh();
    void SetGridMesh(uint32_t divisionX, uint32_t divisionY);
    const SpriteMeshDesc& GetMeshDesc() const { return meshDesc_; }

    void SetEffectAmplitude(float amplitude) { effectParameterData_->amplitude = amplitude; }
    void SetEffectFrequency(float frequency) { effectParameterData_->frequency = frequency; }
    void SetEffectSpeed(float speed) { effectParameterData_->speed = speed; }
    void SetEffectPhase(float phase) { effectParameterData_->phase = phase; }
    void SetEffectDirection(const Vector2& direction) { effectParameterData_->direction = direction; }
    void SetEffectStrength(float strength) { effectParameterData_->strength = strength; }
    void SetEffectThreshold(float threshold) { effectParameterData_->threshold = threshold; }

private:
    void CreateMaterialBuffer();
    void CreateTransformationMatrixBuffer();
    void CreateEffectParameterBuffer();
    void AdjustTextureSize();
    void UpdateTransform();
    void UpdateUvTransform();

private:
    Vector2 position_ = { 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Vector2 size_ = { 640.0f, 360.0f };

    SpriteManager* spriteManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    SpriteTransform* transformationMatrixData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    SpriteMaterialConstants* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> effectParameterResource_;
    SpriteEffectParameters* effectParameterData_ = nullptr;

    std::string textureFilePath_;
    Vector2 anchorPoint_ = { 0.0f, 0.0f };
    bool isFlipX_ = false;
    bool isFlipY_ = false;
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    Vector2 textureSize_ = { 0.0f, 0.0f };

    SpriteGraphicsPipelineDesc pipelineDesc_;
    SpriteMeshDesc meshDesc_;
    std::string materialName_ = "Standard";
    std::string materialFolderPath_ = "resources/Shaders/Sprite/Standard";
};
