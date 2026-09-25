#include "StageSelectScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "PageTransition.h"
#include "TestScene1.h"
#include "SpriteTestScene.h"
#include "TextTestScene.h"
#include "TitleScene.h"
#include "LoadingScene.h"
#include "App/Game/Stage/StageCatalog.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/Light/LightManager.h"
#include <format>
#include <cmath>
#include <algorithm>
#include <numbers>

namespace {
constexpr const char* kDefaultFont ="resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kStageCardModel ="StageSelectBook/StageCard.obj";
constexpr const char* kPageTurnSoundName = "StageSelect.PageTurn";
constexpr const char* kPageFlipSoundName = "StageSelect.PageFlip";
constexpr const char* kPageRiffleSoundName = "StageSelect.PageRiffle";
constexpr const char* kConfirmSoundName = "StageSelect.Confirm";
constexpr const char* kBackSoundName = "StageSelect.Back";
constexpr const char* kConfirmSoundPath = "resources/Audio/StageSelect/confirm.wav";
constexpr const char* kBackSoundPath = "resources/Audio/StageSelect/back.wav";
constexpr const char* kTerminalMoveSoundPath = "resources/Sounds/maou_se_system49.mp3";

constexpr float kCameraApproachDuration = 2.4f;
constexpr float kTitleReturnDuration = 2.0f;
constexpr float kConfirmSceneChangeTime = 1.15f;
const Vector3 kCameraStartEye = { 0.0f, 3.8f, -35.0f };
const Vector3 kCameraEndEye = { 0.75f, 1.65f, -16.5f };
const Vector3 kConfirmCameraEye = { 0.20f, 0.80f, -7.0f };
const Vector3 kCameraStartTarget = { 0.0f, -1.0f, 1.0f };
const Vector3 kCameraEndTarget = { 0.0f, -0.20f, 0.0f };
const Vector3 kConfirmCameraTarget = { 0.0f, -0.10f, 0.0f };

constexpr float kCardOpenDuration = 0.42f;
constexpr float kCardCloseDuration = 0.22f;
constexpr float kPageTurnDuration = 0.62f;
constexpr float kCardRestY = 0.35f;
constexpr float kCardHiddenY = -2.35f;
constexpr uint32_t kTurningPageStripCount = 20;
constexpr uint32_t kOpeningPageCount = 18;
constexpr uint32_t kOpeningPageStripCount = 1;
constexpr uint32_t kDustMoteCount = 52;

}

void StageSelectScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::NeonGlow);
    SceneManager::GetInstance()->SetArchiveApproach(0.0f);

    ShowCursor(TRUE);
    ClipCursor(nullptr);
    SceneManager::GetInstance()->RemovePostEffect(PostEffectType::Fog);
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt(kCameraStartEye, kCameraStartTarget);
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    InitializeStageData();
    InitializeBookObjects();
    InitializeTurningPage();
    InitializeOpeningPages();
    InitializeInterface();
    InitializeDustMotes();

    audio_.Initialize();
    audio_.Load(kPageTurnSoundName, kTerminalMoveSoundPath);
    audio_.Load(kPageFlipSoundName, kTerminalMoveSoundPath);
    audio_.Load(kPageRiffleSoundName, kTerminalMoveSoundPath);
    audio_.Load(kConfirmSoundName, kConfirmSoundPath);
    audio_.Load(kBackSoundName, kBackSoundPath);

    RefreshStageText();
    InitializeTransition();
    StartArchiveApproach();
    UpdateSceneObjects();
}

void StageSelectScene::Finalize()
{
audio_.Stop();
    auto* objects = Object3dManager::GetInstance();
    if (objects->GetDefaultCamera() == camera_.get()) {
        objects->SetDefaultCamera(nullptr);
    }
}

void StageSelectScene::InitializeStageData()
{
auto* catalog = StageCatalog::GetInstance();
    const bool loaded = catalog->Load();
    stages_.clear();
    size_t number = 0;
    if (loaded) {
        for (const auto& stage : catalog->GetStages()) {
            if (stage.id == "gimmick_test") continue;
            stages_.push_back({ std::format("MISSION {:02}  {}", ++number, stage.name),
                stage.description, StageDestination::GamePlay, stage.id });
        }
    }
    if (stages_.empty()) {
        stages_.push_back({ "NO STAGES - RETURN TO TITLE",
            loaded ? "The stage catalog is empty." : "Unable to load the stage catalog.",
            StageDestination::Title, {} });
    }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    stages_.push_back({ "GAME TEST / F1", "Open the gameplay test scene.", StageDestination::GameTest, {} });
    stages_.push_back({ "SPRITE TEST / F2", "Open the sprite test scene.", StageDestination::SpriteTest, {} });
    stages_.push_back({ "TEXT TEST / F3", "Open the text test scene.", StageDestination::TextTest, {} });
    if (loaded) {
        if (const auto* gimmick = catalog->Find("gimmick_test")) {
            stages_.push_back({ "GIMMICK TEST", gimmick->description, StageDestination::GamePlay, gimmick->id });
        }
    }
#endif
    stages_.push_back({ "RETURN TO TITLE", "Return to the title screen.", StageDestination::Title, {} });
}

