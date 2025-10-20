#include "Sprite.h"
#include "MatrixMath.h"
#include "SpriteManager.h"

#pragma region 初期化処理
// ================================
// スプライトの初期化
// ================================
void Sprite::Initialize(SpriteManager* spriteManager, std::string textureFilePath)　
{
    // SpriteManagerを記録（描画管理用）
    spriteManager_ = spriteManager;

    // 頂点バッファを作成
    CreateVertexBuffer();

    // マテリアルバッファを作成（色やテクスチャ情報）
    CreateMaterialBuffer();

    // 変換行列バッファを作成（位置・回転・スケール）
    CreateTransformationMatrixBuffer();

    // 指定のテクスチャを読み込み・番号取得
    textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
}
#pragma endregion

#pragma region 更新処理
// ================================
// 毎フレーム更新処理
// ================================
void Sprite::Update()
{
    // -------------------------------
    // 頂点データを設定
    // -------------------------------
    vertexData[0].position = { 0.0f, 1.0f, 0.0f, 1.0f }; // 左下
    vertexData[0].texcoord = { 0.0f, 1.0f };
    vertexData[0].normal = { 0.0f, 0.0f, -1.0f };

    vertexData[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
    vertexData[1].texcoord = { 0.0f, 0.0f };
    vertexData[1].normal = { 0.0f, 0.0f, -1.0f };

    vertexData[2].position = { 1.0f, 1.0f, 0.0f, 1.0f }; // 右下
    vertexData[2].texcoord = { 1.0f, 1.0f };
    vertexData[2].normal = { 0.0f, 0.0f, -1.0f };

    vertexData[3].position = { 1.0f, 0.0f, 0.0f, 1.0f }; // 右上
    vertexData[3].texcoord = { 1.0f, 0.0f };
    vertexData[3].normal = { 0.0f, 0.0f, -1.0f };

    // -------------------------------
    // インデックス（描画順）を設定
    // -------------------------------
    indexData[0] = 0;
    indexData[1] = 1;
    indexData[2] = 2;
    indexData[3] = 1;
    indexData[4] = 3;
    indexData[5] = 2;

    // -------------------------------
    // トランスフォーム設定
    // -------------------------------
    transform.translate = { position.x, position.y, 0.0f };
    transform.rotate = { 0.0f, 0.0f, rotation };
    transform.scale = { size.x, size.y, 1.0f };

    // -------------------------------
    // 行列を作成（座標変換）
    // -------------------------------
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    Matrix4x4 viewMatrix = MatrixMath::MakeIdentity4x4(); // カメラ無し
    Matrix4x4 orthoSprite = MatrixMath::MakeOrthographicMatrix(
        0.0f, 0.0f,
        float(WinApp::kClientWidth),
        float(WinApp::kClientHeight),
        0.0f, 100.0f); // スプライト用

    // -------------------------------
    // GPUへ行列を転送
    // -------------------------------
    transformationMatrixData->WVP = MatrixMath::Multiply(worldMatrix, MatrixMath::Multiply(viewMatrix, orthoSprite));
    transformationMatrixData->World = worldMatrix;
}
#pragma endregion

#pragma region 描画処理
// ================================
// スプライト描画
// ================================
void Sprite::Draw()
{
    // コマンドリストを取得
    ID3D12GraphicsCommandList* commandList = spriteManager_->GetDxCommon()->GetCommandList();

    // 頂点バッファ・インデックスバッファをセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);

    // マテリアル定数バッファ（色など）
    commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

    // 変換行列定数バッファ（位置・回転など）
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

    // テクスチャ（SRV）をセット
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex));

    // 描画コマンド実行（6頂点＝2枚の三角形）
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
#pragma endregion

#pragma region 頂点バッファの設定
// ================================
// 頂点・インデックスバッファ作成
// ================================
void Sprite::CreateVertexBuffer()
{
    // 頂点リソース作成（4頂点分）
    vertexResource = spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);

    // インデックスリソース作成（6インデックス分）
    indexResource = spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);

    // -------------------------------
    // 頂点バッファビュー設定 
    // -------------------------------
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * 4);
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // -------------------------------
    // インデックスバッファビュー設定
    // -------------------------------
    indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // -------------------------------
    // GPU書き込み用のアドレスを取得
    // -------------------------------
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
}
#pragma endregion

#pragma region マテリアルバッファの設定
// ================================
// マテリアル定数バッファ作成
// ================================
void Sprite::CreateMaterialBuffer()
{
    // マテリアルリソース作成
    materialResource = spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(Material));

    // 書き込み用アドレス取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

    // デフォルト値設定（白・ライティング無効）
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = false;
    materialData->uvTransform = MatrixMath::MakeIdentity4x4();
}
#pragma endregion

#pragma region 変換行列バッファの設定
// ================================
// 変換行列定数バッファ作成
// ================================
void Sprite::CreateTransformationMatrixBuffer()
{
    // リソース作成
    transformationMatrixResource = spriteManager_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));

    // 書き込み用アドレス取得
    transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

    // 安全のため単位行列で初期化
    transformationMatrixData->WVP = MatrixMath::MakeIdentity4x4();
    transformationMatrixData->World = MatrixMath::MakeIdentity4x4();
}
#pragma endregion
