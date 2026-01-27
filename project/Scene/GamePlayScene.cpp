#include "GamePlayScene.h"
#include "../Animation/AnimationLoder.h"
#include "../Light/LightManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "SphereObject.h"
#include <numbers>

void GamePlayScene::Initialize()
{
    // =================================================
    // Camera
    // =================================================
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, -2.0f, -20.0f });
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_);
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);
    // =================================================
    // Managers
    // =================================================
    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);
    // =================================================
    // SkinningObject3d
    // =================================================

    TextureManager::GetInstance()->LoadTexture("resources/BaseColor_Cube.png");

    // nodeLoad
    ModelManager::GetInstance()->Load("Drone/dolone.gltf");
    ModelManager::GetInstance()->Load("AnimatedCube.gltf");
    // animationskinLoad
    // skinningWalk
    ModelManager::GetInstance()->Load("walk.gltf");
    //==============
    //  OBJ
    //==============
    terrain_ = new Object3d();
    terrain_->Initialize(Object3dManager::GetInstance());
    ModelManager::GetInstance()->Load("terrain.obj");
    terrain_->SetModel(ModelManager::GetInstance()->FindModel("terrain.obj"));
    terrain_->SetTranslate({ 0.0f - 2.0f, 0.0f });
    // plane
    plane_ = new Object3d();
    plane_->Initialize(Object3dManager::GetInstance());
    ModelManager::GetInstance()->Load("plane.obj");
    plane_->SetModel(ModelManager::GetInstance()->FindModel("plane.obj"));
    plane_->SetTranslate({ 0.0f, 2.0f, 0.0f });
    // node00
    nodeObject00_ = new Object3d();
    nodeObject00_->Initialize(Object3dManager::GetInstance());
    nodeObject00_->SetModel("Drone/dolone.gltf");
  
    // =================================================
    // animationSkin
    // =================================================
    animationSkin00_ = new SkinningObject3d();
    animationSkin00_->SetModel(ModelManager::GetInstance()->FindModel("walk.gltf"));
 
  
    //  =================================================
    //  Skeleton
    //  =================================================
    // skin
    animationSkinSkeleton00_ = Skeleton::CreateSkeleton(animationSkin00_->GetRootNode());
    // animation
    skinPlay00_ = new PlayAnimation();
    skinAnimation00_ = AnimationLoder::LoadAnimationFile("resources", "walk.gltf");
    skinPlay00_->SetAnimation(&skinAnimation00_);
    skinPlay00_->SetSkeleton(&animationSkinSkeleton00_);
    animationSkin00_->SetAnimation(skinPlay00_);
    animationSkin00_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin00_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin00_->SetTranslate({ 5.0f, -2.0f, 0.0f });
    // node00
    nodeAnimation00_ = AnimationLoder::LoadAnimationFile("resources", "Drone/dolone.gltf");
    nodePlayAnim00_.SetAnimation(&nodeAnimation00_);
    nodeObject00_->SetAnimation(&nodePlayAnim00_);
    nodeObject00_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    nodeObject00_->SetScale({ 1.0f, 1.0f, 1.0f });

   
    // デバッグ（Animation）
    for (const auto& [name, nodeAnim] : skinAnimation00_.nodeAnimations) {
        OutputDebugStringA(name.c_str());
    }

    // =================================================
    // Particle
    // =================================================
    ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");

    EulerTransform t {};
    t.translate = { 0.0f, 0.0f, 0.0f };
    emitter_.Init("circle", t, 30, 0.1f);

    // =================================================
    // Debug Sphere
    // =================================================
    sphere_ = new SphereObject();
    sphere_->Initialize(DirectXCommon::GetInstance(), 16, 1.0f);
    sphere_->SetTranslate({ 0, 0, 0 });
    sphere_->SetScale({ 1.5f, 1.5f, 1.5f });
    sphere_->SetColor({ 1, 1, 1, 1 });

    // =================================================
    // Light
    // =================================================
    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    LightManager::GetInstance()->SetDirectional(
        { 1, 1, 1, 1 },
        { 0, -1, 0 },
        1.0f);

    // =================================================
    // Sound
    // =================================================
    bgm = SoundManager::GetInstance()->SoundLoadFile("Resources/BGM.wav");
    SoundManager::GetInstance()->SoundPlayWave(bgm);
}