void StageSelectScene::InitializeBookObjects()
{
    Object3dManager* objectManager = Object3dManager::GetInstance();
    ModelManager* modelManager = ModelManager::GetInstance();

    backdrop_ = std::make_unique<Object3d>();
    backdrop_->Initialize(objectManager);
    backdrop_->SetModel(modelManager->CreateCube(kWhiteTexture));
    backdrop_->SetScale({ 18.0f, 10.0f, 0.45f });
    backdrop_->SetTranslate({ 0.0f, 0.5f, 8.0f });
    backdrop_->SetEnableLighting(false);
    backdrop_->SetColor({ 0.008f, 0.018f, 0.045f, 1.0f });

    auto createStructure = [&](const Vector3& position, const Vector3& scale,
                               const Vector4& color) {
        auto object = std::make_unique<Object3d>();
        object->Initialize(objectManager);
        object->SetModel(modelManager->CreateCube(kWhiteTexture));
        object->SetTranslate(position);
        object->SetScale(scale);
        object->SetColor(color);
        object->SetEnableLighting(false);
        object->Update();
        hangarStructures_.push_back(std::move(object));
    };

    createStructure({ 0.0f, -5.2f, 1.5f }, { 18.0f, 0.22f, 15.0f }, { 0.018f, 0.035f, 0.075f, 1.0f });
    createStructure({ 0.0f, 7.2f, 2.0f }, { 18.0f, 0.20f, 15.0f }, { 0.012f, 0.025f, 0.055f, 1.0f });
    createStructure({ -16.0f, 1.0f, 2.0f }, { 0.28f, 7.0f, 14.0f }, { 0.025f, 0.070f, 0.115f, 1.0f });
    createStructure({ 16.0f, 1.0f, 2.0f }, { 0.28f, 7.0f, 14.0f }, { 0.025f, 0.070f, 0.115f, 1.0f });
    for (int32_t side : { -1, 1 }) {
        for (int32_t index = 0; index < 5; ++index) {
            createStructure(
                { static_cast<float>(side) * (3.2f + index * 2.4f), -4.88f, 0.0f },
                { 0.055f, 0.035f, 10.0f },
                { 0.05f, 0.75f, 1.0f, 1.0f });
        }
    }

    hangarShip_ = std::make_unique<Object3d>();
    hangarShip_->Initialize(objectManager);
    hangarShip_->SetModel(modelManager->Load("Characters/Player/AirPlane/AirPlane.obj"));
    hangarShip_->SetScale({ 1.55f, 1.55f, 1.55f });
    hangarShip_->SetRotate({ 0.0f, -1.05f, 0.03f });
    hangarShip_->SetTranslate({ 8.2f, -2.1f, 2.4f });
    hangarShip_->SetEnableLighting(true);
    hangarShip_->Update();

    LightManager::GetInstance()->SetDirectional(
        { 0.45f, 0.75f, 1.0f, 1.0f },
        { -0.25f, -0.75f, 0.55f },
        1.4f);

    for (auto* cover : { &leftBookCover_, &rightBookCover_ }) {
        *cover = std::make_unique<Object3d>();
        (*cover)->Initialize(objectManager);
        (*cover)->SetModel(modelManager->CreateCube(kWhiteTexture));
        (*cover)->SetEnableLighting(false);
        (*cover)->SetColor({ 0.025f, 0.075f, 0.13f, 1.0f });
    }

    for (uint32_t index = 0; index < 8; ++index) {
        auto fitting = std::make_unique<Object3d>();
        fitting->Initialize(objectManager);
        fitting->SetModel(modelManager->CreateCube(kWhiteTexture));
        fitting->SetColor({ 0.08f, 0.82f, 1.0f, 1.0f });
        fitting->SetEnableLighting(false);
        bookFittings_.push_back(std::move(fitting));
    }

    bookSpine_ = std::make_unique<Object3d>();
    bookSpine_->Initialize(objectManager);
    bookSpine_->SetModel(modelManager->CreateCube(kWhiteTexture));
    bookSpine_->SetColor({ 0.10f, 0.85f, 1.0f, 1.0f });
    bookSpine_->SetEnableLighting(false);

    leftPageBlock_ = std::make_unique<Object3d>();
    leftPageBlock_->Initialize(objectManager);
    leftPageBlock_->SetModel(modelManager->CreateCube(kWhiteTexture));
    leftPageBlock_->SetColor({ 0.015f, 0.16f, 0.23f, 0.86f });
    leftPageBlock_->SetEnableLighting(false);

    rightPageBlock_ = std::make_unique<Object3d>();
    rightPageBlock_->Initialize(objectManager);
    rightPageBlock_->SetModel(modelManager->CreateCube(kWhiteTexture));
    rightPageBlock_->SetColor({ 0.015f, 0.16f, 0.23f, 0.86f });
    rightPageBlock_->SetEnableLighting(false);

    stageCard_ = std::make_unique<Object3d>();
    stageCard_->Initialize(objectManager);
    stageCard_->SetModel(modelManager->Load(kStageCardModel));
    stageCard_->SetScale({ 3.85f, 2.30f, 0.18f });
    stageCard_->SetTranslate({ 0.0f, kCardHiddenY, -0.70f });
    stageCard_->SetColor({ 0.025f, 0.42f, 0.62f, 0.92f });
    stageCard_->SetEnableLighting(false);

    stageCardShadow_ = std::make_unique<Object3d>();
    stageCardShadow_->Initialize(objectManager);
    stageCardShadow_->SetModel(modelManager->Load(kStageCardModel));
    stageCardShadow_->SetScale({ 4.05f, 2.42f, 0.12f });
    stageCardShadow_->SetTranslate({ 0.12f, kCardHiddenY - 0.12f, -0.48f });
    stageCardShadow_->SetColor({ 0.01f, 0.04f, 0.08f, 0.68f });
    stageCardShadow_->SetEnableLighting(false);

    UpdateBookOpening(0.0f);
    backdrop_->Update();
    leftBookCover_->Update();
    rightBookCover_->Update();
    bookSpine_->Update();
    leftPageBlock_->Update();
    rightPageBlock_->Update();
    stageCardShadow_->Update();
    stageCard_->Update();
}

