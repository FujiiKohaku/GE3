#include "PreviewSmokeTest.h"
#include "ModelPreviewApp.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Logger/Logger.h"
#include <stdexcept>

void RunModelPreviewSmokeTest(ModelPreviewApp& app)
{
    app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
    if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
    app.Capture().RequestPng();
    app.Capture().StartGif(15, 8);
    for (int frame = 0; frame < 8; ++frame) {
        app.Update();
        app.Draw();
    }
    if (!app.Error().empty() || app.Capture().Status().find("Saved:") != 0) {
        throw std::runtime_error("Preview smoke capture failed: " + app.Capture().Status());
    }
    const char* smokeModels[] = {
        "resources/Models/Environment/Ice/IceSpike.obj",
        "resources/Models/Environment/Ice/ice_slab.obj",
        "resources/Models/Environment/Ice/ice_arch.obj",
        "resources/Models/Environment/Ice/ice_island.obj",
        "resources/Models/Environment/Ice/crystal.obj",
        "resources/Models/AnimatedCube.gltf",
        "resources/Models/Boss/IceJellyfish/IceJellyfishBell.obj"
    };
    for (const char* model : smokeModels) {
        app.Load(model, false);
        if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
        app.Capture().RequestPng();
        app.Update();
        app.Draw();
        if (!app.Error().empty() || app.Capture().Status().find("Saved:") != 0) {
            throw std::runtime_error("Model preview capture failed: " + std::string(model));
        }
    }
    Object3dManager::GetInstance()->ReloadMaterialPipelines();
    app.Load("resources/Models/Environment/Ice/ice_boulder.obj", true);
    app.Update();
    app.Draw();
    if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
    app.Load("resources/Models/missing-preview-test.obj", false);
    if (app.Error().empty()) { throw std::runtime_error("Missing model was accepted"); }
    app.Load("resources/Models/Environment/Ice/ice_boulder.obj", false);
    if (!app.Error().empty()) { throw std::runtime_error(app.Error()); }
    Logger::Log("ModelPreview smoke test passed: 8 models, PNG + GIF, multiple objects, shader reload, invalid-file recovery");
}
