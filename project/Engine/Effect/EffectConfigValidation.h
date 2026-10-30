#pragma once
#include "EffectParticleLimits.h"
#include "externals/json.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace EffectConfigValidation {
inline void NormalizeRenderType(nlohmann::json& config)
{
    if (!config.is_object()) { throw std::runtime_error("Effect config must be an object"); }
    if (!config.contains("Render")) {
        nlohmann::json render = nlohmann::json::object();
        // Keep the rendering values supported by the legacy flat JSON format.
        for (const char* key : { "Type", "Texture", "MeshType", "BlendMode", "DepthTest", "DepthWrite",
            "CullMode", "Loop", "Duration", "RenderParameter", "DissolveThreshold", "EmissionStrength",
            "UvScrollSpeedX", "UvScrollSpeedY", "UVScrollSpeedX", "UVScrollSpeedY", "UvScrollSpeed", "UVScrollSpeed" }) {
            if (config.contains(key)) { render[key] = config.at(key); }
        }
        config["Render"] = std::move(render);
    }
    auto& render = config.at("Render");
    if (!render.is_object()) { throw std::runtime_error("Render must be an object"); }
    if (!render.contains("Type")) { render["Type"] = "Mesh"; }
    const auto& type = render.at("Type");
    if (type != "Mesh" && type != "Trail") { throw std::runtime_error("Render.Type must be Mesh or Trail"); }
}

inline void ValidateEmitCount(const nlohmann::json& value)
{
    if (!value.is_number_integer() || value.get<double>() < 0.0 ||
        value.get<double>() > EffectParticleLimits::kMaxEmitCount) {
        throw std::runtime_error("Count/EmitCount must be an integer between 0 and " +
            std::to_string(EffectParticleLimits::kMaxEmitCount));
    }
}

inline void ValidateNumbers(const nlohmann::json& value)
{
    if (value.is_number() && (!std::isfinite(value.get<double>()) ||
        std::abs(value.get<double>()) > (std::numeric_limits<float>::max)())) {
        throw std::runtime_error("Effect config contains a non-finite number");
    }
    if (value.is_structured()) {
        for (const auto& child : value) { ValidateNumbers(child); }
    }
}

inline void ValidateChoice(const nlohmann::json& section, const char* key, std::initializer_list<const char*> choices)
{
    if (!section.contains(key)) { return; }
    const auto& value = section.at(key);
    for (const char* choice : choices) {
        if (value == choice) { return; }
    }
    throw std::runtime_error(std::string(key) + ": unsupported value");
}

inline void ValidateSection(const nlohmann::json& section)
{
    if (!section.is_object()) { throw std::runtime_error("Effect config section must be an object"); }
    for (const auto& entry : section.items()) {
        const std::string& key = entry.key();
        const auto& value = entry.value();
        if (key == "Count" || key == "EmitCount") { ValidateEmitCount(value); }
        size_t vectorSize = 0;
        if (key == "Velocity" || key == "Position" || key == "Direction" || key == "Offset") { vectorSize = 3; }
        if (key == "StartColor" || key == "EndColor" || key == "Color") { vectorSize = 4; }
        if (key == "UvScrollSpeed" || key == "UVScrollSpeed") { vectorSize = 2; }
        if (vectorSize != 0) {
            if (!value.is_array() || value.size() != vectorSize) { throw std::runtime_error(key + ": invalid vector size"); }
            for (const auto& component : value) {
                if (!component.is_number()) { throw std::runtime_error(key + ": vector components must be numeric"); }
            }
        }
        if (key == "Count" || key == "EmitCount" || key == "MaxParticles" || key == "MaxPoints" || key == "ResourcePoolReserve") {
            if (!value.is_number_integer() || value.get<double>() < 0.0 || value.get<double>() > 4294967295.0) {
                throw std::runtime_error(key + ": expected a non-negative 32-bit integer");
            }
            if (key == "MaxParticles" && (value.get<double>() < 1.0 || value.get<double>() > EffectParticleLimits::kMaxParticleCapacity)) {
                throw std::runtime_error("MaxParticles must be between 1 and " + std::to_string(EffectParticleLimits::kMaxParticleCapacity));
            }
            if (key == "MaxPoints" && (value.get<double>() < 2.0 || value.get<double>() > 64.0)) {
                throw std::runtime_error("MaxPoints must be between 2 and 64");
            }
        }
        if (key == "LifeTime" || key == "Frequency" || key == "EmitFrequency") {
            if (!value.is_number() || value.get<double>() <= 0.0) { throw std::runtime_error(key + " must be positive"); }
        }
        if (key == "Radius" || key == "EmitRadius" || key == "StartScale" || key == "EndScale" || key == "StartWidth" || key == "EndWidth") {
            if (!value.is_number() || value.get<double>() < 0.0) { throw std::runtime_error(key + " must be non-negative"); }
        }
    }
}

inline void Validate(const nlohmann::json& config)
{
    ValidateSection(config);
    ValidateNumbers(config);
    for (const char* sectionName : { "Emitter", "Particle", "Simulation", "Render", "RenderParameter", "Trail", "Light", "Shaders" }) {
        if (config.contains(sectionName)) { ValidateSection(config.at(sectionName)); }
    }
    if (config.contains("Render") && config.at("Render").contains("RenderParameter")) {
        ValidateSection(config.at("Render").at("RenderParameter"));
    }
    const auto& emitter = config.value("Emitter", config);
    ValidateChoice(emitter, "Shape", { "Sphere", "Box", "Cone", "Cylinder", "Circle" });
    const auto& render = config.value("Render", config);
    ValidateChoice(render, "Type", { "Mesh", "Trail" });
    ValidateChoice(render, "MeshType", { "Board", "Ring", "Cylinder", "Box", "Sphere", "Cone" });
    ValidateChoice(render, "BlendMode", { "None", "Normal", "Add", "Subtract", "Multiply", "Screen" });
    ValidateChoice(render, "CullMode", { "None", "Back", "Front" });
    if (config.contains("Fields")) {
        const auto& fields = config.at("Fields");
        if (!fields.is_array() || fields.size() > 16) { throw std::runtime_error("Fields must be an array with at most 16 entries"); }
        for (const auto& field : fields) {
            ValidateSection(field);
            ValidateChoice(field, "Type", { "Wind", "Attractor", "Repulsor", "Vortex" });
            ValidateChoice(field, "Space", { "Local", "World" });
        }
    }
}
}
