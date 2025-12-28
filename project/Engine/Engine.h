#pragma once
#include "Core/DirectXCommon/DirectXCommon.h"
#include "Core/Winapp/WinApp.h"
#include "Graphics/2D/SpriteManager.h"
#include "System/ImGuiManager/ImGuiManager.h"
#include "System/SrvManager/SrvManager.h"
#include "System/TextureManager/TextureManager.h"

#include "Graphics/3D/ModelManager.h"

#include "Graphics/3D/Object3dManager.h"

#include "System/input/Input.h"

#include "Graphics/3D/ModelCommon.h"
class Engine {
public:
    static Engine* GetInstance();

    void Initialize();
    void Update();
    void DrawBegin();
    void DrawEnd();
    void Finalize();

    WinApp* GetWinApp() const { return winApp_; }

private:
    Engine() = default;
    ~Engine() = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    WinApp* winApp_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;

    static Engine* instance_;
};
