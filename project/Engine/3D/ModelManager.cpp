#include "ModelManager.h"
#include "Engine/Animation/Event/AnimationEventLoader.h"
#include "Engine/math/MatrixMath.h"
#include <cmath>
#include <cassert>
#include <filesystem>
#include <numbers>
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
    MaterialData material {};
    material.textureFilePath = texturePath;
    modelData.materials.push_back(material);
    SetupDefaultRootNode(modelData, "Plane");

    return modelData;
}

ModelData CreateCubeModelData(const std::string& texturePath)
{
    ModelData modelData {};
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;
    primitive.vertices = {
        { { -0.5f,  0.5f, -0.5f, 1.0f }, { 0.0f, 0.0f }, {  0.0f,  0.0f, -1.0f } },
        { {  0.5f,  0.5f, -0.5f, 1.0f }, { 1.0f, 0.0f }, {  0.0f,  0.0f, -1.0f } },
        { {  0.5f, -0.5f, -0.5f, 1.0f }, { 1.0f, 1.0f }, {  0.0f,  0.0f, -1.0f } },
        { { -0.5f, -0.5f, -0.5f, 1.0f }, { 0.0f, 1.0f }, {  0.0f,  0.0f, -1.0f } },

        { {  0.5f,  0.5f,  0.5f, 1.0f }, { 0.0f, 0.0f }, {  0.0f,  0.0f,  1.0f } },
        { { -0.5f,  0.5f,  0.5f, 1.0f }, { 1.0f, 0.0f }, {  0.0f,  0.0f,  1.0f } },
        { { -0.5f, -0.5f,  0.5f, 1.0f }, { 1.0f, 1.0f }, {  0.0f,  0.0f,  1.0f } },
        { {  0.5f, -0.5f,  0.5f, 1.0f }, { 0.0f, 1.0f }, {  0.0f,  0.0f,  1.0f } },

        { { -0.5f,  0.5f,  0.5f, 1.0f }, { 0.0f, 0.0f }, { -1.0f,  0.0f,  0.0f } },
        { { -0.5f,  0.5f, -0.5f, 1.0f }, { 1.0f, 0.0f }, { -1.0f,  0.0f,  0.0f } },
        { { -0.5f, -0.5f, -0.5f, 1.0f }, { 1.0f, 1.0f }, { -1.0f,  0.0f,  0.0f } },
        { { -0.5f, -0.5f,  0.5f, 1.0f }, { 0.0f, 1.0f }, { -1.0f,  0.0f,  0.0f } },

        { {  0.5f,  0.5f, -0.5f, 1.0f }, { 0.0f, 0.0f }, {  1.0f,  0.0f,  0.0f } },
        { {  0.5f,  0.5f,  0.5f, 1.0f }, { 1.0f, 0.0f }, {  1.0f,  0.0f,  0.0f } },
        { {  0.5f, -0.5f,  0.5f, 1.0f }, { 1.0f, 1.0f }, {  1.0f,  0.0f,  0.0f } },
        { {  0.5f, -0.5f, -0.5f, 1.0f }, { 0.0f, 1.0f }, {  1.0f,  0.0f,  0.0f } },

        { { -0.5f,  0.5f,  0.5f, 1.0f }, { 0.0f, 0.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f,  0.5f, 1.0f }, { 1.0f, 0.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f, -0.5f, 1.0f }, { 1.0f, 1.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f,  0.5f, -0.5f, 1.0f }, { 0.0f, 1.0f }, {  0.0f,  1.0f,  0.0f } },

        { { -0.5f, -0.5f, -0.5f, 1.0f }, { 0.0f, 0.0f }, {  0.0f, -1.0f,  0.0f } },
        { {  0.5f, -0.5f, -0.5f, 1.0f }, { 1.0f, 0.0f }, {  0.0f, -1.0f,  0.0f } },
        { {  0.5f, -0.5f,  0.5f, 1.0f }, { 1.0f, 1.0f }, {  0.0f, -1.0f,  0.0f } },
        { { -0.5f, -0.5f,  0.5f, 1.0f }, { 0.0f, 1.0f }, {  0.0f, -1.0f,  0.0f } }
    };
    primitive.indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };

    modelData.primitives.push_back(std::move(primitive));
    modelData.materials.push_back({ texturePath });
    SetupDefaultRootNode(modelData, "Cube");
    return modelData;
}

