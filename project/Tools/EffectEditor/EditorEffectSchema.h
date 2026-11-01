#pragma once
#include "Engine/Serialization/JsonFile.h"
#include <filesystem>
#include <string>

// Editor-only contract. It never removes values from the engine's Effect.json.
class EditorEffectSchema {
public:
    void Load(const std::filesystem::path& path)
    {
        definition_ = nlohmann::json::object();
        error_.clear();
        try {
            if (!std::filesystem::exists(path)) {
                error_ = "Editor.jsonがありません。対応未確認の項目は非表示です。";
                return;
            }
            auto definition = JsonFile::Load(path);
            if (!definition.is_object() || definition.at("Version") != 1 ||
                !definition.at("Shaders").is_object() || !definition.at("Properties").is_object()) {
                throw std::runtime_error("Version・Shaders・Propertiesの形式を確認してください");
            }
            const auto& shaders = definition.at("Shaders");
            for (const char* stage : { "Emit", "Update", "Vertex", "Pixel" }) {
                if (!shaders.contains(stage) || !shaders.at(stage).is_string()) {
                    throw std::runtime_error("ShadersにはEmit・Update・Vertex・Pixelが必要です");
                }
            }
            const std::string renderType = definition.at("RenderType").get<std::string>();
            if (renderType != "Mesh" && renderType != "Trail") { throw std::runtime_error("RenderTypeが不正です"); }
            for (const auto& item : definition.at("Properties").items()) {
                const auto separator = item.key().find('.');
                if (separator == std::string::npos || separator == 0 || separator + 1 == item.key().size() ||
                    item.key().find('.', separator + 1) != std::string::npos || !item.value().is_object()) {
                    throw std::runtime_error("PropertiesはSection.Key形式のオブジェクトで指定してください");
                }
                const std::string section = item.key().substr(0, separator);
                if (section != "Emitter" && section != "Particle" && section != "Simulation" && section != "Trail" && section != "Render") {
                    throw std::runtime_error("未対応のPropertiesセクションです");
                }
                for (const char* field : { "Label", "Description" }) {
                    if (item.value().contains(field) && !item.value().at(field).is_string()) {
                        throw std::runtime_error("Label・Descriptionは文字列で指定してください");
                    }
                }
            }
            definition_ = std::move(definition);
        } catch (const std::exception& exception) {
            error_ = std::string("Editor.jsonの読み込みに失敗：") + exception.what();
        }
    }

    bool Matches(const nlohmann::json& config) const
    {
        if (definition_.empty() || !config.contains("Shaders") || !config.contains("Render")) { return false; }
        return definition_.at("Shaders") == config.at("Shaders") &&
            definition_.at("RenderType") == config.at("Render").value("Type", std::string("Mesh"));
    }

    bool IsSupported(const nlohmann::json& config, const std::string& section, const std::string& key) const
    {
        // These settings are consumed by the engine independently of the custom shaders.
        if (section == "Emitter" && (key == "Frequency" || key == "EmitFrequency")) { return true; }
        if (section == "Particle" && key == "MaxParticles") {
            return config.at("Render").value("Type", std::string("Mesh")) == "Mesh";
        }
        if (section == "Render") {
            if (key == "Type" || key == "BlendMode" || key == "CullMode" || key == "DepthTest" ||
                key == "DepthWrite" || key == "Loop" || key == "Duration") { return true; }
            if (key == "MeshType" && config.at("Render").value("Type", std::string("Mesh")) == "Mesh") { return true; }
        }
        return Matches(config) && definition_.at("Properties").contains(section + "." + key);
    }

    nlohmann::json Filter(const nlohmann::json& config, const std::string& section, const nlohmann::json& values) const
    {
        auto visible = nlohmann::json::object();
        for (const auto& item : values.items()) {
            if (IsSupported(config, section, item.key())) { visible[item.key()] = item.value(); }
        }
        return visible;
    }

    std::string GetText(const nlohmann::json& config, const std::string& section, const std::string& key, const char* field) const
    {
        if (!Matches(config)) { return {}; }
        const auto property = definition_.at("Properties").find(section + "." + key);
        if (property == definition_.at("Properties").end()) { return {}; }
        return property->value(field, std::string());
    }

    std::string GetWarning(const nlohmann::json& config) const
    {
        if (!error_.empty()) { return error_; }
        if (!Matches(config)) { return "シェーダーまたは種類が対応定義と異なります。対応未確認の項目は非表示です。"; }
        return {};
    }

private:
    nlohmann::json definition_ = nlohmann::json::object();
    std::string error_;
};
