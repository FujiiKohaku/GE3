#include "Sprite.h"
#include "Engine/math/EngineStruct.h"
#include "SpriteManager.h"
#include <cassert>

void Sprite::Initialize(SpriteManager* spriteManager, std::string textureFilePath)
{
    assert(spriteManager);
    spriteManager_ = spriteManager;

    CreateMaterialBuffer();
    CreateTransformationMatrixBuffer();
    CreateEffectParameterBuffer();

    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    textureFilePath_ = textureFilePath;
    AdjustTextureSize();
}

void Sprite::Update()
{
    UpdateTransform();
    UpdateUvTransform();
    effectParameterData_->spriteSize = size_;
}

void Sprite::Draw()
{
    assert(spriteManager_);
    SpriteRenderManager* renderManager = spriteManager_->GetRenderManager();
    SpriteMeshManager* meshManager = spriteManager_->GetMeshManager();
    assert(renderManager);
    assert(meshManager);

    renderManager->BindPipeline(pipelineDesc_);
    const SpriteMesh& mesh = meshManager->GetOrCreateMesh(meshDesc_);

    ID3D12GraphicsCommandList* commandList = spriteManager_->GetDxCommon()->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
    commandList->IASetIndexBuffer(&mesh.indexBufferView);
    commandList->SetGraphicsRootConstantBufferView(
        0,
        transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1,
        materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
    commandList->SetGraphicsRootConstantBufferView(
        3,
        effectParameterResource_->GetGPUVirtualAddress());
    commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

void Sprite::SetShaderPaths(
    const std::string& vertexShaderPath,
    const std::string& pixelShaderPath)
{
    pipelineDesc_.vertexShaderPath = vertexShaderPath;
    pipelineDesc_.pixelShaderPath = pixelShaderPath;
}

void Sprite::SetVertexShaderPath(const std::string& vertexShaderPath)
{
    pipelineDesc_.vertexShaderPath = vertexShaderPath;
}

void Sprite::SetPixelShaderPath(const std::string& pixelShaderPath)
{
    pipelineDesc_.pixelShaderPath = pixelShaderPath;
}

void Sprite::SetBlendMode(BlendMode blendMode)
{
    pipelineDesc_.blendMode = blendMode;
}

void Sprite::SetMaterial(const std::string& shaderFolderPath)
{
    assert(spriteManager_);
    SpriteMaterialManager* materialManager = spriteManager_->GetMaterialManager();
    assert(materialManager);

    const SpriteMaterialConfig& config =
        materialManager->GetOrLoadMaterial(shaderFolderPath);
    pipelineDesc_ = config.pipelineDesc;
    meshDesc_ = config.meshDesc;
    *effectParameterData_ = config.effectParameters;
    effectParameterData_->spriteSize = size_;
    materialName_ = config.name;
    materialFolderPath_ = shaderFolderPath;
}

void Sprite::SetQuadMesh()
{
    meshDesc_.divisionX = 1;
    meshDesc_.divisionY = 1;
}

void Sprite::SetGridMesh(uint32_t divisionX, uint32_t divisionY)
{
    meshDesc_.divisionX = divisionX;
    meshDesc_.divisionY = divisionY;
    if (meshDesc_.divisionX == 0) {
        meshDesc_.divisionX = 1;
    }
    if (meshDesc_.divisionY == 0) {
        meshDesc_.divisionY = 1;
    }
}

void Sprite::CreateMaterialBuffer()
{
    materialResource_ =
        spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(SpriteMaterialConstants));
    materialResource_->SetName(L"Sprite::MaterialCB");
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
}

void Sprite::CreateTransformationMatrixBuffer()
{
    transformationMatrixResource_ =
        spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(SpriteTransform));
    transformationMatrixResource_->SetName(L"Sprite::TransformCB");
    transformationMatrixResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MatrixMath::MakeIdentity4x4();
}

