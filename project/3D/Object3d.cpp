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
    Matrix4x4 localMatrix = model_->GetModelData().rootNode.localMatrix;

    if (animation_) {
        localMatrix = animation_->GetLocalMatrix(model_->GetModelData().rootNode.name);
    }

    Matrix4x4 worldMatrix = MatrixMath::Multiply(localMatrix,MatrixMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate));



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
ModelData Object3d::LoadModeFile(const std::string& directoryPath,
    const std::string filename)
{
    ModelData modelData;

    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const aiScene* scene = importer.ReadFile(
        filePath.c_str(),
        aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

    assert(scene);
    assert(scene->HasMeshes());

    // -------------------------
    // Mesh -> MeshPrimitive
    // -------------------------
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        MeshPrimitive primitive;
        primitive.mode = PrimitiveMode::Triangles; // 今は固定でOK

        // ---- vertices ----
        for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
            VertexData vertex {};

            aiVector3D pos = mesh->mVertices[v];
            aiVector3D nrm = mesh->HasNormals()
                ? mesh->mNormals[v]
                : aiVector3D(0, 1, 0);

            aiVector3D uv = mesh->HasTextureCoords(0)
                ? mesh->mTextureCoords[0][v]
                : aiVector3D(0, 0, 0);

            // 右手 → 左手（X反転）
            vertex.position = { -pos.x, pos.y, pos.z, 1.0f };
            vertex.normal = { -nrm.x, nrm.y, nrm.z };
            vertex.texcoord = { uv.x, uv.y };

            primitive.vertices.push_back(vertex);
        }

        // ---- indices ----
        if (mesh->HasFaces()) {
            for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
                aiFace& face = mesh->mFaces[f];
                // Triangulate してるので 3 のはず
                for (uint32_t i = 0; i < face.mNumIndices; ++i) {
                    primitive.indices.push_back(face.mIndices[i]);
                }
            }
        }
        // indices が空なら drawArrays 扱いでOK

        modelData.primitives.push_back(primitive);
    }

    // -------------------------
    // Material（最小）
    // -------------------------
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial* material = scene->mMaterials[materialIndex];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
            break; // 1枚で十分
        }
    }

    // -------------------------
    // Node（既存の処理）
    // -------------------------
    modelData.rootNode = ReadNode(scene->mRootNode);

    return modelData;
}

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
void Object3d::SetAnimation(PlayAnimation* anim)
{
    animation_ = anim;
}