ModelData CreateCylinderModelData(const std::string& texturePath, uint32_t divisions)
{
    if (divisions < 3u) {
        divisions = 3u;
    }

    ModelData modelData {};
    MeshPrimitive primitive {};
    primitive.mode = PrimitiveMode::Triangles;

    const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divisions);
    for (uint32_t index = 0; index <= divisions; ++index) {
        const float angle = static_cast<float>(index) * angleStep;
        const float x = std::sin(angle);
        const float z = std::cos(angle);
        const float u = static_cast<float>(index) / static_cast<float>(divisions);

        primitive.vertices.push_back({ { x, 1.0f, z, 1.0f }, { u, 0.0f }, { x, 0.0f, z } });
        primitive.vertices.push_back({ { x, 0.0f, z, 1.0f }, { u, 1.0f }, { x, 0.0f, z } });
    }

    for (uint32_t index = 0; index < divisions; ++index) {
        const uint32_t top0 = index * 2;
        const uint32_t bottom0 = top0 + 1;
        const uint32_t top1 = top0 + 2;
        const uint32_t bottom1 = top0 + 3;
        primitive.indices.insert(primitive.indices.end(), {
            top0, top1, bottom0,
            bottom0, top1, bottom1
        });
    }

    modelData.primitives.push_back(std::move(primitive));
    modelData.materials.push_back({ texturePath });
    SetupDefaultRootNode(modelData, "Cylinder");
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
#ifdef _DEBUG
    const std::filesystem::path modelPath =
        std::filesystem::path("resources/Models") / filepath;
    AnimationEventLoader::EnsureEventFilesForModel(modelPath);
#endif
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

Model* ModelManager::CreateBookLeaf(
    const std::string& frontTexturePath,
    const std::string& backTexturePath,
    uint32_t stripIndex,
    uint32_t stripCount)
{
    assert(stripCount > 0 && stripIndex < stripCount);
    const std::string key = "BookLeaf/" + frontTexturePath + "/" + backTexturePath +
        "/" + std::to_string(stripIndex) + "/" + std::to_string(stripCount);
    if (auto it = models_.find(key); it != models_.end()) {
        return it->second.get();
    }

    ModelData data = CreateCubeModelData(frontTexturePath);
    const MeshPrimitive source = data.primitives[0];

    MeshPrimitive frontPrimitive = source;
    frontPrimitive.materialIndex = 0;
    frontPrimitive.indices = {
        0, 1, 2, 0, 2, 3,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };
    for (VertexData& vertex : frontPrimitive.vertices) {
        vertex.texcoord.x =
            (static_cast<float>(stripIndex) + vertex.texcoord.x) /
            static_cast<float>(stripCount);
    }

    MeshPrimitive backPrimitive = source;
    backPrimitive.materialIndex = 1;
    backPrimitive.indices = { 4, 5, 6, 4, 6, 7 };
    const uint32_t backStripIndex = stripCount - 1 - stripIndex;
    for (VertexData& vertex : backPrimitive.vertices) {
        vertex.texcoord.x =
            (static_cast<float>(backStripIndex) + vertex.texcoord.x) /
            static_cast<float>(stripCount);
    }

    data.primitives.clear();
    data.primitives.push_back(std::move(frontPrimitive));
    data.primitives.push_back(std::move(backPrimitive));
    data.materials.clear();
    data.materials.push_back({ frontTexturePath });
    data.materials.push_back({ backTexturePath });

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), data);
    Model* result = model.get();
    models_.emplace(key, std::move(model));
    return result;
}

Model* ModelManager::CreateCube(const std::string& texturePath)
{
    std::string actualTexturePath = texturePath;
    if (actualTexturePath.empty()) {
        actualTexturePath = kDefaultPlaneTexture;
    }

    const std::string key = "Primitive/Cube/" + actualTexturePath;
    const auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second.get();
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_.get(),
        CreateCubeModelData(actualTexturePath));

    Model* raw = model.get();
    models_.emplace(key, std::move(model));
    return raw;
}

Model* ModelManager::CreateCylinder(const std::string& texturePath, uint32_t divisions)
{
    const std::string actualTexturePath = texturePath.empty() ? kDefaultPlaneTexture : texturePath;
    const std::string key = "Primitive/Cylinder/" + actualTexturePath + "_" + std::to_string(divisions);
    const auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second.get();
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_.get(), CreateCylinderModelData(actualTexturePath, divisions));

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
    MaterialData material {};
    material.textureFilePath = texturePath;
    modelData.materials.push_back(material);
    
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