void StageSelectScene::InitializeTurningPage()
{
    Object3dManager* objectManager = Object3dManager::GetInstance();
    Model* stripModel = ModelManager::GetInstance()->CreateCube(kWhiteTexture);

    turningPageStrips_.reserve(kTurningPageStripCount);
    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        std::unique_ptr<Object3d> strip = std::make_unique<Object3d>();
        strip->Initialize(objectManager);
        strip->SetModel(stripModel);
        strip->SetEnableLighting(false);
        strip->SetColor({ 0.08f, 0.85f, 1.0f, 0.0f });
        strip->SetScale({ 0.35f, 4.6f, 0.025f });
        strip->SetTranslate({ 0.0f, -8.0f, 2.0f });
        strip->Update();
        turningPageStrips_.push_back(std::move(strip));
    }
}

void StageSelectScene::InitializeOpeningPages()
{
    Model* scanModel = ModelManager::GetInstance()->CreateCube(kWhiteTexture);
    openingPageStrips_.reserve(kOpeningPageCount * kOpeningPageStripCount);
    openingPageVisible_.assign(kOpeningPageCount, false);
    for (uint32_t index = 0; index < kOpeningPageCount * kOpeningPageStripCount; ++index) {
        auto strip = std::make_unique<Object3d>();
        strip->Initialize(Object3dManager::GetInstance());
        strip->SetModel(scanModel);
        strip->SetEnableLighting(false);
        strip->SetColor({ 0.10f, 0.88f, 1.0f, 0.0f });
        strip->SetScale({ 0.0f, 0.025f, 0.02f });
        strip->SetTranslate({ 0.0f, -8.0f, 2.0f });
        strip->Update();
        openingPageStrips_.push_back(std::move(strip));
    }
}

