#include "Tools/ModelPreview/ModelPreviewApp.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Effect/EffectConfigValidation.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Logger/Logger.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"
#include "Engine/PostEffect/Fog/FogManager.h"
#include "Engine/Light/LightManager.h"
#include "EffectCapacityTestAccess.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {
void Require(bool isSatisfied, const std::string& message)
{
    if (!isSatisfied) { throw std::runtime_error(message); }
}

void Frame(ModelPreviewApp& app, EffectManager* effects, OffscreenRenderer& renderTarget,
    EffectCapacityTestAccess* capacityTest = nullptr, EffectHandle handle = kInvalidEffectHandle,
    bool shouldTestRejectedEmission = false)
{
    SrvManager::GetInstance()->PreDraw();
    effects->Update();
    if (capacityTest != nullptr) { capacityTest->Record(effects, handle, shouldTestRejectedEmission); }
    renderTarget.PreDraw(DirectXCommon::GetInstance()->GetDSVHandle());
    effects->PreDraw();
    effects->Draw();
    renderTarget.PostDraw();
    app.Update();
    app.Draw();
}

void ExpectInvalid(const nlohmann::json& config)
{
    bool hasFailed = false;
    try { EffectConfigValidation::Validate(config); }
    catch (const std::exception&) { hasFailed = true; }
    Require(hasFailed, "Invalid config was accepted");
}

void VerifyTypeAndSave(EffectManager* effects, const nlohmann::json& original, EffectHandle handle)
{
    std::string error;
    nlohmann::json actual;
    Require(original.at("Render").at("Type") == "Mesh", "Legacy mesh config did not acquire Render.Type");
    auto changedType = original;
    changedType["Render"]["Type"] = "Trail";
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", changedType, error) &&
        error.find("type cannot be changed") != std::string::npos, "Mesh-to-Trail change was accepted");
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == original && effects->IsEffectAlive(handle),
        "Rejected type change affected the running mesh");

    nlohmann::json trail;
    Require(effects->GetEffectConfig("BackflipTrail", trail, error), error);
    Require(trail.at("Render").at("Type") == "Trail", "Trail type was lost");
    changedType = trail;
    changedType["Render"]["Type"] = "Mesh";
    Require(!effects->ApplyEffectConfigAndRestart("BackflipTrail", changedType, error), "Trail-to-Mesh change was accepted");
    changedType["Render"].erase("Type");
    Require(!effects->ApplyEffectConfigAndRestart("BackflipTrail", changedType, error), "Omitted type changed an existing Trail to Mesh");
    Require(effects->GetEffectConfig("BackflipTrail", actual, error) && actual == trail, "Rejected type change affected Trail config");

    bool hasRejectedRegistration = false;
    try {
        effects->RegisterEffect({ "DamageHit", "resources/Effects/BackflipTrail",
            "resources/Effects/BackflipTrail/Effect.json", "resources/Effects/BackflipTrail/Emit.CS.hlsl",
            "resources/Effects/BackflipTrail/Update.CS.hlsl" });
    } catch (const std::exception&) { hasRejectedRegistration = true; }
    Require(hasRejectedRegistration, "RegisterEffect bypassed the fixed type");

    const std::filesystem::path jsonPath = "runtime/EffectEditingSaveTest.json";
    const std::filesystem::path temporaryPath = "runtime/EffectEditingSaveTest.json.tmp";
    Require(!std::filesystem::exists(jsonPath) && !std::filesystem::exists(temporaryPath), "Save test paths are already occupied");
    auto legacy = original;
    legacy["Render"].erase("Type");
    { std::ofstream file(jsonPath); file << legacy.dump(2); }
    effects->RegisterEffect({ "EffectEditingSaveTest", "resources/Effects/DamageHit", jsonPath.generic_string(),
        "resources/Effects/DamageHit/Emit.CS.hlsl", "resources/Effects/DamageHit/Update.CS.hlsl" });
    Require(effects->GetEffectConfig("EffectEditingSaveTest", actual, error) && actual == original, "Legacy type normalization changed settings");
    nlohmann::json onDisk;
    { std::ifstream file(jsonPath); file >> onDisk; }
    Require(onDisk == legacy, "Loading unexpectedly rewrote JSON");
    Require(effects->SaveEffectConfig("EffectEditingSaveTest", error), error);
    { std::ifstream file(jsonPath); file >> onDisk; }
    Require(onDisk == original, "Saved JSON did not explicitly preserve Mesh type");
    { std::ofstream file(temporaryPath); file << "keep existing temporary file"; }
    Require(!effects->SaveEffectConfig("EffectEditingSaveTest", error), "Save overwrote an occupied temporary file");
    { std::ifstream file(jsonPath); file >> onDisk; }
    Require(onDisk == original, "Failed save damaged original JSON");
    std::filesystem::remove(temporaryPath);
    std::filesystem::remove(jsonPath);
    { std::ofstream file(jsonPath); file << trail.dump(2); }
    effects->RegisterEffect({ "EffectEditingTrailSaveTest", "resources/Effects/BackflipTrail", jsonPath.generic_string(),
        "resources/Effects/BackflipTrail/Emit.CS.hlsl", "resources/Effects/BackflipTrail/Update.CS.hlsl" });
    Require(effects->SaveEffectConfig("EffectEditingTrailSaveTest", error), error);
    { std::ifstream file(jsonPath); file >> onDisk; }
    Require(onDisk == trail, "Saved Trail config changed type or settings");
    std::filesystem::remove(jsonPath);
    Require(!effects->SaveEffectConfig("missing-effect", error), "Unknown effect was saved");
}

