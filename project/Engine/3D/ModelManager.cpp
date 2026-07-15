#include "ModelManager.h"
#include "Engine/math/MatrixMath.h"
#include <utility>

namespace {
const char* kPrimitivePlanePrefix = "Primitive/Plane/";
const char* kDefaultPlaneTexture = "resources/Textures/BaseColor_Cube.png";

std::string BuildPlaneKey(const std::string& texturePath, float tilingX, float tilingY)
{
    std::string key = kPrimitivePlanePrefix;
    key += texturePath + "_" + std::to_string(tilingX) + "_" + std::to_string(tilingY);
    return key;
}

void SetupDefaultRootNode(ModelData& modelData, const std::string& name)
{
    modelData.rootNode.name = name;
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    modelData.rootNode.transform.scale = { 1.0f, 1.0f, 1.0f };
    modelData.rootNode.transform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    modelData.rootNode.transform.translate = { 0.0f, 0.0f, 0.0f };
}

ModelData CreatePlaneModelData(const std::string& texturePath, float tilingX, float tilingY)
{
    ModelData modelData {};
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices.resize(4);

    primitive.vertices[0].position = { -0.5f, 0.5f, 0.0f, 1.0f };
    primitive.vertices[0].texcoord = { 0.0f, 0.0f };
    primitive.vertices[0].normal = { 0.0f, 0.0f, -1.0f };

    primitive.vertices[1].position = { 0.5f, 0.5f, 0.0f, 1.0f };
    primitive.vertices[1].texcoord = { tilingX, 0.0f };
    primitive.vertices[1].normal = { 0.0f, 0.0f, -1.0f };

    primitive.vertices[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    primitive.vertices[2].texcoord = { tilingX, tilingY };
    primitive.vertices[2].normal = { 0.0f, 0.0f, -1.0f };

    primitive.vertices[3].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    primitive.vertices[3].texcoord = { 0.0f, tilingY };
    primitive.vertices[3].normal = { 0.0f, 0.0f, -1.0f };

    primitive.indices = { 0, 1, 2, 0, 2, 3 };

    modelData.primitives.push_back(primitive);
    modelData.material.textureFilePath = texturePath;
    SetupDefaultRootNode(modelData, "Plane");

    return modelData;
}
}

std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;


ModelManager* ModelManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<ModelManager>(ConstructorKey());
    }
    return instance_.get();
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
    instance_.reset();
}

Model* ModelManager::Load(const std::string& filepath)
{
    auto it = models_.find(filepath);
    if (it != models_.end()) {
        return it->second.get();
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), "resources/Models", filepath);

    Model* raw = model.get();
    models_.emplace(filepath, std::move(model));
    return raw;
}

Model* ModelManager::CreatePlane(const std::string& texturePath, float tilingX, float tilingY)
{
    std::string actualTexturePath = texturePath;
    if (actualTexturePath.empty()) {
        actualTexturePath = kDefaultPlaneTexture;
    }

    std::string key = BuildPlaneKey(actualTexturePath, tilingX, tilingY);
    auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second.get();
    }

    ModelData modelData = CreatePlaneModelData(actualTexturePath, tilingX, tilingY);
    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), modelData);

    Model* raw = model.get();
    models_.emplace(key, std::move(model));
    return raw;
}

ModelData CreateBeamCrossModelData(const std::string& texturePath)
{
    ModelData modelData {};
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices.resize(8);

    // 1枚目 (X-Z平面): 横に広がり、Z軸方向に伸びる
    // 頂点0 (左手前)
    primitive.vertices[0].position = { -0.5f, 0.0f, 0.0f, 1.0f };
    primitive.vertices[0].texcoord = { 0.0f, 0.0f };
    primitive.vertices[0].normal = { 0.0f, 1.0f, 0.0f };

    // 頂点1 (右手前)
    primitive.vertices[1].position = { 0.5f, 0.0f, 0.0f, 1.0f };
    primitive.vertices[1].texcoord = { 1.0f, 0.0f };
    primitive.vertices[1].normal = { 0.0f, 1.0f, 0.0f };

    // 頂点2 (右奥)
    primitive.vertices[2].position = { 0.5f, 0.0f, 1.0f, 1.0f };
    primitive.vertices[2].texcoord = { 1.0f, 1.0f };
    primitive.vertices[2].normal = { 0.0f, 1.0f, 0.0f };

    // 頂点3 (左奥)
    primitive.vertices[3].position = { -0.5f, 0.0f, 1.0f, 1.0f };
    primitive.vertices[3].texcoord = { 0.0f, 1.0f };
    primitive.vertices[3].normal = { 0.0f, 1.0f, 0.0f };

    // 2枚目 (Y-Z平面): 縦に広がり、Z軸方向に伸びる
    // 頂点4 (下手前)
    primitive.vertices[4].position = { 0.0f, -0.5f, 0.0f, 1.0f };
    primitive.vertices[4].texcoord = { 0.0f, 0.0f };
    primitive.vertices[4].normal = { 1.0f, 0.0f, 0.0f };

    // 頂点5 (上手前)
    primitive.vertices[5].position = { 0.0f, 0.5f, 0.0f, 1.0f };
    primitive.vertices[5].texcoord = { 1.0f, 0.0f };
    primitive.vertices[5].normal = { 1.0f, 0.0f, 0.0f };

    // 頂点6 (上奥)
    primitive.vertices[6].position = { 0.0f, 0.5f, 1.0f, 1.0f };
    primitive.vertices[6].texcoord = { 1.0f, 1.0f };
    primitive.vertices[6].normal = { 1.0f, 0.0f, 0.0f };

    // 頂点7 (下奥)
    primitive.vertices[7].position = { 0.0f, -0.5f, 1.0f, 1.0f };
    primitive.vertices[7].texcoord = { 0.0f, 1.0f };
    primitive.vertices[7].normal = { 1.0f, 0.0f, 0.0f };

    // インデックス設定
    primitive.indices = {
        0, 1, 2, 0, 2, 3, // 1枚目
        4, 5, 6, 4, 6, 7  // 2枚目
    };

    modelData.primitives.push_back(primitive);
    modelData.material.textureFilePath = texturePath;
    
    modelData.rootNode.name = "BeamCross";
    modelData.rootNode.transform.scale = { 1.0f, 1.0f, 1.0f };
    modelData.rootNode.transform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    modelData.rootNode.transform.translate = { 0.0f, 0.0f, 0.0f };
    modelData.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();

    return modelData;
}

Model* ModelManager::CreateBeamCross(const std::string& texturePath)
{
    std::string actualTexturePath = texturePath;
    if (actualTexturePath.empty()) {
        actualTexturePath = kDefaultPlaneTexture;
    }

    std::string key = "Primitive/BeamCross/" + actualTexturePath;
    auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second.get();
    }

    ModelData modelData = CreateBeamCrossModelData(actualTexturePath);
    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), modelData);

    Model* raw = model.get();
    models_.emplace(key, std::move(model));
    return raw;
}


Model* ModelManager::FindModel(const std::string& filepath)
{
    auto it = models_.find(filepath);
    if (it != models_.end()) {
        return it->second.get();
    }
    return nullptr;
}

ModelManager::ModelManager(ConstructorKey)
{
}