void StageSelectScene::InitializeInterface()
{
    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("MISSION CONTROL");
    titleText_->SetPosition({ 640.0f, 68.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(48.0f);
    titleText_->SetColor({ 0.16f, 0.88f, 1.0f, 1.0f });
    titleText_->SetOutlineColor({ 0.02f, 0.03f, 0.06f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);

    stageText_ = std::make_unique<Text>();
    stageText_->Initialize(kDefaultFont);
    stageText_->SetPosition({ 640.0f, 318.0f });
    stageText_->SetAnchorPoint({ 0.5f, 0.5f });
    stageText_->SetFontSize(32.0f);
    stageText_->SetColor({ 0.93f, 0.98f, 1.0f, 0.0f });
    stageText_->SetOutlineColor({ 0.0f, 0.03f, 0.05f, 1.0f });
    stageText_->SetOutlineWidth(2.0f);

    descriptionText_ = std::make_unique<Text>();
    descriptionText_->Initialize(kDefaultFont);
    descriptionText_->SetPosition({ 640.0f, 365.0f });
    descriptionText_->SetAnchorPoint({ 0.5f, 0.5f });
    descriptionText_->SetFontSize(20.0f);
    descriptionText_->SetColor({ 0.65f, 0.92f, 0.95f, 0.0f });

    pageText_ = std::make_unique<Text>();
    pageText_->Initialize(kDefaultFont);
    pageText_->SetPosition({ 640.0f, 435.0f });
    pageText_->SetAnchorPoint({ 0.5f, 0.5f });
    pageText_->SetFontSize(18.0f);
    pageText_->SetColor({ 0.82f, 0.74f, 0.55f, 0.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("A / D: MISSION    ENTER / CLICK: LAUNCH    BACKSPACE: TITLE");
    instructionText_->SetPosition({ 640.0f, 660.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(20.0f);
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, 1.0f });
}

void StageSelectScene::InitializeDustMotes()
{
    dustMotes_.reserve(kDustMoteCount);
    Model* dustModel = ModelManager::GetInstance()->CreateCube("resources/Textures/white.png");

    for (uint32_t index = 0; index < kDustMoteCount; ++index) {
        const float seed = static_cast<float>(index);
        DustMote mote;
        mote.object = std::make_unique<Object3d>();
        mote.object->Initialize(Object3dManager::GetInstance());
        mote.object->SetModel(dustModel);

        // 決まった数列を使い、起動するたびに配置が変わらないようにする。
        mote.basePosition = {
            std::sin(seed * 2.17f) * 10.5f,
            -4.2f + std::fmod(seed * 1.73f, 10.7f),
            -9.0f + std::fmod(seed * 2.91f, 16.0f)
        };
        mote.phase = seed * 0.83f;
        mote.speed = 0.10f + std::fmod(seed * 0.037f, 0.13f);
        mote.drift = 0.12f + std::fmod(seed * 0.071f, 0.28f);

        const float size = 0.018f + std::fmod(seed * 0.013f, 0.035f);
        mote.object->SetScale({ size, size, size });
        mote.object->SetTranslate(mote.basePosition);
        const float brightness = 0.72f + std::fmod(seed * 0.041f, 0.22f);
        mote.object->SetColor({ brightness * 0.25f, brightness * 0.80f, brightness, 0.42f });
        mote.object->SetEnableLighting(false);
        mote.object->Update();
        dustMotes_.push_back(std::move(mote));
    }
}

void StageSelectScene::UpdateDustMotes(float deltaTime)
{
    const float totalTime = static_cast<float>(TimeManager::GetInstance()->GetTotalTime());
    for (DustMote& mote : dustMotes_) {
        mote.basePosition.y += mote.speed * deltaTime;
        if (mote.basePosition.y > 6.5f) {
            mote.basePosition.y = -4.2f;
        }

        const Vector3 position = {
            mote.basePosition.x + std::sin(totalTime * 0.31f + mote.phase) * mote.drift,
            mote.basePosition.y + std::sin(totalTime * 0.47f + mote.phase * 1.7f) * 0.10f,
            mote.basePosition.z + std::cos(totalTime * 0.23f + mote.phase) * mote.drift * 0.45f
        };
        mote.object->SetTranslate(position);
        mote.object->Update();
    }
}

void StageSelectScene::InitializeTransition()
{
animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    openingRifflePlayed_ = false;
    UpdateBookOpening(0.0f);
    std::fill(openingPageVisible_.begin(), openingPageVisible_.end(), false);
    UpdateCardTransform(0.0f, 0.0f);
    transitionPage_ = std::make_unique<Sprite>();
    transitionPage_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    transitionPage_->SetAnchorPoint({ 0.5f, 0.5f });
    transitionPage_->SetPosition({ 640.0f, 360.0f });
    transitionPage_->SetSize({ 1280.0f, 720.0f });
    transitionPage_->SetColor({ 1.0f, 0.91f, 0.68f, 0.0f });
    transitionPage_->Update();
}

void StageSelectScene::StartArchiveApproach()
{
    audio_.Play(kConfirmSoundName, 0.65f);
    state_ = BookSelectState::CameraApproach;
    animationTime_ = 0.0f;
    openingRifflePlayed_ = false;
    titleText_->SetText("MISSION CONTROL");
    titleText_->SetPosition({ 640.0f, 68.0f });
    titleText_->SetFontSize(48.0f);
    titleText_->SetColor({ 0.16f, 0.88f, 1.0f, 0.0f });
    instructionText_->SetText("A / D: MISSION    ENTER / CLICK: LAUNCH    BACKSPACE: TITLE");
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    instructionText_->SetText("A / D: MISSION    ENTER: LAUNCH    BACKSPACE: TITLE    F1 / F2 / F3: TESTS");
#endif
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, 0.0f });
}

void StageSelectScene::StartTitleReturn()
{
    state_ = BookSelectState::ReturningToTitle;
    animationTime_ = 0.0f;
    audio_.Play(kBackSoundName, 0.55f);
    audio_.Play(kPageRiffleSoundName, 0.42f);
    titleText_->SetText("MISSION CONTROL");
    titleText_->SetPosition({ 640.0f, 68.0f });
    titleText_->SetFontSize(48.0f);
    instructionText_->SetText(
        "A / D: MISSION    ENTER / CLICK: LAUNCH    BACKSPACE: TITLE");
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    instructionText_->SetText(
        "A / D: MISSION    ENTER: LAUNCH    BACKSPACE: TITLE    F1 / F2 / F3: TESTS");
#endif
    std::fill(openingPageVisible_.begin(), openingPageVisible_.end(), false);
}

void StageSelectScene::UpdateTitleReturn(float deltaTime)
{
    animationTime_ += deltaTime;
    const float progress = Clamp01(animationTime_ / kTitleReturnDuration);
    const float eased = progress * progress * progress *
        (progress * (progress * 6.0f - 15.0f) + 10.0f);

    camera_->LookAt(
        kCameraEndEye + (kCameraStartEye - kCameraEndEye) * eased,
        kCameraEndTarget + (kCameraStartTarget - kCameraEndTarget) * eased);
    SceneManager::GetInstance()->SetArchiveApproach(1.0f - eased);

    // ミッションパネルを収納してから、端末を閉じながら後退する。
    const float cardProgress = SmoothStep(progress / 0.28f);
    UpdateCardTransform(1.0f - cardProgress, 1.0f - cardProgress);
    const float closeProgress = SmoothStep((progress - 0.10f) / 0.82f);
    UpdateBookOpening(1.0f - closeProgress);

    const float interfaceAlpha = 1.0f - SmoothStep(progress / 0.35f);
    titleText_->SetColor({ 0.16f, 0.88f, 1.0f, interfaceAlpha });
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, interfaceAlpha });

    if (progress >= 1.0f) {
        transitionQueued_ = true;
        audio_.Stop();
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
    }
}

void StageSelectScene::Update()
{//deltaTimeを取得
    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    if (transitionQueued_) return;
    // The previous scene is finalized one frame after our Initialize.
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    audio_.Update();
    //埃を動かす
    UpdateDustMotes(deltaTime);

	// 入力処理を行い、状態遷移が発生した場合は更新を中断する。
    if (HandleInput()) {
        return;
    }

    UpdateCurrentState(deltaTime);
    UpdateSceneObjects();
}

bool StageSelectScene::HandleInput()
{
Input* input = Input::GetInstance();
    if (!input || state_ != BookSelectState::Idle || transitionQueued_) return false;
    if (input->IsKeyTrigger(DIK_BACK)) {
        StartTitleReturn();
        return true;
    }
    StageDestination shortcut = StageDestination::Title;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (input->IsKeyTrigger(DIK_F1)) shortcut = StageDestination::GameTest;
    else if (input->IsKeyTrigger(DIK_F2)) shortcut = StageDestination::SpriteTest;
    else if (input->IsKeyTrigger(DIK_F3)) shortcut = StageDestination::TextTest;
#endif
    if (shortcut != StageDestination::Title) {
        auto entry = std::find_if(stages_.begin(), stages_.end(), [shortcut](const StageData& stage) {
            return stage.destination == shortcut;
        });
        if (entry != stages_.end()) {
            currentStageIndex_ = static_cast<int32_t>(entry - stages_.begin());
            RefreshStageText();
            ConfirmStage();
        }
        return true;
    }
    bool leftClick = false, rightClick = false, confirmClick = false;
    if (input->IsMouseTrigger(0)) {
        POINT mouse {};
        RECT client {};
        const HWND window = WinApp::GetInstance()->GetHwnd();
        if (GetCursorPos(&mouse) && ScreenToClient(window, &mouse) && GetClientRect(window, &client) &&
            client.right > 0 && client.bottom > 0) {
            const float x = mouse.x * 1280.0f / client.right;
            const float y = mouse.y * 720.0f / client.bottom;
            if (y >= 200.0f && y <= 560.0f) {
                leftClick = x >= 120.0f && x < 440.0f;
                rightClick = x > 840.0f && x <= 1160.0f;
                confirmClick = x >= 440.0f && x <= 840.0f;
            }
        }
    }
    if (input->IsKeyTrigger(DIK_RIGHT) || input->IsKeyTrigger(DIK_D) || rightClick) {
        StartPageTurn(PageTurnDirection::Right);
    } else if (input->IsKeyTrigger(DIK_LEFT) || input->IsKeyTrigger(DIK_A) || leftClick) {
        StartPageTurn(PageTurnDirection::Left);
    } else if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE) || confirmClick) {
        ConfirmStage();
        return true;
    }
    return false;
}

