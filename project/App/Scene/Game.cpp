#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    Logger::Log("Game Initialize Start");

    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");

    WinApp::GetInstance()->initialize();

    DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

    ImGuiManager::GetInstance()->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    SkinningObject3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    SkyBoxManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

    modelCommon_.Initialize(DirectXCommon::GetInstance());

    Input::GetInstance()->Initialize(WinApp::GetInstance());

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
    Input::GetInstance()->Update();

    if (Input::GetInstance()->IsKeyPressed(DIK_P)) {
        isPostEffectEnabled_ = !isPostEffectEnabled_;
    }

    ImGuiManager::GetInstance()->Begin();

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

    // ↑で描画したやつをテクスチャとして描画する
    DirectXCommon::GetInstance()->PreDraw();
    copyImageRenderer_->SetPostEffectType(SceneManager::GetInstance()->GetPostEffectType()); // シーンマネージャーからポストエフェクトの種類を取得してセット
    copyImageRenderer_->Draw(offscreenRenderer_->GetSrvHandleGPU(),offscreenRenderer_->GetDepthSrvHandleGPU());
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

    WinApp::FinalizeInstance();

    Logger::Log("Game Finalize End");
}
