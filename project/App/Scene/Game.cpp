#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    Logger::Log("Game Initialize Start");

    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");

    winApp_ = std::make_unique<WinApp>();
    winApp_->initialize();

    DirectXCommon::GetInstance()->Initialize(winApp_.get());
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

    TextureManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(),
        SrvManager::GetInstance());

    ImGuiManager::GetInstance()->Initialize(winApp_.get(),DirectXCommon::GetInstance(),SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    SkinningObject3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    SkyBoxManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

    modelCommon_.Initialize(DirectXCommon::GetInstance());

    Input::GetInstance()->Initialize(winApp_.get());

    Logger::Log("Load Default Models");
    ModelManager::GetInstance()->Load("plane.obj");
    ModelManager::GetInstance()->Load("axis.obj");
    ModelManager::GetInstance()->Load("titleTex.obj");
    ModelManager::GetInstance()->Load("fence.obj");

    Logger::Log("Load Default Textures");
    TextureManager::GetInstance()->LoadTexture("resources/white.png");
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/fence.png");
    TextureManager::GetInstance()->LoadTexture("resources/BaseColor_Cube.png");

    SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());

    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize();

    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(DirectXCommon::GetInstance());

    SoundManager::GetInstance()->Initialize();

    Logger::Log("Game Initialize End");
}

void Game::Update()
{
    ImGuiManager::GetInstance()->Begin();

    Input::GetInstance()->Update();

    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        Logger::Log("Escape Pressed");
        endRequest_ = true;
    }

    SceneManager::GetInstance()->Update();
    SceneManager::GetInstance()->DrawImGui();

    ImGuiManager::GetInstance()->End();
}

void Game::Draw()
{
    SrvManager::GetInstance()->PreDraw();

    offscreenRenderer_->PreDraw();
    SceneManager::GetInstance()->Draw3D();
    offscreenRenderer_->PostDraw();

    DirectXCommon::GetInstance()->PreDraw();
    copyImageRenderer_->Draw(offscreenRenderer_->GetSrvHandleGPU());
    SceneManager::GetInstance()->Draw2D();

    ImGuiManager::GetInstance()->Draw();
    DirectXCommon::GetInstance()->PostDraw();
}

void Game::Finalize()
{
    Logger::Log("Game Finalize Start");

    SceneManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();

    SkinningObject3dManager::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();
    SkyBoxManager::GetInstance()->Finalize();

    TextureManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();

    SoundManager::GetInstance()->Finalize();

    DirectXCommon::GetInstance()->Finalize();

    if (winApp_) {
        winApp_->Finalize();
        winApp_.reset();
    }

    Logger::Log("Game Finalize End");
}