void GamePlayScene::Update()
{

    nodePlayAnim00_.Update(1.0f / 60.0f);
    nodeObject00_->Update();
  
    // ImGuiのBegin/Endは絶対に呼ばない！
    // rotate
    r += 0.03f;
    // animationSkin
    skinPlay00_->Update(1.0f / 60.0f);
    animationSkin00_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton00_.UpdateSkeleton();
    animationSkin00_->Update();
   

    ParticleManager::GetInstance()->Update();
    emitter_.Update();
    sphere_->Update(camera_);

    terrain_->Update();
    camera_->Update();
    camera_->DebugUpdate();

    plane_->Update();

#pragma region ImGuiによるライト操作パネル
    // ==================================
    // Lighting Panel（ライト操作パネル）
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライトの ON / OFF ----
    static bool lightEnabled = true;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライトの色 ----
    static Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ） ----
    static float lightIntensity = 1.0f;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光の向き ----
    static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
    ImGui::SliderFloat3("Direction", &lightDir.x, -1.0f, 1.0f);

    // ---- 正規化 ----
    Vector3 normalizedDir = Normalize(lightDir);

    float intensity = lightIntensity;
    if (!lightEnabled) {
        intensity = 0.0f; // OFF のときは光なし
    }

    LightManager::GetInstance()->SetDirectional(
        { lightColor.x, lightColor.y, lightColor.z, 1.0f },
        normalizedDir,
        intensity);

    // ---- リセットボタン（向きだけ元に戻す）----
    if (ImGui::Button("Reset Direction")) {
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::SameLine();

    // ---- ライトを完全初期化 ----
    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        lightIntensity = 1.0f;
        lightDir = { 0.0f, -1.0f, 0.0f };
    }
    ImGui::Separator();
    ImGui::Text("Point Light Control");

    static bool pointEnabled = true;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    static Vector3 pointPos = { 0.0f, 2.0f, 0.0f };
    ImGui::SliderFloat3("Point Position", &pointPos.x, -10.0f, 10.0f);

    static float pointIntensity = 1.0f;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    float pI = pointEnabled ? pointIntensity : 0.0f;
    static float pointRadius = 10.0f;
    static float pointDecay = 1.0f;

    ImGui::SliderFloat("Point Radius", &pointRadius, 0.1f, 30.0f);
    ImGui::SliderFloat("Point Decay", &pointDecay, 0.1f, 5.0f);

    LightManager::GetInstance()->SetPointRadius(pointRadius);
    LightManager::GetInstance()->SetPointDecay(pointDecay);
    LightManager::GetInstance()->SetPointLight(pointColor, pointPos, pI);
    ImGui::Separator();
    ImGui::Text("Spot Light Control");
    // ================================
    // Spot Light Control
    // ================================

    static bool spotEnabled = true;
    ImGui::Checkbox("Enable Spot Light", &spotEnabled);

    // 色
    static Vector4 spotColor = { 1, 1, 1, 1 };
    ImGui::ColorEdit3("Spot Color", (float*)&spotColor);

    // 位置
    static Vector3 spotPos = { 2.0f, 1.25f, 0.0f };
    ImGui::SliderFloat3("Spot Position", &spotPos.x, -10.0f, 10.0f);

    // 方向
    static Vector3 spotDir = { -1.0f, -1.0f, 0.0f };
    ImGui::SliderFloat3("Spot Direction", &spotDir.x, -1.0f, 1.0f);
    Vector3 normalizedSpotDir = Normalize(spotDir);

    // 強さ
    static float spotIntensity = 4.0f;
    ImGui::SliderFloat("Spot Intensity", &spotIntensity, 0.0f, 10.0f);

    // 距離・減衰
    static float spotDistance = 7.0f;
    static float spotDecay = 2.0f;
    ImGui::SliderFloat("Spot Distance", &spotDistance, 0.1f, 30.0f);
    ImGui::SliderFloat("Spot Decay", &spotDecay, 0.1f, 5.0f);

    // 角度（度数で操作 → cos に変換）
    static float spotAngleDeg = 60.0f;
    static float spotFalloffStartDeg = 30.0f;

    ImGui::SliderFloat("Spot Angle (deg)", &spotAngleDeg, 1.0f, 90.0f);
    ImGui::SliderFloat("Falloff Start (deg)", &spotFalloffStartDeg, 1.0f, spotAngleDeg - 1.0f);

    // cos に変換
    float cosAngle = std::cos(spotAngleDeg * std::numbers::pi_v<float> / 180.0f);
    float cosFalloffStart = std::cos(spotFalloffStartDeg * std::numbers::pi_v<float> / 180.0f);

    // OFF のとき
    float sI = spotEnabled ? spotIntensity : 0.0f;

    // LightManager に反映
    auto* lm = LightManager::GetInstance();
    lm->SetSpotLightColor(spotColor);
    lm->SetSpotLightPosition(spotPos);
    lm->SetSpotLightDirection(normalizedSpotDir);
    lm->SetSpotLightIntensity(sI);
    lm->SetSpotLightDistance(spotDistance);
    lm->SetSpotLightDecay(spotDecay);
    lm->SetSpotLightCosAngle(cosAngle);
    lm->SetSpotLightCosFalloffStart(cosFalloffStart);

    ImGui::End();

    // ==================================
    // Sphere Control
    // ==================================
    ImGui::Begin("Sphere Control");

    // ---- このオブジェクトだけ ライティングする？ ----
    // OFF にすると「フラット表示」になる
    ImGui::Checkbox("Enable Lighting", &sphereLighting);

    // ---- 位置 ----
    ImGui::SliderFloat3("Position", &spherePos.x, -10.0f, 10.0f);

    // ---- 回転 ----
    ImGui::SliderFloat3("Rotate", &sphereRotate.x, -3.14f, 3.14f);

    ImGui::SliderFloat3("Scale", &sphereScale.x, 1.0f, 10.0f);
    // ---- テカり具合（鏡面反射の鋭さ） ----
    static float shininess = 32.0f;
    ImGui::SliderFloat("Shininess", &shininess, 1.0f, 128.0f);

    ImGui::End();

  
    ImGui::Begin("Plane Control");

    // 位置
    ImGui::SliderFloat3("Position", &planePos.x, -50.0f, 50.0f);

    // 回転（ラジアン）
    ImGui::SliderFloat3("Rotate", &planeRotate.x, -3.14f, 3.14f);

    // スケール
    ImGui::SliderFloat3("Scale", &planeScale.x, 0.1f, 10.0f);

    ImGui::End();
    // 反映
    plane_->SetTranslate(planePos);
    plane_->SetRotate(planeRotate);
    plane_->SetScale(planeScale);
    terrain_->SetTranslate(terrainPos);
    terrain_->SetRotate(terrainRotate);
    terrain_->SetScale(terrainScale);
    sphere_->SetEnableLighting(sphereLighting);
    sphere_->SetTranslate(spherePos);
    sphere_->SetRotate(sphereRotate);
    sphere_->SetScale(sphereScale);
    sphere_->SetShininess(shininess);

#pragma endregion
}

void GamePlayScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());

    //sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    //terrain_->Draw();

   // plane_->Draw();
   // nodeObject00_->Draw();


    //----------------------
    // スキニング
    //----------------------
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList()); // ここでもう一回バインドしないといけない
    animationSkin00_->Draw();

    ParticleManager::GetInstance()->PreDraw();
    ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI

#endif
}

void GamePlayScene::Finalize()
{
    ParticleManager::GetInstance()->Finalize();

    LightManager::GetInstance()->Finalize();

    delete sprite_;
    sprite_ = nullptr;

    delete sphere_;
    sphere_ = nullptr;

    delete player2_;
    player2_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    delete terrain_;
    terrain_ = nullptr;

    delete plane_;
    plane_ = nullptr;
    delete nodeObject00_;
    nodeObject00_ = nullptr;
 

    delete animationSkin00_;
    animationSkin00_ = nullptr;

    delete skinningPlayer_;
    skinningPlayer_ = nullptr;
    SoundManager::GetInstance()->SoundUnload(&bgm);
}