ActiveEffect FindEffect(EffectManager* effects, EffectHandle handle)
{
    for (const auto& activeEffect : effects->GetActiveEffects()) {
        if (activeEffect.handle == handle) { return activeEffect; }
    }
    throw std::runtime_error("Test effect was not found");
}

bool AreLightsEqual(const PointLight& first, const PointLight& second)
{
    return first.color.x == second.color.x && first.color.y == second.color.y &&
        first.color.z == second.color.z && first.color.w == second.color.w &&
        first.position.x == second.position.x && first.position.y == second.position.y && first.position.z == second.position.z &&
        first.intensity == second.intensity && first.radius == second.radius && first.decay == second.decay && first.isActive == second.isActive;
}

void VerifyLightReuse(ModelPreviewApp& app, EffectManager* effects, OffscreenRenderer& renderTarget,
    EffectHandle handle, EffectHandle secondHandle, EffectHandle trailHandle)
{
    auto* lights = LightManager::GetInstance();
    std::string error;
    nlohmann::json config;
    Require(effects->GetEffectConfig("DamageHit", config, error), error);
    config["Light"] = { { "Enabled", true }, { "Type", "Point" }, { "Color", { 1.0f, 0.2f, 0.1f, 1.0f } },
        { "Intensity", 2.0f }, { "Radius", 5.0f }, { "Decay", 2.0f }, { "Offset", { 0.0f, 0.0f, 0.0f } } };
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", config, error), error);
    Frame(app, effects, renderTarget);
    const auto lightHandle = FindEffect(effects, handle).pointLightHandle;
    const auto secondLightHandle = FindEffect(effects, secondHandle).pointLightHandle;
    Require(lightHandle != kInvalidPointLightHandle && secondLightHandle != kInvalidPointLightHandle, "Effect lights were not created");
    Require(lights->SetPointLightShadowEnabled(lightHandle, true), "Could not enable existing light shadow");
    std::vector<PointLightHandle> fillerHandles;
    std::vector<PointLight> fillerLights;
    for (uint32_t index = 0; index < LightManager::kMaxPointLights; ++index) {
        const auto filler = lights->AddPointLight({ 0.2f, 0.4f, 0.6f, 1.0f }, { 100.0f + index, 2.0f, 3.0f }, 1.0f, 3.0f, 1.0f);
        if (filler == kInvalidPointLightHandle) { break; }
        PointLight light {};
        Require(lights->TryGetPointLight(filler, light), "Could not inspect filler light");
        fillerHandles.push_back(filler);
        fillerLights.push_back(light);
    }
    Require(!fillerHandles.empty() && lights->AddPointLight({ 1, 1, 1, 1 }, {}, 1, 3, 1) == kInvalidPointLightHandle,
        "Light pool was not full");
    config["Light"]["Color"] = { 0.1f, 0.3f, 1.0f, 1.0f };
    config["Light"]["Intensity"] = 3.0f;
    config["Light"]["Radius"] = 8.0f;
    config["Light"]["Offset"] = { 1.0f, 2.0f, 3.0f };
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", config, error), error);
    Frame(app, effects, renderTarget);
    Require(FindEffect(effects, handle).pointLightHandle == lightHandle &&
        FindEffect(effects, secondHandle).pointLightHandle == secondLightHandle, "Full-pool edit replaced existing light handles");
    PointLight current {};
    Require(lights->TryGetPointLight(lightHandle, current) && current.color.z == 1.0f &&
        current.intensity == 3.0f && current.radius == 8.0f && current.position.y == 2.0f, "Reused light settings were not updated");
    Require(lights->IsPointLightShadowEnabled(lightHandle & 0xffu), "Reusing a light reset its shadow flag");
    Require(effects->RestartEffect(handle, error), error);
    Frame(app, effects, renderTarget);
    Require(effects->ReloadEffectShaders("DamageHit", error), error);
    Frame(app, effects, renderTarget);
    Require(FindEffect(effects, handle).pointLightHandle == lightHandle, "Restart/reload replaced a borrowed light");
    PointLight firstBefore {};
    PointLight secondBefore {};
    Require(lights->TryGetPointLight(lightHandle, firstBefore) && lights->TryGetPointLight(secondLightHandle, secondBefore), "Existing light disappeared");
    auto invalid = config;
    invalid["Light"]["Intensity"] = 100.0f;
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error), "Invalid reused-light settings were accepted");
    Require(lights->TryGetPointLight(lightHandle, current) && AreLightsEqual(current, firstBefore), "Failed edit changed an existing light");
    // The first update succeeds; overflowing the second instance's position forces rollback of that first update.
    const Vector3 secondPosition = FindEffect(effects, secondHandle).position;
    const float kMaxCoordinate = (std::numeric_limits<float>::max)();
    Require(effects->SetEffectPosition(secondHandle, { kMaxCoordinate, 0, 0 }), "Could not set rollback test position");
    invalid = config;
    invalid["Light"]["Offset"] = { kMaxCoordinate, 0, 0 };
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error), "Overflowing light position was accepted");
    Require(lights->TryGetPointLight(lightHandle, current) && AreLightsEqual(current, firstBefore), "Partial light update was not rolled back");
    Require(lights->TryGetPointLight(secondLightHandle, current) && AreLightsEqual(current, secondBefore), "Failed edit changed the second light");
    Require(effects->SetEffectPosition(secondHandle, secondPosition), "Could not restore test position");
    nlohmann::json actual;
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == config, "Failed light update changed effect config");

    nlohmann::json trailConfig;
    Require(effects->GetEffectConfig("BackflipTrail", trailConfig, error), error);
    const auto secondTrailHandle = effects->PlayLoopEffect("BackflipTrail", {});
    Require(secondTrailHandle != kInvalidEffectHandle, "Could not play second unlit Trail");
    Frame(app, effects, renderTarget);
    auto litTrailConfig = trailConfig;
    litTrailConfig["Light"] = config.at("Light");
    Require(!effects->ApplyEffectConfigAndRestart("BackflipTrail", litTrailConfig, error), "New light stole a slot in the full pool");
    // Make only one slot available for two instances: a partially allocated replacement must be released on failure.
    Require(lights->RemovePointLight(fillerHandles.back()), "Could not free one test slot");
    fillerHandles.pop_back();
    fillerLights.pop_back();
    Require(!effects->ApplyEffectConfigAndRestart("BackflipTrail", litTrailConfig, error), "Partial new-light allocation was accepted");
    Require(effects->GetEffectConfig("BackflipTrail", actual, error) && actual == trailConfig, "Failed allocation changed Trail config");
    Require(FindEffect(effects, trailHandle).pointLightHandle == kInvalidPointLightHandle &&
        FindEffect(effects, secondTrailHandle).pointLightHandle == kInvalidPointLightHandle, "Failed allocation changed unlit instances");
    const auto probe = lights->AddPointLight({ 1, 1, 1, 1 }, {}, 1, 3, 1);
    Require(probe != kInvalidPointLightHandle && lights->AddPointLight({ 1, 1, 1, 1 }, {}, 1, 3, 1) == kInvalidPointLightHandle,
        "Failed allocation leaked a light slot");
    Require(lights->RemovePointLight(probe), "Could not release light probe");
    Require(lights->RemovePointLight(fillerHandles.back()), "Could not free second test slot");
    fillerHandles.pop_back();
    fillerLights.pop_back();
    Require(effects->ApplyEffectConfigAndRestart("BackflipTrail", litTrailConfig, error), error);
    Frame(app, effects, renderTarget);
    Require(FindEffect(effects, trailHandle).pointLightHandle != kInvalidPointLightHandle &&
        FindEffect(effects, secondTrailHandle).pointLightHandle != kInvalidPointLightHandle, "New lights were not added when slots became available");
    for (size_t index = 0; index < fillerHandles.size(); ++index) {
        Require(lights->TryGetPointLight(fillerHandles[index], current) && AreLightsEqual(current, fillerLights[index]),
            "An unrelated light was modified or removed");
    }
    config["Light"]["Enabled"] = false;
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", config, error), error);
    Frame(app, effects, renderTarget);
    Require(!lights->TryGetPointLight(lightHandle, current) && !lights->TryGetPointLight(secondLightHandle, current),
        "Disabling effect lighting did not release its own lights");
    for (const auto filler : fillerHandles) { Require(lights->RemovePointLight(filler), "Unrelated light handle was invalidated"); }
    Logger::Log("Effect light tests PASS: full-pool reuse, handles, shadows, rollback, allocation failure, unrelated lights");
}

