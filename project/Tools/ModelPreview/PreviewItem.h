#pragma once
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include <memory>
#include <string>

struct PreviewItem {
    std::unique_ptr<Model> model;
    std::unique_ptr<Object3d> object;
    std::string name;
    Vector3 center {};
    float radius = 1.0f;
    Vector3 position {};
    Vector3 rotation {};
    Vector3 axisScale { 1.0f, 1.0f, 1.0f };
    bool stageTransform = false;
    float scale = 1.0f;
    Vector4 color { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string shader = "Standard";
    bool transparent = false;
    bool environment = false;
    float reflection = 0.12f;
    float shininess = 32.0f;
};
