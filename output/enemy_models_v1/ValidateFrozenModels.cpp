#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path directory = "project/resources/Models/Enemy/Frozen";
    const char* modelNames[] = { "NormalEnemy", "MoveEnemy", "ArmoredEnemy", "PaintShooterEnemy", "SwarmEnemy" };
    for (const char* modelName : modelNames) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile((directory / (std::string(modelName) + ".obj")).string(),
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
        if (scene == nullptr || !scene->HasMeshes()) {
            std::cerr << importer.GetErrorString();
            return 1;
        }
        unsigned int triangleCount = 0;
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            if (!mesh->HasNormals() || mesh->mNumVertices == 0) {
                return 2;
            }
            triangleCount += mesh->mNumFaces;
            const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString texturePath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS ||
                !std::filesystem::is_regular_file(directory / texturePath.C_Str())) {
                return 3;
            }
        }
        std::cout << modelName << ": " << triangleCount << " triangles, " << scene->mNumMeshes << " material meshes\n";
    }
}
