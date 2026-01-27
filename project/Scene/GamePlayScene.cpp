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
    ModelManager::GetInstance()->Load("Animation_Node_01.gltf");
    ModelManager::GetInstance()->Load("Animation_Node_02.gltf");
    ModelManager::GetInstance()->Load("Animation_Node_03.gltf");
    ModelManager::GetInstance()->Load("Animation_Node_04.gltf");
    ModelManager::GetInstance()->Load("Animation_Node_05.gltf");
    // animationskinLoad
    ModelManager::GetInstance()->Load("Drone/dolone.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_01.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_02.gltf");
    /*  ModelManager::GetInstance()->Load("Animation_Skin_03.gltf");*/
    /* ModelManager::GetInstance()->Load("Animation_Skin_04.gltf");*/
    ModelManager::GetInstance()->Load("Animation_Skin_05.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_06.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_07.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_08.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_09.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_10.gltf");
    ModelManager::GetInstance()->Load("Animation_Skin_11.gltf");
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
    nodeObject00_->SetModel("AnimatedCube.gltf");
    // node01
    nodeObject01_ = new Object3d();
    nodeObject01_->Initialize(Object3dManager::GetInstance());
    nodeObject01_->SetModel("Animation_Node_01.gltf");
    // node02
    nodeObject02_ = new Object3d();
    nodeObject02_->Initialize(Object3dManager::GetInstance());
    nodeObject02_->SetModel("Animation_Node_02.gltf");
    // node03
    nodeObject03_ = new Object3d();
    nodeObject03_->Initialize(Object3dManager::GetInstance());
    nodeObject03_->SetModel("Animation_Node_03.gltf");
    // node04
    nodeObject04_ = new Object3d();
    nodeObject04_->Initialize(Object3dManager::GetInstance());
    nodeObject04_->SetModel("Animation_Node_04.gltf");
    // node05
    nodeObject05_ = new Object3d();
    nodeObject05_->Initialize(Object3dManager::GetInstance());
    nodeObject05_->SetModel("Animation_Node_05.gltf");
    // =================================================
    // animationSkin
    // =================================================
    animationSkin00_ = new SkinningObject3d();
    animationSkin00_->SetModel(ModelManager::GetInstance()->FindModel("Drone/dolone.gltf"));
    animationSkin01_ = new SkinningObject3d();
    animationSkin01_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_01.gltf"));
    animationSkin02_ = new SkinningObject3d();
    animationSkin02_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_02.gltf"));
    /* animationSkin03_ = new SkinningObject3d();
     animationSkin03_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_03.gltf"));*/
    // animationSkin04_ = new SkinningObject3d();
    // animationSkin04_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_04.gltf"));
    animationSkin05_ = new SkinningObject3d();
    animationSkin05_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_05.gltf"));
    animationSkin06_ = new SkinningObject3d();
    animationSkin06_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_06.gltf"));
    animationSkin07_ = new SkinningObject3d();
    animationSkin07_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_07.gltf"));
    animationSkin08_ = new SkinningObject3d();
    animationSkin08_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_08.gltf"));
    /* animationSkin09_ = new SkinningObject3d();
     animationSkin09_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_09.gltf"));*/
    animationSkin10_ = new SkinningObject3d();
    animationSkin10_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_10.gltf"));
    animationSkin11_ = new SkinningObject3d();
    animationSkin11_->SetModel(ModelManager::GetInstance()->FindModel("Animation_Skin_11.gltf"));

    skinningPlayer_ = new SkinningObject3d();
    skinningPlayer_->SetModel(ModelManager::GetInstance()->FindModel("walk.gltf"));

    //  =================================================
    //  Skeleton
    //  =================================================
    // skin
    animationSkinSkeleton00_ = Skeleton::CreateSkeleton(animationSkin00_->GetRootNode());
    // animation
    skinPlay00_ = new PlayAnimation();
    skinAnimation00_ = AnimationLoder::LoadAnimationFile("resources", "Drone/dolone.gltf");
    skinPlay00_->SetAnimation(&skinAnimation00_);
    skinPlay00_->SetSkeleton(&animationSkinSkeleton00_);
    animationSkin00_->SetAnimation(skinPlay00_);
    animationSkin00_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin00_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin00_->SetTranslate({ 5.0f, -2.0f, 0.0f });

    // skin
    animationSkinSkeleton01_ = Skeleton::CreateSkeleton(animationSkin01_->GetRootNode());
    // animation
    skinPlay01_ = new PlayAnimation();
    skinAnimation01_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_01.gltf");
    skinPlay01_->SetAnimation(&skinAnimation01_);
    skinPlay01_->SetSkeleton(&animationSkinSkeleton01_);
    animationSkin01_->SetAnimation(skinPlay01_);
    animationSkin01_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin01_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin01_->SetTranslate({ 10.0f, -2.0f, 0.0f });
    // skin
    animationSkinSkeleton02_ = Skeleton::CreateSkeleton(animationSkin02_->GetRootNode());
    // animation
    skinPlay02_ = new PlayAnimation();
    skinAnimation02_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_02.gltf");
    skinPlay02_->SetAnimation(&skinAnimation02_);
    skinPlay02_->SetSkeleton(&animationSkinSkeleton02_);
    animationSkin02_->SetAnimation(skinPlay02_);
    animationSkin02_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin02_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin02_->SetTranslate({ 15.0f, -2.0f, 0.0f });
    // skin
    animationSkinSkeleton05_ = Skeleton::CreateSkeleton(animationSkin05_->GetRootNode());
    // animation
    skinPlay05_ = new PlayAnimation();
    skinAnimation05_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_05.gltf");
    skinPlay05_->SetAnimation(&skinAnimation05_);
    skinPlay05_->SetSkeleton(&animationSkinSkeleton05_);
    animationSkin05_->SetAnimation(skinPlay05_);
    animationSkin05_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin05_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin05_->SetTranslate({ 30.0f, -2.0f, 0.0f });
    // skin
    animationSkinSkeleton06_ = Skeleton::CreateSkeleton(animationSkin06_->GetRootNode());
    // animation
    skinPlay06_ = new PlayAnimation();
    skinAnimation06_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_06.gltf");
    skinPlay06_->SetAnimation(&skinAnimation06_);
    skinPlay06_->SetSkeleton(&animationSkinSkeleton06_);
    animationSkin06_->SetAnimation(skinPlay06_);
    animationSkin06_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin06_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin06_->SetTranslate({ 35.0f, -2.0f, 0.0f });
    // skin
    animationSkinSkeleton07_ = Skeleton::CreateSkeleton(animationSkin07_->GetRootNode());
    // animation
    skinPlay07_ = new PlayAnimation();
    skinAnimation07_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_07.gltf");
    skinPlay07_->SetAnimation(&skinAnimation07_);
    skinPlay07_->SetSkeleton(&animationSkinSkeleton07_);
    animationSkin07_->SetAnimation(skinPlay07_);
    animationSkin07_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin07_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin07_->SetTranslate({ 40.0f, -2.0f, 0.0f });

    // skin
    animationSkinSkeleton08_ = Skeleton::CreateSkeleton(animationSkin08_->GetRootNode());
    // animation
    skinPlay08_ = new PlayAnimation();
    skinAnimation08_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_08.gltf");
    skinPlay08_->SetAnimation(&skinAnimation08_);
    skinPlay08_->SetSkeleton(&animationSkinSkeleton08_);
    animationSkin08_->SetAnimation(skinPlay08_);
    animationSkin08_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin08_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin08_->SetTranslate({ 40.0f, -2.0f, 0.0f });

    //// skin
    // animationSkinSkeleton09_ = Skeleton::CreateSkeleton(animationSkin09_->GetRootNode());
    //// animation
    // skinPlay09_ = new PlayAnimation();
    // skinAnimation09_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_09.gltf");
    // skinPlay09_->SetAnimation(&skinAnimation09_);
    // skinPlay09_->SetSkeleton(&animationSkinSkeleton09_);
    // animationSkin09_->SetAnimation(skinPlay09_);
    // animationSkin09_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    // animationSkin09_->Initialize(SkinningObject3dManager::GetInstance());
    // animationSkin09_->SetTranslate({ 45.0f, -2.0f, 0.0f });

    // skin
    animationSkinSkeleton10_ = Skeleton::CreateSkeleton(animationSkin10_->GetRootNode());
    // animation
    skinPlay10_ = new PlayAnimation();
    skinAnimation10_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_08.gltf");
    skinPlay10_->SetAnimation(&skinAnimation10_);
    skinPlay10_->SetSkeleton(&animationSkinSkeleton10_);
    animationSkin10_->SetAnimation(skinPlay10_);
    animationSkin10_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin10_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin10_->SetTranslate({ 50.0f, -2.0f, 0.0f });
    // skin
    animationSkinSkeleton11_ = Skeleton::CreateSkeleton(animationSkin11_->GetRootNode());
    // animation
    skinPlay11_ = new PlayAnimation();
    skinAnimation11_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Skin_11.gltf");
    skinPlay11_->SetAnimation(&skinAnimation11_);
    skinPlay11_->SetSkeleton(&animationSkinSkeleton11_);
    animationSkin11_->SetAnimation(skinPlay11_);
    animationSkin11_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    animationSkin11_->Initialize(SkinningObject3dManager::GetInstance());
    animationSkin11_->SetTranslate({ 55.0f, -2.0f, 0.0f });

    // skin
    skinningSkeleton_ = Skeleton::CreateSkeleton(skinningPlayer_->GetRootNode());
    // animation
    skinningPlay_ = new PlayAnimation();
    skinningAnimation_ = AnimationLoder::LoadAnimationFile("resources", "walk.gltf");
    skinningPlay_->SetAnimation(&skinningAnimation_);
    skinningPlay_->SetSkeleton(&skinningSkeleton_);
    skinningPlayer_->SetAnimation(skinningPlay_);
    skinningPlayer_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    skinningPlayer_->Initialize(SkinningObject3dManager::GetInstance());
    skinningPlayer_->SetTranslate({ 0.0f, 3.0f, 0.0f });
    // node00
    nodeAnimation00_ = AnimationLoder::LoadAnimationFile("resources", "Drone/dolone.gltf");
    nodePlayAnim00_.SetAnimation(&nodeAnimation00_);
    nodeObject00_->SetAnimation(&nodePlayAnim00_);
    nodeObject00_->SetTranslate({ 5.0f, 0.0f, 0.0f });
    nodeObject00_->SetScale({ 1.0f, 1.0f, 1.0f });

    // node01
    nodeAnimation01_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Node_01.gltf");
    nodePlayAnim01_.SetAnimation(&nodeAnimation01_);
    nodeObject01_->SetAnimation(&nodePlayAnim01_);
    nodeObject01_->SetTranslate({ 10.0f, 0.0f, 0.0f });
    nodeObject01_->SetScale({ 1.0f, 1.0f, 1.0f });
    // node02
    nodeAnimation02_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Node_02.gltf");
    nodePlayAnim02_.SetAnimation(&nodeAnimation02_);
    nodeObject02_->SetAnimation(&nodePlayAnim02_);
    nodeObject02_->SetTranslate({ 15.0f, 0.0f, 0.0f });
    nodeObject02_->SetScale({ 1.0f, 1.0f, 1.0f });
    // node03
    nodeAnimation03_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Node_03.gltf");
    nodePlayAnim03_.SetAnimation(&nodeAnimation03_);
    nodeObject03_->SetAnimation(&nodePlayAnim03_);
    nodeObject03_->SetTranslate({ 20.0f, 0.0f, 0.0f });
    nodeObject03_->SetScale({ 1.0f, 1.0f, 1.0f });
    // node04
    nodeAnimation04_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Node_04.gltf");
    nodePlayAnim04_.SetAnimation(&nodeAnimation04_);
    nodeObject04_->SetAnimation(&nodePlayAnim04_);
    nodeObject04_->SetTranslate({ 25.0f, 0.0f, 0.0f });
    nodeObject04_->SetScale({ 1.0f, 1.0f, 1.0f });
    // node05
    nodeAnimation05_ = AnimationLoder::LoadAnimationFile("resources", "Animation_Node_05.gltf");
    nodePlayAnim05_.SetAnimation(&nodeAnimation05_);
    nodeObject05_->SetAnimation(&nodePlayAnim05_);
    nodeObject05_->SetTranslate({ 30.0f, 0.0f, 0.0f });
    nodeObject05_->SetScale({ 1.0f, 1.0f, 1.0f });
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
    nodePlayAnim01_.Update(1.0f / 60.0f);
    nodeObject00_->Update();
    nodeObject01_->Update();
    nodePlayAnim02_.Update(1.0f / 60.0f);
    nodeObject02_->Update();
    nodePlayAnim03_.Update(1.0f / 60.0f);
    nodeObject03_->Update();
    nodePlayAnim04_.Update(1.0f / 60.0f);
    nodeObject04_->Update();
    nodePlayAnim05_.Update(1.0f / 60.0f);
    nodeObject05_->Update();
    // ImGuiのBegin/Endは絶対に呼ばない！
    // rotate
    r += 0.03f;
    // animationSkin
    skinPlay00_->Update(1.0f / 60.0f);
    animationSkin00_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton00_.UpdateSkeleton();
    animationSkin00_->Update();
    skinPlay01_->Update(1.0f / 60.0f);
    animationSkin01_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton01_.UpdateSkeleton();
    animationSkin01_->Update();
    skinPlay02_->Update(1.0f / 60.0f);
    animationSkin02_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton02_.UpdateSkeleton();
    animationSkin02_->Update();
    skinPlay05_->Update(1.0f / 60.0f);
    animationSkin05_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton05_.UpdateSkeleton();
    animationSkin05_->Update();

    skinPlay06_->Update(1.0f / 60.0f);
    animationSkin06_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton06_.UpdateSkeleton();
    animationSkin06_->Update();

    skinPlay07_->Update(1.0f / 60.0f);
    animationSkin07_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton07_.UpdateSkeleton();
    animationSkin07_->Update();

    skinPlay08_->Update(1.0f / 60.0f);
    animationSkin08_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton08_.UpdateSkeleton();
    animationSkin08_->Update();

    /* skinPlay09_->Update(1.0f / 60.0f);
     animationSkin09_->SetRotate({ 0.0f, r, 0.0f });
     animationSkinSkeleton09_.UpdateSkeleton();
     animationSkin09_->Update();*/

    skinPlay10_->Update(1.0f / 60.0f);
    animationSkin10_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton10_.UpdateSkeleton();
    animationSkin10_->Update();

    skinPlay11_->Update(1.0f / 60.0f);
    animationSkin11_->SetRotate({ 0.0f, r, 0.0f });
    animationSkinSkeleton11_.UpdateSkeleton();
    animationSkin11_->Update();

    skinningPlay_->Update(1.0f / 60.0f);
    skinningSkeleton_.UpdateSkeleton();
    skinningPlayer_->Update();

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

    //plane_->Draw();
   // nodeObject00_->Draw();
    //nodeObject01_->Draw();
    //nodeObject02_->Draw();
    //nodeObject03_->Draw();
    //nodeObject04_->Draw();
    //nodeObject05_->Draw();

    //----------------------
    // スキニング
    //----------------------
    SkinningObject3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList()); // ここでもう一回バインドしないといけない
    animationSkin00_->Draw();
    //animationSkin01_->Draw();
    //animationSkin02_->Draw();
    //animationSkin05_->Draw();
    //animationSkin06_->Draw();
    //animationSkin07_->Draw();
    //animationSkin08_->Draw();
    ///* animationSkin09_->Draw();*/
    //animationSkin10_->Draw();
    //animationSkin11_->Draw();
    //skinningPlayer_->Draw();
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
    delete nodeObject01_;
    nodeObject01_ = nullptr;
    delete nodeObject02_;
    nodeObject01_ = nullptr;
    delete nodeObject03_;
    nodeObject03_ = nullptr;
    delete nodeObject04_;
    nodeObject04_ = nullptr;
    delete nodeObject05_;
    nodeObject05_ = nullptr;

    delete animationSkin00_;
    animationSkin00_ = nullptr;

    delete animationSkin01_;
    animationSkin01_ = nullptr;
    delete animationSkin02_;
    animationSkin02_ = nullptr;
    delete animationSkin05_;
    animationSkin05_ = nullptr;
    delete animationSkin06_;
    animationSkin06_ = nullptr;
    delete animationSkin07_;
    animationSkin07_ = nullptr;
    delete animationSkin08_;
    animationSkin08_ = nullptr;
    /* delete animationSkin09_;
     animationSkin09_ = nullptr;*/
    delete animationSkin10_;
    animationSkin10_ = nullptr;
    delete animationSkin11_;
    animationSkin11_ = nullptr;
    delete skinningPlayer_;
    skinningPlayer_ = nullptr;
    SoundManager::GetInstance()->SoundUnload(&bgm);
}