void StageSelectScene::UpdateCurrentState(float deltaTime)
{
    switch (state_) {
    case BookSelectState::CameraApproach:
		// 作戦端末に近づくアニメーションを更新する。
        UpdateCameraApproach(deltaTime);
        break;
    case BookSelectState::CardOpening:
		// カードを開くアニメーションを更新する。
        UpdateCardOpening(deltaTime);
        break;
    case BookSelectState::Idle:
		// 選択待機中はミッションパネルをわずかに浮遊させる。
        UpdateCardIdle();
        break;
    case BookSelectState::CardClosing:
		// カードを閉じるアニメーションを更新する。
        UpdateCardClosing(deltaTime);
        break;
    case BookSelectState::PageTurning:
		// ミッション表示を切り替えるスキャン演出を更新する。
        UpdatePageTurning(deltaTime);
        break;
    case BookSelectState::ReturningToTitle:
		// タイトル画面へ戻るアニメーションを更新する。
        UpdateTitleReturn(deltaTime);
        break;
    case BookSelectState::StageConfirmed:
		// ミッション確定後、カメラを作戦パネルへ寄せる。
        UpdateStageConfirmed(deltaTime);
        break;
    }
}

void StageSelectScene::UpdateSceneObjects()
{
    camera_->Update();
    if (state_ == BookSelectState::CameraApproach || state_ == BookSelectState::StageConfirmed) {
        for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
            if (!openingPageVisible_[page]) {
                continue;
            }
            for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
                openingPageStrips_[page * kOpeningPageStripCount + strip]->Update();
            }
        }
    }
    backdrop_->Update();
    if (hangarShip_ != nullptr) {
        hangarShip_->Update();
    }
    for (const auto& structure : hangarStructures_) {
        structure->Update();
    }
    for (const auto& fitting : bookFittings_) {
        fitting->Update();
    }
    leftBookCover_->Update();
    rightBookCover_->Update();
    bookSpine_->Update();
    leftPageBlock_->Update();
    rightPageBlock_->Update();
    stageCardShadow_->Update();
    stageCard_->Update();
    titleText_->Update();
    stageText_->Update();
    descriptionText_->Update();
    pageText_->Update();
    instructionText_->Update();
}

void StageSelectScene::UpdateCameraApproach(float deltaTime)
{
    animationTime_ += deltaTime;
    const float progress = Clamp01(animationTime_ / kCameraApproachDuration);
	// 端末の起動途中で電子音を鳴らす。
    if (!openingRifflePlayed_ && progress >= 0.24f) {
        audio_.Play(kPageRiffleSoundName, 0.48f);
        openingRifflePlayed_ = true;
    }

	// 進行度をイージングして、カメラと端末パネルを更新する。
    const float eased = progress * progress * progress *(progress * (progress * 6.0f - 15.0f) + 10.0f);
	// 接近度をSceneManagerへ渡し、遷移演出と同期させる。
    SceneManager::GetInstance()->SetArchiveApproach(eased);
	// カメラの位置と注視点を更新する。
    camera_->LookAt(kCameraStartEye + (kCameraEndEye - kCameraStartEye) * eased,kCameraStartTarget + (kCameraEndTarget - kCameraStartTarget) * eased);
	// 左右の端末パネルを少し遅れて展開する。
    const float coverTime = Clamp01((progress - 0.08f) / 0.88f);
    const float bookProgress = coverTime * coverTime * coverTime *(coverTime * (coverTime * 6.0f - 15.0f) + 10.0f);
    UpdateBookOpening(bookProgress);
    UpdateOpeningPages(progress, bookProgress);
	// タイトルと操作説明の表示を徐々にフェードインさせる。
    const float titleAlpha = SmoothStep((progress - 0.55f) / 0.45f);
    titleText_->SetColor({ 0.16f, 0.88f, 1.0f, titleAlpha });
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, titleAlpha });
    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        state_ = BookSelectState::CardOpening;
    }
}

