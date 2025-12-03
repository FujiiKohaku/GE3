#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
void TitleScene::Initialize()
{
 
}

void TitleScene::Update()
{
    // 例：Enterキーでゲーム画面へ
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {

        BaseScene* scene = new GamePlayScene();

        SceneManager::GetInstance()->SetNextScene(scene);
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