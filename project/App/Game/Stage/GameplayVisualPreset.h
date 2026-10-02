#pragma once

#include "App/Scene/Common/SceneManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/math/MatrixMath.h"
#include <string>

// Game and model preview deliberately share these settings.
namespace GameplayVisualPreset {
inline constexpr float kIceShininess = 96.0f;
struct LightingPreset {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Vector4 ambient;
};

inline LightingPreset GetLighting(const std::string& stageId)
{
    if (stageId == "stage03") {
        return { { 0.90f, 0.96f, 1.0f, 1.0f },
            { -0.52f, -0.76f, 0.39f }, 0.90f, { 0.38f, 0.50f, 0.67f, 0.16f } };
    }
    return { { 1.0f, 0.97f, 0.90f, 1.0f },
        { -0.28f, -0.86f, 0.42f }, 0.95f, { 0.52f, 0.60f, 0.68f, 0.22f } };
}

inline void ApplyLighting(const std::string& stageId)
{
    LightManager* lightManager = LightManager::GetInstance();
    const LightingPreset lighting = GetLighting(stageId);
    lightManager->SetDirectional(lighting.color, lighting.direction, lighting.intensity);
    lightManager->SetAmbientColor({ lighting.ambient.x, lighting.ambient.y, lighting.ambient.z });
    lightManager->SetAmbientIntensity(lighting.ambient.w);
    if (stageId == "stage03") {
        lightManager->SetHemisphereColors({ 0.72f, 0.94f, 1.16f }, { 0.24f, 0.36f, 0.50f });
    } else {
        lightManager->SetHemisphereColors({ 0.78f, 0.90f, 1.10f }, { 0.42f, 0.38f, 0.34f });
    }
    lightManager->SetPointRadius(10.0f);
    lightManager->SetPointDecay(1.0f);
    lightManager->SetPointLight({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 2.0f, 0.0f }, 0.0f);
    lightManager->SetSpotLightIntensity(0.0f);
}

inline void ApplyAtmosphere(const std::string& stageId)
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (stageId == "stage03") {
        sceneManager->SetSceneExposure(0.95f);
        // Keep nearby ice clear and let middle-distance silhouettes survive the fog.
        sceneManager->SetSceneFogColor({ 0.58f, 0.80f, 0.96f, 1.0f });
        sceneManager->SetSceneDistanceFog({ 450.0f, 1000.0f, 1.0f, 1.35f });
        return;
    }
    sceneManager->SetSceneExposure(1.0f);
    sceneManager->SetSceneDistanceFog({ 380.0f, 720.0f, 1.0f, 1.2f });
}

inline void ConfigurePostEffects(bool boosting)
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->ClearPostEffects();
    sceneManager->AddPostEffect(PostEffectType::DepthOutline, PostEffectStage::BeforeParticle);
    sceneManager->AddPostEffect(PostEffectType::Fog, PostEffectStage::BeforeParticle);
    if (boosting) {
        sceneManager->AddPostEffect(PostEffectType::RadialBlur, PostEffectStage::BeforeParticle);
        sceneManager->AddPostEffect(PostEffectType::FocusLine, PostEffectStage::BeforeParticle);
        // Bloom is applied globally after particles by PostEffectManager.
    }
}

inline const char* IceMaterialFolder(const std::string& modelPath)
{
    if (modelPath == "Environment/Ice/ice_island.obj") { return "resources/Shaders/Object3D/StageIceIsland"; }
    if (modelPath == "Environment/Ice/ice_boulder.obj") { return "resources/Shaders/Object3D/StageIceBoulder"; }
    if (modelPath == "Environment/Ice/ice_slab.obj") { return "resources/Shaders/Object3D/StageIceSlab"; }
    if (modelPath == "Environment/Ice/ice_arch.obj") { return "resources/Shaders/Object3D/StageIceArch"; }
    if (modelPath == "Environment/Ice/crystal.obj") { return "resources/Shaders/Object3D/StageIceCrystal"; }
    return "resources/Shaders/Object3D/StageIceSpire";
}
}