void StageSelectScene::UpdateBookOpening(float progress)
{
    const float opened = SmoothStep(progress);
    const float panelCenterX = 0.55f + 2.15f * opened;
    const float panelY = -0.10f;
    const float panelZ = 0.35f - 0.65f * opened;
    const float panelAlpha = 0.18f + 0.68f * opened;

    const auto setPart = [](Object3d* object, const Vector3& scale,
                            const Vector3& rotation, const Vector3& position) {
        object->SetCustomWorldMatrix(
            MatrixMath::MakeAffineMatrix(scale, rotation, position));
    };

    setPart(leftPageBlock_.get(), { 2.35f, 4.55f, 0.075f },
        { 0.0f, -0.08f * opened, 0.0f }, { -panelCenterX, panelY, panelZ });
    setPart(rightPageBlock_.get(), { 2.35f, 4.55f, 0.075f },
        { 0.0f, 0.08f * opened, 0.0f }, { panelCenterX, panelY, panelZ });
    leftPageBlock_->SetColor({ 0.012f, 0.18f, 0.27f, panelAlpha });
    rightPageBlock_->SetColor({ 0.012f, 0.18f, 0.27f, panelAlpha });

    const float frameX = panelCenterX + 1.85f;
    const float frameYaw = (1.0f - opened) * 0.28f;
    setPart(leftBookCover_.get(), { 0.72f, 5.25f, 0.24f },
        { 0.0f, -frameYaw, 0.0f }, { -frameX, panelY, panelZ + 0.12f });
    setPart(rightBookCover_.get(), { 0.72f, 5.25f, 0.24f },
        { 0.0f, frameYaw, 0.0f }, { frameX, panelY, panelZ + 0.12f });
    setPart(bookSpine_.get(), { 0.20f, 5.35f, 0.22f },
        { 0.0f, 0.0f, 0.0f }, { 0.0f, panelY, panelZ - 0.08f });

    for (uint32_t index = 0; index < bookFittings_.size(); ++index) {
        const bool right = index >= 4;
        const uint32_t edge = index % 4;
        const float side = right ? 1.0f : -1.0f;
        Vector3 scale = { 2.35f, 0.055f, 0.04f };
        Vector3 position = { side * panelCenterX, 4.42f, panelZ - 0.12f };
        if (edge == 1) {
            position.y = -4.62f;
        } else if (edge == 2 || edge == 3) {
            scale = { 0.055f, 4.55f, 0.04f };
            position.x = side * (panelCenterX + (edge == 2 ? 2.30f : -2.30f));
            position.y = panelY;
        }
        setPart(bookFittings_[index].get(), scale,
            { 0.0f, 0.0f, 0.0f }, position);
        bookFittings_[index]->SetColor({ 0.08f, 0.82f, 1.0f, opened });
    }
}

void StageSelectScene::UpdateOpeningPages(float cameraProgress, float bookProgress)
{
    (void)bookProgress;
    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        const float order = static_cast<float>(page) / static_cast<float>(kOpeningPageCount - 1);
        const float start = 0.18f + order * 0.58f;
        const float duration = 0.20f;
        const float phase = Clamp01((cameraProgress - start) / duration);
        openingPageVisible_[page] = phase > 0.0f && phase < 1.0f;
        if (!openingPageVisible_[page]) {
            continue;
        }
        const float pulse = std::sin(phase * std::numbers::pi_v<float>);
        const float y = -4.05f + order * 8.10f;
        Object3d* strip = openingPageStrips_[page * kOpeningPageStripCount].get();
        strip->SetScale({ 4.75f * pulse, 0.018f + 0.025f * pulse, 0.025f });
        strip->SetTranslate({ 0.0f, y, -0.62f });
        strip->SetColor({ 0.08f, 0.88f, 1.0f, pulse });
    }
}

void StageSelectScene::StartPageTurn(PageTurnDirection direction)
{
	// 選択待機中でなければミッション切り替え入力を無視する。
    if (state_ != BookSelectState::Idle) {
        return;
    }

    audio_.Play(kPageTurnSoundName, 0.55f);
    audio_.Play(kPageFlipSoundName, 0.72f);

    pageTurnDirection_ = direction;
    const int32_t directionValue = static_cast<int32_t>(direction);
	// 次のミッション番号を循環させる。
    const int32_t printSpreadCount = static_cast<int32_t>(stages_.size());
    nextPrintSpreadIndex_ =
        (printSpreadIndex_ + directionValue + printSpreadCount) % printSpreadCount;
    animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    state_ = BookSelectState::CardClosing;
}

void StageSelectScene::UpdateCardOpening(float deltaTime)
{
    animationTime_ += deltaTime;
    float progress = Clamp01(animationTime_ / kCardOpenDuration);
    float easedProgress = EaseOutBack(progress);
    float textAlpha = Clamp01((progress - 0.22f) / 0.48f);
    UpdateCardTransform(easedProgress, textAlpha);

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        state_ = BookSelectState::Idle;
    }
}

