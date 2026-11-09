#include "AlmaRunScene.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "GameOverScene.h"
#include "ClearScene.h"
#include "BitmapFont.h"
#include "App/AlmaRun/Common/SceneManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Screenshot/ScreenshotManager.h"
using namespace KamataEngine;
AlmaRunScene::AlmaRunScene()=default;
AlmaRunScene::~AlmaRunScene()=default;
void AlmaRunScene::Start(Phase next) {
 ::DirectXCommon::GetInstance()->WaitForGPU();
 uiDraws.clear();title.reset();game.reset();over.reset();clear.reset();phase=next;
 if(next==Phase::Title){title=std::make_unique<AlmaTitleScene>();title->Initialize();}
 if(next==Phase::Game){game=std::make_unique<AlmaGameScene>();game->Initialize();}
 if(next==Phase::Over){over=std::make_unique<AlmaGameOverScene>();over->Initialize();}
 if(next==Phase::Clear){clear=std::make_unique<AlmaClearScene>();clear->Initialize();}
 Logger::Log("[AlmaRun] Initialized phase " + std::to_string(static_cast<int>(next)));
}
void AlmaRunScene::Initialize(){
 Object3dManager::GetInstance()->Initialize(::DirectXCommon::GetInstance());
 LightManager::GetInstance()->Initialize(::DirectXCommon::GetInstance());
 LightingPreset light;light.color={1.0f,0.91f,0.76f,1};light.direction={0.35f,-0.7f,0.6f};light.intensity=1.25f;
 light.ambient={0.8f,0.88f,1,0.45f};light.skyColor={0.62f,0.77f,1};light.groundColor={0.32f,0.26f,0.2f};
 LightManager::GetInstance()->ApplyLightingPreset(light);
 LightManager::GetInstance()->SetSkyLighting(true,0.45f);
 auto* scenes=SceneManager::GetInstance();scenes->SetSceneFogColor({0.62f,0.72f,0.83f,1});
 scenes->SetSceneDistanceFog({45,110,0.45f,1.2f});scenes->AddPostEffect(PostEffectType::Fog,PostEffectStage::BeforeParticle);
 Start(Phase::Title);
 verification=std::wstring(GetCommandLineW()).find(L"--verify-almarun")!=std::wstring::npos;
}
void AlmaRunScene::Finalize(){
 ::DirectXCommon::GetInstance()->WaitForGPU();
 uiDraws.clear();title.reset();game.reset();over.reset();clear.reset();
 AlmaBitmapFont::GetInstance()->Finalize();
 PrimitiveDrawer::GetInstance()->Finalize();
 Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}
void AlmaRunScene::Update(){
 // Opt-in smoke run exercises every scene using the normal update/draw path.
 if(verification){
  ++verificationFrame;
  switch(verificationFrame){
  case 60: Start(Phase::Game);break;
  case 360: Start(Phase::Over);break;
  case 480: Start(Phase::Game);break;
  case 720: Start(Phase::Clear);break;
  case 840: Start(Phase::Title);break;
  case 960: Start(Phase::Game);break;
  case 1200: Logger::Log("[AlmaRun] Verification complete: 1200 frames");Logger::Flush();PostQuitMessage(0);break;
  }
  if(verificationFrame==120 || verificationFrame==180 || verificationFrame==240 || verificationFrame==600 || verificationFrame==810){
   ScreenshotManager::GetInstance()->RequestCapture(true,false);
  }
 }
 if(::Input::GetInstance()->IsKeyTrigger(DIK_F6)){enhanced=!enhanced;}
 if(title){title->Update();if(title->IsFinished())Start(Phase::Game);}
 else if(game){game->Update();if(game->IsFinished()){auto next=game->IsReturnToTitle()?Phase::Title:game->IsClear()?Phase::Clear:Phase::Over;Start(next);}}
 else if(over){over->Update();if(over->IsFinished()){auto next=over->GetResult()==AlmaGameOverScene::AlmaResult::Retry?Phase::Game:Phase::Title;Start(next);}}
 else if(clear){clear->Update();if(clear->IsFinished())Start(Phase::Title);}
 AlmaAudio::GetInstance()->Flush();
}
void AlmaRunScene::Draw3D(){uiDraws.clear(); PrimitiveDrawer::GetInstance()->Reset(); AlmaModel::BeginFrame(); AlmaSprite::BeginFrame();if(title)title->Draw();else if(game)game->Draw();else if(over)over->Draw();else if(clear)clear->Draw();}
void AlmaRunScene::Draw2D(){for(auto& draw:uiDraws)draw();uiDraws.clear();}
