#include "GamePlayScene.h"
#include "../Light/LightManager.h"
#include "ParticleManager.h"
#include "SphereObject.h"
#include <numbers>
#include"../Animation/AnimationLoder.h"
#include "SoundManager.h"
void GamePlayScene::Initialize()
{
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate(Vector3 { 0.0f, 0.0f, -20.0f });

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    sprite_ = new Sprite();
    sprite_->Initialize(SpriteManager::GetInstance(), "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });

    player2_ = new Object3d();
    player2_->Initialize(Object3dManager::GetInstance());
    player2_->SetModel("AnimatedCube.gltf");
    player2_->SetTranslate({ 3.0f, 0.0f, 0.0f });
   
    playAnim_ = new PlayAnimation();
    
    // メンバ変数に直接ロード
    animation_ = AnimationLoder::LoadAnimationFile("resources", "AnimatedCube.gltf");

    // 再生器に渡す（寿命OK）
    playAnim_->SetAnimation(&animation_);

    // Object3d に紐づけ
    player2_->SetAnimation(playAnim_);
    // player2_->SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });

    ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");
    EulerTransform t {};
    t.translate = { 0.0f, 0.0f, 0.0f };

    emitter_.Init("circle", t, 30, 0.1f);

    sphere_ = new SphereObject();
    sphere_->Initialize(DirectXCommon::GetInstance(), 16, 1.0f);

    // Transform
    sphere_->SetTranslate({ 0, 0, 0 }); // 消える？
    sphere_->SetScale({ 1.5f, 1.5f, 1.5f });

    // Material
    sphere_->SetColor({ 1, 1, 1, 1 });
    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);

    bgm = SoundManager::GetInstance()->SoundLoadFile("Resources/BGM.wav");

    SoundManager::GetInstance()->SoundPlayWave(bgm);
}

void GamePlayScene::Update()
{
    playAnim_->Update(1.0f / 60.0f);
    // ImGuiのBegin/Endは絶対に呼ばない！
    emitter_.Update();
    ParticleManager::GetInstance()->Update();
    player2_->Update();
    sprite_->Update();
    sphere_->Update(camera_);
    camera_->Update();
    camera_->DebugUpdate();
  

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

    // 反映
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
    player2_->Draw();
    sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    ParticleManager::GetInstance()->PreDraw();
    ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    sprite_->Draw();
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

    SoundManager::GetInstance()->SoundUnload(&bgm);
}