void StageSelectScene::UpdateCardIdle()
{
    float totalTime = static_cast<float>(TimeManager::GetInstance()->GetTotalTime());
    float floating = std::sin(totalTime * 2.0f) * 0.055f;
    stageCard_->SetTranslate({ 0.0f, kCardRestY + floating, -0.70f });
    stageCard_->SetScale({ 3.85f, 2.30f, 0.18f });
    stageCard_->SetRotate({-0.035f,std::sin(totalTime * 0.85f) * 0.055f,std::sin(totalTime * 1.25f) * 0.012f
    });

    stageCardShadow_->SetTranslate({0.14f,kCardRestY + floating - 0.14f,-0.48f
    });
    stageCardShadow_->SetScale({ 4.05f, 2.42f, 0.12f });
    stageCardShadow_->SetRotate({-0.035f,std::sin(totalTime * 0.85f) * 0.055f,std::sin(totalTime * 1.25f) * 0.012f
    });

    float screenOffset = floating * -38.0f;
    stageText_->SetPosition({ 640.0f, 318.0f + screenOffset });
    descriptionText_->SetPosition({ 640.0f, 365.0f + screenOffset });
    pageText_->SetPosition({ 640.0f, 435.0f + screenOffset });
}

void StageSelectScene::UpdateCardClosing(float deltaTime)
{
    animationTime_ += deltaTime;
    float progress = Clamp01(animationTime_ / kCardCloseDuration);
    float easedProgress = SmoothStep(progress);
    UpdateCardTransform(1.0f - easedProgress, 1.0f - easedProgress);

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        pageTurnProgress_ = 0.0f;
        state_ = BookSelectState::PageTurning;
		// スキャン切り替えアニメーションを開始する。
        //あらかじめ設置しておく
        UpdateTurningPage();
    }
}

void StageSelectScene::UpdatePageTurning(float deltaTime)
{
    animationTime_ += deltaTime;
    pageTurnProgress_ = Clamp01(animationTime_ / kPageTurnDuration);
    UpdateTurningPage();

    if (!stageIndexChanged_ && pageTurnProgress_ >= 0.5f) {
        ChangeStageIndex();
        stageIndexChanged_ = true;
    }

    if (pageTurnProgress_ >= 1.0f) {
        printSpreadIndex_ = nextPrintSpreadIndex_;
        animationTime_ = 0.0f;
        pageTurnProgress_ = 0.0f;
        state_ = BookSelectState::CardOpening;
    }
}

void StageSelectScene::UpdateTurningPage()
{
    const float direction = static_cast<float>(static_cast<int32_t>(pageTurnDirection_));
    const float stripWidth = 9.6f / static_cast<float>(kTurningPageStripCount);
    const float envelope = std::sin(pageTurnProgress_ * std::numbers::pi_v<float>);
    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        const float rate = (static_cast<float>(index) + 0.5f) /
            static_cast<float>(kTurningPageStripCount);
        const float wave = std::sin((rate - pageTurnProgress_) * std::numbers::pi_v<float> * 2.0f);
        const float x = direction * (-5.2f + (rate + pageTurnProgress_) * 5.2f);
        const float z = -0.72f - envelope * 0.25f - std::abs(wave) * 0.08f;
        Object3d* strip = turningPageStrips_[index].get();
        strip->SetTranslate({ x, -0.10f, z });
        strip->SetScale({ stripWidth * 0.82f, 4.60f, 0.025f });
        strip->SetRotate({ 0.0f, 0.0f, wave * 0.025f });
        strip->SetColor({ 0.08f, 0.78f + 0.20f * envelope, 1.0f, envelope * 0.72f });
        strip->Update();
    }
}

void StageSelectScene::UpdateCardTransform(float progress, float alpha)
{
    float positionY = kCardHiddenY + (kCardRestY - kCardHiddenY) * progress;
    float scaleFactor = 0.12f + 0.88f * progress;
    float rotateX = (1.0f - Clamp01(progress)) * 0.42f;

    stageCard_->SetTranslate({ 0.0f, positionY, -0.70f });
    stageCard_->SetScale({
        3.85f * scaleFactor,
        2.30f * scaleFactor,
        0.18f
    });
    stageCard_->SetRotate({ rotateX, 0.0f, 0.0f });

    stageCardShadow_->SetTranslate({ 0.14f, positionY - 0.14f, -0.48f });
    stageCardShadow_->SetScale({
        4.05f * scaleFactor,
        2.42f * scaleFactor,
        0.12f
    });
    stageCardShadow_->SetRotate({ rotateX, 0.0f, 0.0f });

    float textPositionY = 420.0f - 102.0f * Clamp01(progress);
    stageText_->SetPosition({ 640.0f, textPositionY });
    descriptionText_->SetPosition({ 640.0f, textPositionY + 47.0f });
    pageText_->SetPosition({ 640.0f, textPositionY + 117.0f });
    stageText_->SetColor({ 0.93f, 0.98f, 1.0f, alpha });
    descriptionText_->SetColor({ 0.65f, 0.92f, 0.95f, alpha });
    pageText_->SetColor({ 0.82f, 0.74f, 0.55f, alpha });
}

void StageSelectScene::ChangeStageIndex()
{
    currentStageIndex_ += static_cast<int32_t>(pageTurnDirection_);
    int32_t stageCount = static_cast<int32_t>(stages_.size());
    if (currentStageIndex_ < 0) {
        currentStageIndex_ = stageCount - 1;
    }
    if (currentStageIndex_ >= stageCount) {
        currentStageIndex_ = 0;
    }
    RefreshStageText();
}

void StageSelectScene::RefreshStageText()
{
    const StageData& stage = stages_[currentStageIndex_];
    stageText_->SetText(stage.name);
    descriptionText_->SetText(stage.description);
    pageText_->SetText(
        "MISSION DATA " + std::to_string(currentStageIndex_ + 1) +
        " / " + std::to_string(stages_.size()));
}

