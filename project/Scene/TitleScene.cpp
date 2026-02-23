#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
void TitleScene::Initialize()
{
}

void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {

        

        SceneManager::GetInstance()->SetNextScene(std::make_unique<GamePlayScene>());
    }
}

void TitleScene::Draw2D()
{
}

void TitleScene::Draw3D()
{
}

void TitleScene::DrawImGui()
{
}

void TitleScene::Finalize()
{
}