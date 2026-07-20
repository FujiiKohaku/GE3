#include "SpriteMaterialManager.h"

#include "Engine/Logger/Logger.h"
#include "externals/json.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
BlendMode ParseBlendMode(const std::string& value)
{
    if (value == "None") {
        return kBlendModeNone;
    }
    if (value == "Add") {
        return kBlendModeAdd;
    }
    if (value == "Subtract") {
        return kBlendModeSubtract;
    }
    if (value == "Multiply") {
        return kBlendModeMultiply;
    }
    if (value == "Screen") {
        return kBlendModeScreen;
    }
    return kBlendModeNormal;
}

D3D12_CULL_MODE ParseCullMode(const std::string& value)
{
    if (value == "Front") {
        return D3D12_CULL_MODE_FRONT;
    }
    if (value == "Back") {
        return D3D12_CULL_MODE_BACK;
    }
    return D3D12_CULL_MODE_NONE;
}

Vector2 ReadVector2(const nlohmann::json& value, const Vector2& fallback)
{
    if (!value.is_array() || value.size() < 2) {
        return fallback;
    }

    Vector2 result;
    result.x = value.at(0).get<float>();
    result.y = value.at(1).get<float>();
    return result;
}
}

const SpriteMaterialConfig& SpriteMaterialManager::GetOrLoadMaterial(
    const std::string& shaderFolderPath)
{
    const std::string cacheKey = MakeCacheKey(shaderFolderPath);
    std::unordered_map<std::string, SpriteMaterialConfig>::iterator iterator =
        materialCache_.find(cacheKey);
    if (iterator != materialCache_.end()) {
        return iterator->second;
    }

    SpriteMaterialConfig config = LoadMaterial(shaderFolderPath);
    std::pair<std::unordered_map<std::string, SpriteMaterialConfig>::iterator, bool> result =
        materialCache_.emplace(cacheKey, std::move(config));
    return result.first->second;
}

SpriteMaterialConfig SpriteMaterialManager::LoadMaterial(
    const std::string& shaderFolderPath) const
{
    const std::filesystem::path folderPath =
        std::filesystem::path(shaderFolderPath).lexically_normal();
    const std::filesystem::path materialPath = folderPath / "Material.json";
    const std::filesystem::path vertexShaderPath = folderPath / "Render.VS.hlsl";
    const std::filesystem::path pixelShaderPath = folderPath / "Render.PS.hlsl";

    if (!std::filesystem::is_regular_file(materialPath)) {
        const std::string message =
            "Sprite material file is missing: " + materialPath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }
    if (!std::filesystem::is_regular_file(vertexShaderPath)) {
        const std::string message =
            "Sprite vertex shader is missing: " + vertexShaderPath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }
    if (!std::filesystem::is_regular_file(pixelShaderPath)) {
        const std::string message =
            "Sprite pixel shader is missing: " + pixelShaderPath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    std::ifstream file(materialPath);
    if (!file.is_open()) {
        const std::string message =
            "Failed to open sprite material: " + materialPath.generic_string();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    SpriteMaterialConfig config;
    config.name = folderPath.filename().string();
    config.pipelineDesc.vertexShaderPath = vertexShaderPath.generic_string();
    config.pipelineDesc.pixelShaderPath = pixelShaderPath.generic_string();

    try {
        nlohmann::json root;
        file >> root;

        config.name = root.value("Name", config.name);

        if (root.contains("Pipeline")) {
            const nlohmann::json& pipeline = root.at("Pipeline");
            config.pipelineDesc.blendMode =
                ParseBlendMode(pipeline.value("BlendMode", std::string("Normal")));
            config.pipelineDesc.depthTest =
                pipeline.value("DepthTest", config.pipelineDesc.depthTest);
            config.pipelineDesc.depthWrite =
                pipeline.value("DepthWrite", config.pipelineDesc.depthWrite);
            config.pipelineDesc.cullMode =
                ParseCullMode(pipeline.value("CullMode", std::string("None")));
        }

        if (root.contains("Mesh")) {
            const nlohmann::json& mesh = root.at("Mesh");
            config.meshDesc.divisionX = mesh.value("DivisionX", config.meshDesc.divisionX);
            config.meshDesc.divisionY = mesh.value("DivisionY", config.meshDesc.divisionY);
        }

        if (root.contains("Effect")) {
            const nlohmann::json& effect = root.at("Effect");
            config.effectParameters.amplitude =
                effect.value("Amplitude", config.effectParameters.amplitude);
            config.effectParameters.frequency =
                effect.value("Frequency", config.effectParameters.frequency);
            config.effectParameters.speed =
                effect.value("Speed", config.effectParameters.speed);
            config.effectParameters.phase =
                effect.value("Phase", config.effectParameters.phase);
            config.effectParameters.strength =
                effect.value("Strength", config.effectParameters.strength);
            config.effectParameters.threshold =
                effect.value("Threshold", config.effectParameters.threshold);
            if (effect.contains("Direction")) {
                config.effectParameters.direction = ReadVector2(
                    effect.at("Direction"),
                    config.effectParameters.direction);
            }
        }
    } catch (const nlohmann::json::exception& exception) {
        const std::string message =
            "Invalid sprite material JSON: " + materialPath.generic_string() +
            " Error: " + exception.what();
        Logger::Error(message);
        throw std::runtime_error(message);
    }

    if (config.meshDesc.divisionX == 0) {
        Logger::Warning(
            "Sprite material DivisionX was zero. Using 1: " +
            materialPath.generic_string());
        config.meshDesc.divisionX = 1;
    }
    if (config.meshDesc.divisionY == 0) {
        Logger::Warning(
            "Sprite material DivisionY was zero. Using 1: " +
            materialPath.generic_string());
        config.meshDesc.divisionY = 1;
    }

    return config;
}

std::string SpriteMaterialManager::MakeCacheKey(const std::string& shaderFolderPath) const
{
    std::string cacheKey =
        std::filesystem::path(shaderFolderPath).lexically_normal().generic_string();
    for (char& character : cacheKey) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return cacheKey;
}
