#pragma once

#include "Engine/math/SpriteStruct.h"
#include "SpriteMeshManager.h"
#include "SpriteRenderManager.h"
#include <string>
#include <unordered_map>

struct SpriteMaterialConfig {
    std::string name;
    SpriteGraphicsPipelineDesc pipelineDesc;
    SpriteMeshDesc meshDesc;
    SpriteEffectParameters effectParameters = {
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        { 1.0f, 0.0f },
        1.0f,
        0.5f,
        { 0.0f, 0.0f },
        { 0.0f, 0.0f }
    };
};

class SpriteMaterialManager {
public:
    const SpriteMaterialConfig& GetOrLoadMaterial(const std::string& shaderFolderPath);

private:
    SpriteMaterialConfig LoadMaterial(const std::string& shaderFolderPath) const;
    std::string MakeCacheKey(const std::string& shaderFolderPath) const;

private:
    std::unordered_map<std::string, SpriteMaterialConfig> materialCache_;
};