void VerifyInstanceParameterRetention(ModelPreviewApp& app, EffectManager* effects, OffscreenRenderer& renderTarget,
    EffectHandle handle, EffectHandle secondHandle)
{
    const auto thirdHandle = effects->PlayLoopEffect("DamageHit", {});
    Require(thirdHandle != kInvalidEffectHandle, "Could not create zero-velocity override test instance");
    Frame(app, effects, renderTarget);
    std::string error;
    nlohmann::json config;
    Require(effects->GetEffectConfig("DamageHit", config, error), error);
    config["Particle"]["Velocity"] = { 8.0f, 9.0f, 10.0f };
    config["Particle"]["MaxParticles"] = 257;
    const Vector3 velocity { 2.0f, 3.0f, 4.0f };
    const Vector3 direction { 1.0f, 0.5f, 0.0f };
    const Vector3 defaultDirection { 0.0f, 0.0f, -1.0f };
    const Vector3 thirdDirection { 0.0f, 1.0f, 0.0f };
    EffectSkeletonPose skeletonPose {};
    skeletonPose.jointCount = 2;
    skeletonPose.jointPositions[0] = { 1.0f, 2.0f, 3.0f, 1.0f };
    skeletonPose.jointPositions[1] = { 4.0f, 5.0f, 6.0f, 1.0f };
    const EffectSkeletonPose defaultPose {};
    Require(effects->SetEffectVelocity(handle, velocity) && effects->SetEffectDirection(handle, direction) &&
        effects->SetEffectSkeletonPose(handle, skeletonPose), "Could not set first instance inputs");
    Require(effects->SetEffectVelocity(thirdHandle, {}) && effects->SetEffectDirection(thirdHandle, thirdDirection),
        "Could not explicitly override velocity with zero");
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", config, error), error);
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, handle, velocity, direction, skeletonPose, true);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, secondHandle, { 8, 9, 10 }, defaultDirection, defaultPose, false);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, thirdHandle, {}, thirdDirection, defaultPose, true);
    Require(effects->RestartEffect(handle, error), error);
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, handle, velocity, direction, skeletonPose, true);

    const Vector3 latestVelocity { -1.0f, -2.0f, -3.0f };
    const Vector3 latestDirection { -1.0f, 0.0f, 0.0f };
    skeletonPose.jointCount = kEffectSkeletonJointCount;
    for (uint32_t jointIndex = 0; jointIndex < kEffectSkeletonJointCount; ++jointIndex) {
        const float coordinate = static_cast<float>(jointIndex);
        skeletonPose.jointPositions[jointIndex] = { coordinate, coordinate + 1.0f, coordinate + 2.0f, 1.0f };
    }
    Require(effects->SetEffectVelocity(handle, latestVelocity) && effects->SetEffectDirection(handle, latestDirection) &&
        effects->SetEffectSkeletonPose(handle, skeletonPose), "Could not update per-instance inputs");
    auto invalid = config;
    invalid["Render"]["Texture"] = "resources/missing-instance-input-test.png";
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error), "Invalid edit was accepted");
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, handle, latestVelocity, latestDirection, skeletonPose, true);
    Require(effects->ReloadEffectShaders("DamageHit", error), error);
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, handle, latestVelocity, latestDirection, skeletonPose, true);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, secondHandle, { 8, 9, 10 }, defaultDirection, defaultPose, false);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, thirdHandle, {}, thirdDirection, defaultPose, true);

    config["Particle"]["Velocity"] = { -4.0f, -5.0f, -6.0f };
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", config, error), error);
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, handle, latestVelocity, latestDirection, skeletonPose, true);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, secondHandle, { -4, -5, -6 }, defaultDirection, defaultPose, false);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, thirdHandle, {}, thirdDirection, defaultPose, true);
    nlohmann::json actual;
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == config, "Instance overrides leaked into saved config");
    Require(!effects->SetEffectVelocity(kInvalidEffectHandle, velocity) && !effects->SetEffectDirection(kInvalidEffectHandle, direction) &&
        !effects->SetEffectSkeletonPose(kInvalidEffectHandle, skeletonPose), "Invalid setter handle was accepted");
    effects->StopAllEffects();
    const auto freshHandle = effects->PlayLoopEffect("DamageHit", {});
    Require(freshHandle != kInvalidEffectHandle, "Could not reuse stopped effect resources");
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess::VerifyInstanceParameters(effects, freshHandle, { -4, -5, -6 }, defaultDirection, defaultPose, false);
    Logger::Log("Effect instance input tests PASS: restart, config apply, reload, latest values, zero override, failure, pool reset");
}

