#pragma once
#include "Engine/Light/LightManager.h"
#include "App/Scene/Common/SceneManager.h"
#include "externals/json.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

// Asset identifiers and preset selection belong to application data, not the renderer.
class VisualPresetLibrary {
public:
    static VisualPresetLibrary& GetInstance()
    {
        static VisualPresetLibrary library;
        return library;
    }
    const LightingPreset& GetLighting(const std::string& presetId) const
    {
        const auto preset = lightingPresets_.find(presetId);
        if (preset == lightingPresets_.end()) { return lightingPresets_.at("default"); }
        return preset->second;
    }
    const AtmospherePreset& GetAtmosphere(const std::string& presetId) const
    {
        const auto preset = atmospherePresets_.find(presetId);
        if (preset == atmospherePresets_.end()) { return atmospherePresets_.at("default"); }
        return preset->second;
    }
    const std::string& GetMaterial(const std::string& modelPath) const
    {
        const auto material = materials_.find(modelPath);
        if (material == materials_.end()) { return defaultMaterial_; }
        return material->second;
    }
    const std::vector<PostEffectInfo>& GetPostEffects(const std::string& presetId) const
    {
        return effectChains_.at(presetId);
    }
    float GetShininess(const std::string& presetId) const { return shininess_.at(presetId); }

private:
    VisualPresetLibrary()
    {
        std::ifstream file("resources/Graphics/visual-presets.json");
        if (!file) { throw std::runtime_error("Cannot open graphics preset data"); }
        const nlohmann::json data = nlohmann::json::parse(file);
        for (const auto& entry : data.at("lighting").items()) {
            const auto& value = entry.value();
            LightingPreset preset;
            preset.color = ReadVector4(value.at("color"));
            preset.direction = ReadVector3(value.at("direction"));
            preset.intensity = value.at("intensity").get<float>();
            preset.ambient = ReadVector4(value.at("ambient"));
            preset.skyColor = ReadVector3(value.at("skyColor"));
            preset.groundColor = ReadVector3(value.at("groundColor"));
            lightingPresets_.emplace(entry.key(), preset);
        }
        for (const auto& entry : data.at("atmosphere").items()) {
            const auto& value = entry.value();
            AtmospherePreset preset;
            preset.exposure = value.at("exposure").get<float>();
            const auto& fog = value.at("distanceFog");
            preset.distanceFog = { fog.at(0).get<float>(), fog.at(1).get<float>(),
                fog.at(2).get<float>(), fog.at(3).get<float>() };
            if (value.contains("fogColor")) {
                preset.shouldApplyFogColor = true;
                preset.fogColor = ReadVector4(value.at("fogColor"));
            }
            atmospherePresets_.emplace(entry.key(), preset);
        }
        defaultMaterial_ = data.at("defaultMaterial").get<std::string>();
        for (const auto& entry : data.at("materials").items()) {
            materials_.emplace(entry.key(), entry.value().get<std::string>());
        }
        for (const auto& entry : data.at("shininess").items()) {
            shininess_.emplace(entry.key(), entry.value().get<float>());
        }
        for (const auto& entry : data.at("effectChains").items()) {
            std::vector<PostEffectInfo> effects;
            for (const auto& value : entry.value()) {
                PostEffectInfo effect;
                bool isFound = false;
                const std::string effectName = value.get<std::string>();
                for (int typeIndex = 0; typeIndex <= static_cast<int>(PostEffectType::ScreenLighting); ++typeIndex) {
                    const auto type = static_cast<PostEffectType>(typeIndex);
                    if (effectName == GetPostEffectTypeName(type)) { effect.type = type; isFound = true; break; }
                }
                if (!isFound) { throw std::runtime_error("Unknown effect in graphics preset: " + effectName); }
                effects.push_back(effect);
            }
            effectChains_.emplace(entry.key(), std::move(effects));
        }
    }
    static Vector3 ReadVector3(const nlohmann::json& value)
    {
        return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
    }
    static Vector4 ReadVector4(const nlohmann::json& value)
    {
        return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>() };
    }
    std::unordered_map<std::string, LightingPreset> lightingPresets_;
    std::unordered_map<std::string, AtmospherePreset> atmospherePresets_;
    std::unordered_map<std::string, std::string> materials_;
    std::unordered_map<std::string, std::vector<PostEffectInfo>> effectChains_;
    std::unordered_map<std::string, float> shininess_;
    std::string defaultMaterial_;
};