void StageSelectScene::ConfirmStage()
{
    if (stages_.empty() || state_ != BookSelectState::Idle) return;
    if (stages_[currentStageIndex_].destination == StageDestination::Title) {
        StartTitleReturn();
        return;
    }
    state_ = BookSelectState::StageConfirmed;
    audio_.Play(kConfirmSoundName, 0.75f);
    const StageData& stage = stages_[currentStageIndex_];
    confirmedDestination_ = stage.destination;
    confirmedStageId_ = stage.stageId;
    confirmationPageSoundPlayed_ = false;
    std::fill(openingPageVisible_.begin(), openingPageVisible_.end(), false);
    animationTime_ = 0.0f;
}

void StageSelectScene::UpdateStageConfirmed(float deltaTime)
{
    animationTime_ += deltaTime;

    // 選択パネルを弾ませたあと端末へ収納し、発進演出へつなげる。
    const float bumpProgress = Clamp01(animationTime_ / 0.20f);
    const float bump = std::sin(bumpProgress * std::numbers::pi_v<float>) * 0.08f;
    const float cardSink = SmoothStep((animationTime_ - 0.18f) / 0.28f);
    UpdateCardTransform(1.0f - cardSink, 1.0f - cardSink);
    if (cardSink <= 0.0f) {
        stageCard_->SetScale({ 3.85f * (1.0f + bump), 2.30f * (1.0f + bump), 0.18f });
        stageCardShadow_->SetScale({ 4.05f * (1.0f + bump), 2.42f * (1.0f + bump), 0.12f });
    }

    const float goldProgress = SmoothStep((animationTime_ - 0.15f) / 0.40f);
    stageText_->SetColor({
        0.93f + 0.07f * goldProgress,
        0.98f - 0.20f * goldProgress,
        1.0f - 0.62f * goldProgress,
        1.0f - cardSink
    });

    const float pageProgress = SmoothStep((animationTime_ - 0.25f) / 0.80f);
    if (!confirmationPageSoundPlayed_ && animationTime_ >= 0.25f) {
        audio_.Play(kPageRiffleSoundName, 0.72f);
        confirmationPageSoundPlayed_ = true;
    }
    UpdateOpeningPages(0.24f + pageProgress * 0.76f, 1.0f);

    const float cameraProgress = SmoothStep((animationTime_ - 0.30f) / 0.78f);
    camera_->LookAt(
        kCameraEndEye + (kConfirmCameraEye - kCameraEndEye) * cameraProgress,
        kCameraEndTarget + (kConfirmCameraTarget - kCameraEndTarget) * cameraProgress);

    const float lightProgress = SmoothStep((animationTime_ - 0.72f) / 0.43f);
    transitionPage_->SetColor({ 1.0f, 0.91f, 0.68f, lightProgress });
    transitionPage_->Update();

    if (animationTime_ >= kConfirmSceneChangeTime && !transitionQueued_) {
        transitionQueued_ = true;
        audio_.Stop();
        PageTransition::RequestReveal();
        auto* manager = SceneManager::GetInstance();
        switch (confirmedDestination_) {
        case StageDestination::GamePlay:
            manager->SetNextSceneWithLoading<LoadingScene, GamePlayScene>(confirmedStageId_);
            break;
        case StageDestination::GameTest:
            manager->SetNextSceneWithLoading<LoadingScene, TestScene1>();
            break;
        case StageDestination::SpriteTest:
            manager->SetNextScene(std::make_unique<SpriteTestScene>());
            break;
        case StageDestination::TextTest:
            manager->SetNextScene(std::make_unique<TextTestScene>());
            break;
        case StageDestination::Title:
            manager->SetNextScene(std::make_unique<TitleScene>());
            break;
        }
    }
}

float StageSelectScene::Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float StageSelectScene::SmoothStep(float value)
{
    float clamped = Clamp01(value);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float StageSelectScene::EaseOutBack(float value)
{
    float clamped = Clamp01(value);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    float shifted = clamped - 1.0f;
    return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
}

void StageSelectScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    if (state_ != BookSelectState::CameraApproach) {
        stageText_->Draw();
        descriptionText_->Draw();
        pageText_->Draw();
    }
    instructionText_->Draw();
    if (state_ == BookSelectState::StageConfirmed) {
        SpriteManager::GetInstance()->PreDraw();
        transitionPage_->Draw();
    }
}

void StageSelectScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    backdrop_->Draw();
    for (const auto& structure : hangarStructures_) {
        structure->Draw();
    }
    if (hangarShip_ != nullptr) {
        hangarShip_->Draw();
    }
    for (const DustMote& mote : dustMotes_) {
        mote.object->Draw();
    }
    leftBookCover_->Draw();
    rightBookCover_->Draw();
    for (const auto& fitting : bookFittings_) {
        fitting->Draw();
    }
    bookSpine_->Draw();
    leftPageBlock_->Draw();
    rightPageBlock_->Draw();
    if (state_ == BookSelectState::CameraApproach || state_ == BookSelectState::StageConfirmed) {
        for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
            if (!openingPageVisible_[page]) {
                continue;
            }
            for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
                openingPageStrips_[page * kOpeningPageStripCount + strip]->Draw();
            }
        }
    }
    if (state_ == BookSelectState::PageTurning) {
        for (const std::unique_ptr<Object3d>& strip : turningPageStrips_) {
            strip->Draw();
        }
    }
    if (state_ != BookSelectState::CameraApproach) {
        stageCardShadow_->Draw();
        stageCard_->Draw();
    }
}

void StageSelectScene::DrawParticle()
{
}

void StageSelectScene::DrawImGui()
{
}
