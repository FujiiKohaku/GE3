#include "Object3d.h"
#include "MatrixMath.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3dManager.h"
#include <cassert>
#include <fstream>
#include <sstream>
#pragma region 初期化処理
void Object3d::Initialize(Object3dManager* object3DManager)
{
    // Object3dManager と DebugCamera を受け取って保持
    object3dManager_ = object3DManager;

    camera_ = object3dManager_->GetDefaultCamera();
    // ================================
    // Transformバッファ初期化
    // ================================
    transformationMatrixResource = object3dManager_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource->SetName(L"Object3d::TransformCB");
    transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
    transformationMatrixData->WVP = MatrixMath::MakeIdentity4x4();
    transformationMatrixData->World = MatrixMath::MakeIdentity4x4();

    // ================================
    // 平行光源データ初期化
    // ================================
    // directionalLightResource = object3dManager_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
    // directionalLightResource->SetName(L"Object3d::DirectionalLightCB");
    // directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    // directionalLightData->color = { 1, 1, 1, 1 };
    // directionalLightData->direction = MatrixMath::Normalize({ 0, -1, 0 });
    // directionalLightData->intensity = 1.0f;
    // マテリアルリソース作成
    materialResource = object3dManager_->GetDxCommon()->CreateBufferResource(sizeof(Material));
    materialResource->SetName(L"Object3d::MaterialCB");

    // マテリアル初期化
    // 書き込み用アドレス取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    // デフォルト値設定（白・ライティング無効）
    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData_->enableLighting = true;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
    materialData_->shininess = 32.0f;

    // ================================
    // Transform初期値設定
    // ================================
    transform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    cameraTransform = { { 1.0f, 1.0f, 1.0f }, { 0.3f, 0.0f, 0.0f }, { 0.0f, 4.0f, -10.0f } };
}
#pragma endregion

#pragma region 更新処理

void Object3d::Update()
{
    // ================================
    // 各種行列を作成
    // ================================

    //  モデル自身のワールド行列（スケール・回転・移動）
    Matrix4x4 worldMatrix = MatrixMath::Multiply(
        model_->GetModelData().rootNode.localMatrix,
        MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate));


    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
    } else {
        worldViewProjectionMatrix = worldMatrix;
    }

    // ================================
    // WVP行列を計算して転送
    // ================================
    transformationMatrixData->WVP = worldViewProjectionMatrix;

    // ワールド行列も送る（ライティングなどで使用）
    transformationMatrixData->World = worldMatrix;


    Matrix4x4 inv = MatrixMath::Inverse(worldViewProjectionMatrix);
    transformationMatrixData->WorldInverseTranspose = MatrixMath::Transpose(inv);
}

#pragma endregion

#pragma region 描画処理
void Object3d::Draw()
{
    ID3D12GraphicsCommandList* commandList = object3dManager_->GetDxCommon()->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
    // Transform定数バッファをセット
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());

    // ライト情報をセット
    // commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera_->GetGPUAddress());

    // モデルが設定されていれば描画
    if (model_) {
        model_->Draw();
    }
}
#pragma endregion

#pragma region OBJ読み込み処理
// ===============================================
// OBJファイルの読み込み
// ===============================================
ModelData Object3d::LoadModeFile(const std::string& directoryPath, const std::string filename)
{
    // 1.中で必要となる変数の宣言
    ModelData modelData; // 構築するModelData
    // ファイルから読んだ一行を格納するもの

    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const aiScene* scene = importer.ReadFile(
        filePath.c_str(),
        aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

    assert(scene->HasMeshes());
    // 3.実際にファイルを読み、ModelDataを構築していく
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        assert(mesh->HasNormals());
        assert(mesh->HasTextureCoords(0));

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3); // 三角形のみサポート
            for (uint32_t element = 0; element < face.mNumIndices; ++element) {

                uint32_t vertexIndex = face.mIndices[element];

                aiVector3D position = mesh->mVertices[vertexIndex];
                aiVector3D normal = mesh->mNormals[vertexIndex];
                aiVector3D texcoord = mesh->mTextureCoords[0][vertexIndex];

                VertexData vertex {};

                vertex.position = { position.x, position.y, position.z, 1.0f };
                vertex.normal = { normal.x, normal.y, normal.z };
                vertex.texcoord = { texcoord.x, texcoord.y };

                // aiProcess_MakeLeftHanded は z を -1 で反転するが、
                // 右手→左手変換をするので手動で対応
                vertex.position.x *= -1.0f;
                vertex.normal.x *= -1.0f;

                modelData.vertices.push_back(vertex);
            }
        }
    }
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {

        aiMaterial* material = scene->mMaterials[materialIndex];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {

            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);

            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
        }
    }
    modelData.rootNode = ReadNode(scene->mRootNode);

    // 4.ModelDataを返す
    return modelData;
}
#pragma endregion

#pragma region MTL読み込み処理
//// ===============================================
//// マテリアル（.mtl）ファイルの読み込み
//// ===============================================
// MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
//{
//
//     // 1.中で必要となる変数の宣言
//     MaterialData materialData; // 構築するMaterialData
//     // 2.ファイルを開く
//     std::string line; // ファイルから読んだ１行を格納するもの
//     std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
//     assert(file.is_open()); // とりあえず開けなかったら止める
//     // 3.実際にファイルを読み、MaterialDataを構築していく
//     while (std::getline(file, line)) {
//         std::string identifier;
//         std::istringstream s(line);
//         s >> identifier;
//         // identifierに応じた処理
//         if (identifier == "map_Kd") {
//             std::string textureFilename;
//             s >> textureFilename;
//             // 連結してファイルパスにする
//             materialData.textureFilePath = directoryPath + "/" + textureFilename;
//         }
//     }
//     // 4.materialDataを返す
//     return materialData;
// }

#pragma endregion

void Object3d::SetModel(const std::string& filePath)
{
    // モデルを検索してセットする
    model_ = ModelManager::GetInstance()->FindModel(filePath);
}
Node Object3d::ReadNode(aiNode* node)
{
    Node result;

    aiMatrix4x4 aiLocal = node->mTransformation;
    aiLocal.Transpose();

    result.localMatrix.m[0][0] = aiLocal[0][0];
    result.localMatrix.m[0][1] = aiLocal[0][1];
    result.localMatrix.m[0][2] = aiLocal[0][2];
    result.localMatrix.m[0][3] = aiLocal[0][3];

    result.localMatrix.m[1][0] = aiLocal[1][0];
    result.localMatrix.m[1][1] = aiLocal[1][1];
    result.localMatrix.m[1][2] = aiLocal[1][2];
    result.localMatrix.m[1][3] = aiLocal[1][3];

    result.localMatrix.m[2][0] = aiLocal[2][0];
    result.localMatrix.m[2][1] = aiLocal[2][1];
    result.localMatrix.m[2][2] = aiLocal[2][2];
    result.localMatrix.m[2][3] = aiLocal[2][3];

    result.localMatrix.m[3][0] = aiLocal[3][0];
    result.localMatrix.m[3][1] = aiLocal[3][1];
    result.localMatrix.m[3][2] = aiLocal[3][2];
    result.localMatrix.m[3][3] = aiLocal[3][3];

    result.name = node->mName.C_Str();

    result.children.resize(node->mNumChildren);
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        result.children[i] = ReadNode(node->mChildren[i]);
    }

    return result;
}
