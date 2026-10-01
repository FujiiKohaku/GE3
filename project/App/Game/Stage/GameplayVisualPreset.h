#pragma once

#include "App/Scene/SceneManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/math/MatrixMath.h"
#include <string>

// Game and model preview deliberately share these settings.
namespace GameplayVisualPreset {
inline constexpr float kIceShininess = 96.0f;
inline void ApplyLighting(const std::string& stageId)
{
    LightManager* lightManager = LightManager::GetInstance();
    if (stageId == "stage03") {
        lightManager->SetDirectional({ 0.90f, 0.96f, 1.0f, 1.0f },
            Normalize(Vector3 { -0.35f, -0.82f, 0.45f }), 0.90f);
        lightManager->SetAmbientColor({ 0.40f, 0.52f, 0.68f });
        lightManager->SetAmbientIntensity(0.24f);
    } else {
        lightManager->SetDirectional({ 1.0f, 0.97f, 0.90f, 1.0f },
            Normalize(Vector3 { -0.28f, -0.86f, 0.42f }), 1.0f);
        lightManager->SetAmbientColor({ 0.52f, 0.60f, 0.68f });
        lightManager->SetAmbientIntensity(0.28f);
    }
    lightManager->SetPointRadius(10.0f);
    lightManager->SetPointDecay(1.0f);
    lightManager->SetPointLight({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 2.0f, 0.0f }, 0.0f);
    lightManager->SetSpotLightIntensity(0.0f);
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
        sceneManager->AddPostEffect(PostEffectType::Bloom, PostEffectStage::BeforeParticle);
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