void Sprite::CreateEffectParameterBuffer()
{
    effectParameterResource_ =
        spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(SpriteEffectParameters));
    effectParameterResource_->SetName(L"Sprite::EffectParameters");
    effectParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&effectParameterData_));

    effectParameterData_->amplitude = 0.0f;
    effectParameterData_->frequency = 1.0f;
    effectParameterData_->speed = 1.0f;
    effectParameterData_->phase = 0.0f;
    effectParameterData_->direction = { 1.0f, 0.0f };
    effectParameterData_->strength = 1.0f;
    effectParameterData_->threshold = 0.0f;
    effectParameterData_->spriteSize = size_;
    effectParameterData_->padding = { 0.0f, 0.0f };
}

void Sprite::AdjustTextureSize()
{
    const DirectX::TexMetadata& metadata =
        TextureManager::GetInstance()->GetMetaData(textureFilePath_);
    textureSize_.x = static_cast<float>(metadata.width);
    textureSize_.y = static_cast<float>(metadata.height);
    size_ = textureSize_;
}

void Sprite::UpdateTransform()
{
    float clientWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float clientHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    if (clientWidth <= 0.0f) {
        clientWidth = static_cast<float>(WinApp::kClientWidth);
    }
    if (clientHeight <= 0.0f) {
        clientHeight = static_cast<float>(WinApp::kClientHeight);
    }

    Vector3 localScale = { 1.0f, 1.0f, 1.0f };
    Vector3 localTranslate = { -anchorPoint_.x, -anchorPoint_.y, 0.0f };
    if (isFlipX_) {
        localScale.x = -1.0f;
        localTranslate.x = anchorPoint_.x;
    }
    if (isFlipY_) {
        localScale.y = -1.0f;
        localTranslate.y = anchorPoint_.y;
    }

    const Vector3 localRotation = { 0.0f, 0.0f, 0.0f };
    const Matrix4x4 localMatrix = MatrixMath::MakeAffineMatrix(
        localScale,
        localRotation,
        localTranslate);
    const Vector3 objectScale = { size_.x, size_.y, 1.0f };
    const Vector3 objectRotation = { 0.0f, 0.0f, rotation_ };
    const Vector3 objectTranslate = { position_.x, position_.y, 0.0f };
    const Matrix4x4 objectMatrix = MatrixMath::MakeAffineMatrix(
        objectScale,
        objectRotation,
        objectTranslate);
    const Matrix4x4 worldMatrix = MatrixMath::Multiply(localMatrix, objectMatrix);
    const Matrix4x4 orthoMatrix = MatrixMath::MakeOrthographicMatrix(
        0.0f,
        0.0f,
        clientWidth,
        clientHeight,
        0.0f,
        100.0f);
    transformationMatrixData_->WVP = MatrixMath::Multiply(worldMatrix, orthoMatrix);
}

void Sprite::UpdateUvTransform()
{
    const DirectX::TexMetadata& metadata =
        TextureManager::GetInstance()->GetMetaData(textureFilePath_);
    const float textureWidth = static_cast<float>(metadata.width);
    const float textureHeight = static_cast<float>(metadata.height);

    Vector3 uvScale = { 1.0f, 1.0f, 1.0f };
    Vector3 uvTranslate = { 0.0f, 0.0f, 0.0f };
    if (textureWidth > 0.0f) {
        uvScale.x = textureSize_.x / textureWidth;
        uvTranslate.x = textureLeftTop_.x / textureWidth;
    }
    if (textureHeight > 0.0f) {
        uvScale.y = textureSize_.y / textureHeight;
        uvTranslate.y = textureLeftTop_.y / textureHeight;
    }

    const Matrix4x4 scaleMatrix = MatrixMath::Matrix4x4MakeScaleMatrix(uvScale);
    const Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(uvTranslate);
    materialData_->uvTransform = MatrixMath::Multiply(scaleMatrix, translateMatrix);
}

Sprite::~Sprite()
{
    if (transformationMatrixResource_ && transformationMatrixData_) {
        transformationMatrixResource_->Unmap(0, nullptr);
    }
    if (materialResource_ && materialData_) {
        materialResource_->Unmap(0, nullptr);
    }
    if (effectParameterResource_ && effectParameterData_) {
        effectParameterResource_->Unmap(0, nullptr);
    }
}
