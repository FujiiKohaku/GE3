#include "ModelPreviewApp.h"
#include "App/Game/VisualPresetLibrary.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/TextureManager/TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
constexpr const char* kDefaultVertexShader = "resources/Shaders/Object3D/Object3d.VS.hlsl";
}

void ModelPreviewApp::ScanModels()
{
    modelPaths_.clear();
    for (const auto& entry : std::filesystem::recursive_directory_iterator("resources/Models")) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string extension = entry.path().extension().string();
        for (char& character : extension) {
            if (character >= 'A' && character <= 'Z') {
                character += 'a' - 'A';
            }
        }
        if (extension == ".obj" || extension == ".gltf" || extension == ".glb" ||
            extension == ".fbx" || extension == ".dae" || extension == ".ply") {
            modelPaths_.push_back(entry.path().generic_string());
        }
    }
    std::sort(modelPaths_.begin(), modelPaths_.end());
}

void ModelPreviewApp::Import(const std::filesystem::path& path, PreviewItem& item)
{
    Assimp::Importer importer;
    // Bake the entire scene hierarchy, so a multi-mesh model is previewed as a whole.
    const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate |
        aiProcess_GenSmoothNormals | aiProcess_PreTransformVertices |
        aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
    if (scene == nullptr || !scene->HasMeshes()) {
        throw std::runtime_error(std::string("Model import failed: ") + importer.GetErrorString());
    }
    ModelData data {};
    data.rootNode.localMatrix = MatrixMath::MakeIdentity4x4();
    data.rootNode.transform.scale = { 1.0f, 1.0f, 1.0f };
    data.rootNode.transform.rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    data.rootNode.name = "PreviewRoot";
    data.materials.resize((std::max)(scene->mNumMaterials, 1u));
    for (unsigned int index = 0; index < data.materials.size(); ++index) {
        auto& material = data.materials[index];
        material.textureFilePath = "resources/Textures/white.png";
        if (index >= scene->mNumMaterials) {
            continue;
        }
        aiString textureName;
        auto* source = scene->mMaterials[index];
        aiTextureType type = aiTextureType_BASE_COLOR;
        if (source->GetTextureCount(type) == 0) {
            type = aiTextureType_DIFFUSE;
        }
        if (source->GetTexture(type, 0, &textureName) != AI_SUCCESS) {
            continue;
        }
        const aiTexture* embedded = scene->GetEmbeddedTexture(textureName.C_Str());
        if (embedded != nullptr) {
            std::string key = "embedded://preview/" + path.generic_string() + "/" + textureName.C_Str();
            if (embedded->mHeight == 0) {
                TextureManager::GetInstance()->LoadTextureFromMemory(key,
                    reinterpret_cast<const std::uint8_t*>(embedded->pcData), embedded->mWidth);
            } else {
                TextureManager::GetInstance()->LoadTextureFromBGRA(key,
                    reinterpret_cast<const std::uint8_t*>(embedded->pcData),
                    embedded->mWidth, embedded->mHeight);
            }
            material.textureFilePath = key;
        } else {
            auto texturePath = (path.parent_path() / textureName.C_Str()).lexically_normal();
            if (std::filesystem::is_regular_file(texturePath)) {
                material.textureFilePath = texturePath.string();
            }
        }
    }
    float infinity = std::numeric_limits<float>::infinity();
    Vector3 minimum { infinity, infinity, infinity };
    Vector3 maximum { -infinity, -infinity, -infinity };
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const auto* mesh = scene->mMeshes[meshIndex];
        if (mesh->mNumVertices == 0 || !(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) {
            continue;
        }
        MeshPrimitive primitive {};
        primitive.mode = PrimitiveMode::Triangles;
        primitive.materialIndex = mesh->mMaterialIndex;
        for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            auto position = mesh->mVertices[vertexIndex];
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                throw std::runtime_error("Model contains non-finite vertex positions");
            }
            VertexData vertex {};
            vertex.position = { -position.x, position.y, position.z, 1.0f };
            if (mesh->HasNormals()) {
                const auto& normal = mesh->mNormals[vertexIndex];
                vertex.normal = { -normal.x, normal.y, normal.z };
            } else {
                vertex.normal = { 0.0f, 1.0f, 0.0f };
            }
            if (mesh->HasTextureCoords(0)) {
                const auto& uv = mesh->mTextureCoords[0][vertexIndex];
                vertex.texcoord = { uv.x, uv.y };
            }
            primitive.vertices.push_back(vertex);
            minimum.x = (std::min)(minimum.x, vertex.position.x);
            minimum.y = (std::min)(minimum.y, vertex.position.y);
            minimum.z = (std::min)(minimum.z, vertex.position.z);
            maximum.x = (std::max)(maximum.x, vertex.position.x);
            maximum.y = (std::max)(maximum.y, vertex.position.y);
            maximum.z = (std::max)(maximum.z, vertex.position.z);
        }
        for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const auto& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices == 3) {
                primitive.indices.insert(primitive.indices.end(), face.mIndices, face.mIndices + 3);
            }
        }
        if (!primitive.indices.empty()) {
            data.primitives.push_back(std::move(primitive));
        }
    }
    if (data.primitives.empty()) {
        throw std::runtime_error("This model has no triangle meshes");
    }
    item.center = (minimum + maximum) * 0.5f;
    Vector3 extent = maximum - minimum;
    item.radius = (std::max)(0.01f, std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z) * 0.5f);
    item.model = std::make_unique<Model>();
    item.model->Initialize(&modelCommon_, data);
}