void Run(ModelPreviewApp& app)
{
    ExpectInvalid({ { "Particle", { { "MaxParticles", -1 } } } });
    ExpectInvalid({ { "Particle", { { "LifeTime", 0 } } } });
    ExpectInvalid({ { "Particle", { { "Velocity", { 1, 2 } } } } });
    ExpectInvalid({ { "Particle", { { "StartScale", std::numeric_limits<double>::infinity() } } } });
    ExpectInvalid({ { "Fields", nlohmann::json::object() } });
    ExpectInvalid({ { "Emitter", { { "Count", -1 } } } });
    ExpectInvalid({ { "Emitter", { { "Count", 1.5 } } } });
    ExpectInvalid({ { "Emitter", { { "Count", 4097 } } } });
    ExpectInvalid({ { "Emitter", { { "EmitCount", 16777216 } } } });
    ExpectInvalid({ { "Emitter", { { "Count", 0xffffffffu } } } });
    EffectConfigValidation::Validate({ { "Emitter", { { "Count", 0 } } } });
    EffectConfigValidation::Validate({ { "Emitter", { { "Count", 4096 } } } });
    nlohmann::json flat = { { "Texture", "legacy.png" }, { "Loop", true }, { "EmissionStrength", 2.0f }, { "UnknownField", 7 } };
    EffectConfigValidation::NormalizeRenderType(flat);
    Require(flat.at("Render").at("Type") == "Mesh" && flat.at("Render").at("Texture") == "legacy.png" &&
        flat.at("Render").at("Loop") == true && flat.at("Render").at("EmissionStrength") == 2.0f &&
        flat.at("UnknownField") == 7, "Legacy flat rendering settings were lost");
    Camera camera;
    camera.Initialize();
    auto* effects = EffectManager::GetInstance();
    effects->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), &camera);
    OffscreenRenderer renderTarget;
    renderTarget.Initialize();
    FogManager fog;
    fog.Initialize(DirectXCommon::GetInstance());
    effects->SetFogConstantBufferView(fog.GetConstantBufferView());
    const auto handle = effects->PlayLoopEffect("DamageHit", {});
    Require(handle != kInvalidEffectHandle, "Could not play DamageHit");
    const auto secondHandle = effects->PlayLoopEffect("DamageHit", { 1.0f, 0.0f, 0.0f });
    Require(secondHandle != kInvalidEffectHandle, "Could not play second instance");
    Frame(app, effects, renderTarget);

    std::string error;
    nlohmann::json original;
    Require(effects->GetEffectConfig("DamageHit", original, error), error);
    VerifyTypeAndSave(effects, original, handle);
    const auto trailHandle = effects->PlayLoopEffect("BackflipTrail", {});
    Require(trailHandle != kInvalidEffectHandle, "Could not play Trail");
    Frame(app, effects, renderTarget);
    nlohmann::json editedTrail;
    Require(effects->GetEffectConfig("BackflipTrail", editedTrail, error), error);
    editedTrail["Trail"]["StartWidth"] = 0.4f;
    Require(effects->ApplyEffectConfigAndRestart("BackflipTrail", editedTrail, error), error);
    Frame(app, effects, renderTarget);
    Require(effects->IsEffectAlive(trailHandle), "Same-type Trail edit lost its handle");
    auto edited = original;
    edited["Emitter"]["Count"] = 7;
    edited["Particle"]["StartColor"] = { 0.1, 0.3, 1.0, 1.0 };
    edited["EditorUnknownField"] = "preserved";
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
    Require(effects->IsEffectAlive(handle) && effects->IsEffectAlive(secondHandle), "Handles changed during apply");
    Frame(app, effects, renderTarget);
    nlohmann::json actual;
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == edited, "Config snapshot mismatch");
    Require(effects->GetActiveEffects().front().isLoop, "Explicit loop was lost");

    auto invalid = edited;
    invalid["Particle"]["MaxParticles"] = -1;
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error) && !error.empty(), "Negative capacity was accepted");
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == edited, "Failed validation changed config");
    invalid = edited;
    invalid["Emitter"]["Count"] = 16777216;
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error), "Excessive emit count was accepted");
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == edited, "Invalid emit count changed config");
    invalid = edited;
    invalid["Render"]["Texture"] = "resources/missing-effect-edit-test.png";
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error), "Missing texture was accepted");
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == edited, "Missing texture changed config");

    const std::filesystem::path invalidShader = "resources/Effects/EffectEditingInvalid.PS.hlsl";
    { std::ofstream file(invalidShader); file << "This is deliberately invalid HLSL;"; }
    invalid = edited;
    invalid["Shaders"]["Pixel"] = invalidShader.generic_string();
    const bool hasAcceptedInvalidShader = effects->ApplyEffectConfigAndRestart("DamageHit", invalid, error);
    std::filesystem::remove(invalidShader);
    Require(!hasAcceptedInvalidShader && !error.empty(), "Invalid shader was accepted");
    Require(effects->GetEffectConfig("DamageHit", actual, error) && actual == edited, "Shader failure changed config");
    Require(effects->IsEffectAlive(handle), "Shader failure stopped current effect");
    Frame(app, effects, renderTarget);
    Require(effects->ReloadEffectShaders("DamageHit", error), error);
    Frame(app, effects, renderTarget);
    Require(effects->RestartEffect(handle, error), error);
    Frame(app, effects, renderTarget);

    DirectXCommon::GetInstance()->GetCommandList();
    Require(!effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), "Mid-frame apply was accepted");
    Frame(app, effects, renderTarget);
    EffectCapacityTestAccess capacityTest;
    const uint32_t kCapacities[] = { 1, 17, 255, 256, 257, 1024, 2048, 4096 };
    edited["Particle"]["LifeTime"] = 100.0f;
    edited["Emitter"]["Frequency"] = 0.000001f;
    for (uint32_t capacity : kCapacities) {
        edited["Particle"]["MaxParticles"] = capacity;
        edited["Emitter"]["Count"] = capacity;
        Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
        Frame(app, effects, renderTarget, &capacityTest, handle);
        capacityTest.Verify(capacity);
    }
    // Exercise release and re-emission above the old 1024 limit, not only initial allocation.
    edited["Particle"]["LifeTime"] = 0.001f;
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
    bool hasSeenFullBuffer = false;
    bool hasSeenEmptyBuffer = false;
    for (int frame = 0; frame < 16; ++frame) {
        Frame(app, effects, renderTarget, &capacityTest, handle);
        const uint32_t liveCount = capacityTest.Verify(0xffffffffu);
        if (liveCount == 4096) { hasSeenFullBuffer = true; }
        if (liveCount == 0) { hasSeenEmptyBuffer = true; }
    }
    Require(hasSeenFullBuffer && hasSeenEmptyBuffer, "Large particle buffer was not emitted and recycled");
    edited["Particle"]["LifeTime"] = 100.0f;
    // Reject bad counts immediately before Dispatch, even if CPU emitter data bypasses JSON validation.
    edited["Emitter"]["Count"] = 0;
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
    Frame(app, effects, renderTarget, &capacityTest, handle, true);
    capacityTest.Verify(0);
    // The highest occupied indices are above 1024, so fixed-size field dispatch guards would fail this check.
    edited["Emitter"]["Count"] = 7;
    edited["Fields"] = nlohmann::json::array({ {
        { "Type", "Wind" }, { "Space", "World" }, { "Direction", { 1.0f, 0.0f, 0.0f } },
        { "Radius", 1000.0f }, { "Strength", 1000000.0f }, { "Falloff", 1.0f }
    } });
    Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
    Frame(app, effects, renderTarget, &capacityTest, handle);
    capacityTest.Verify(7, true);
    edited.erase("Fields");
    for (int iteration = 0; iteration < 12; ++iteration) {
        edited["Emitter"]["Count"] = iteration + 1;
        Require(effects->ApplyEffectConfigAndRestart("DamageHit", edited, error), error);
        Frame(app, effects, renderTarget);
    }
    Require(!effects->RestartEffect(kInvalidEffectHandle, error), "Invalid handle was accepted");
    Require(!effects->ReloadEffectShaders("missing-effect", error), "Unknown effect was accepted");
    VerifyLightReuse(app, effects, renderTarget, handle, secondHandle, trailHandle);
    VerifyInstanceParameterRetention(app, effects, renderTarget, handle, secondHandle);
    effects->StopAllEffects();
    EffectManager::Finalize();
    Logger::Log("EffectEditingTests PASS: apply, restart, capacity, shader reload, validation, rollback, frame guard");
}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    ModelPreviewApp app;
    Logger::Initialize();
    int exitCode = 0;
    try {
        app.Initialize();
        Run(app);
        app.Finalize();
    } catch (const std::exception& exception) {
        Logger::Error(exception.what());
        std::ofstream("runtime/EffectEditingTests.failure.txt") << exception.what();
        exitCode = 1;
    }
    Logger::Finalize();
    return exitCode;
}
