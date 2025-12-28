#include "Engine.h"

Engine* Engine::instance_ = nullptr;

Engine* Engine::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new Engine();
    }
    return instance_;
}
void Engine::Initialize()
{
    winApp_ = new WinApp();
    winApp_->initialize();
    DirectXCommon::GetInstance()->Initialize(winApp_);
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    ImGuiManager::GetInstance()->Initialize(winApp_, DirectXCommon::GetInstance(), SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Input::GetInstance()->Initialize(winApp_);
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(DirectXCommon::GetInstance());
}
void Engine::Update()
{
    Input::GetInstance()->Update();

    ImGuiManager::GetInstance()->Begin();
    ImGuiManager::GetInstance()->End();
}
void Engine::DrawBegin()
{
    SrvManager::GetInstance()->PreDraw();
    DirectXCommon::GetInstance()->PreDraw();
}

void Engine::DrawEnd()
{
    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}
void Engine::Finalize()
{

    ImGuiManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();
    DirectXCommon::GetInstance()->Finalize();
    winApp_->Finalize();
    delete winApp_;
    delete modelCommon_;
    delete instance_;
    instance_ = nullptr;
}