void ModelPreviewApp::ApplyMaterial(PreviewItem& item)
{
    item.object->SetMaterial("resources/Shaders/Object3D/" + item.shader);
    item.object->SetVertexShaderPath(kDefaultVertexShader);
    if (item.shader == "IceJellyfish") {
        item.object->SetVertexShaderPath("resources/Shaders/Object3D/IceJellyfish/Render.VS.hlsl");
    }
    item.object->SetColor(item.color);
    item.object->SetTransparent(item.transparent);
    item.object->SetEnableEnvironmentMap(item.environment);
    item.object->SetEnvironmentMapStrength(item.reflection);
    item.object->GetMaterial()->shininess = item.shininess;
}

void ModelPreviewApp::Load(const std::filesystem::path& path, bool append)
{
    try {
        if (capture_.IsRecording()) {
            throw std::runtime_error("Stop GIF recording before changing models");
        }
        dx_->WaitForGPU();
        auto item = std::make_unique<PreviewItem>();
        item->name = path.filename().string();
        Import(std::filesystem::absolute(path), *item);
        item->object = std::make_unique<Object3d>();
        item->object->Initialize(Object3dManager::GetInstance());
        item->object->SetModel(item->model.get());
        item->object->SetCamera(camera_.get());
        std::string name = path.filename().string();
        if (path.generic_string().find("Environment/Ice/") != std::string::npos || name == "IceSpike.obj") {
            std::string gameModelPath = "Environment/Ice/" + name;
            if (path.parent_path().filename() != "Ice" && name == "IceSpike.obj") {
                gameModelPath = name;
            }
            item->shader = std::filesystem::path(
                VisualPresetLibrary::GetInstance().GetMaterial(gameModelPath)).filename().string();
            item->color = { 0.82f, 0.94f, 1.0f, 1.0f };
            item->shininess = VisualPresetLibrary::GetInstance().GetShininess("ice");
            item->environment = false;
            item->reflection = 0.0f;
            // Match the scale and placement used by the game.
            for (const auto& placement : iceLayout_.objects) {
                if (placement.fileName == gameModelPath && !placement.disabled) {
                    item->position = placement.translation;
                    item->rotation = placement.rotation;
                    item->axisScale = placement.scale;
                    item->stageTransform = true;
                    break;
                }
            }
        } else if (name.find("IceJellyfishBell") != std::string::npos ||
            name.find("IceJellyfishSegment") != std::string::npos ||
            name.find("IceJellyfishTip") != std::string::npos) {
            item->shader = "IceJellyfish";
            item->color = { 0.65f, 0.86f, 1.0f, 0.95f };
            item->shininess = 96.0f;
            item->environment = true;
            item->reflection = 0.10f;
            item->transparent = true;
        }
        ApplyMaterial(*item);
        // Validate/compile the pipeline before replacing the current model.
        Object3dManager::GetInstance()->BindPipeline(item->object->GetPixelShaderPath(),
            item->transparent, false, item->object->GetVertexShaderPath());
        if (!append) {
            items_.clear();
        }
        items_.push_back(std::move(item));
        selected_ = static_cast<int>(items_.size()) - 1;
        Fit();
        error_.clear();
        Logger::Log("Preview model loaded: " + path.generic_string());
    } catch (const std::exception& error) {
        error_ = error.what();
        Logger::Error(error_);
    }
}
