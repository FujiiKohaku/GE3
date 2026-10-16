// タイトルの基地背景・ステージ選択・設定画面と、各画面をつなぐカメラ演出を扱う。
#pragma once

#include "App/Scene/Common/BaseScene.h"
#include "BaseTitleState.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/OceanSurface.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Light/LightManager.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class TitleScene : public BaseScene {
public:
    explicit TitleScene(bool shouldOpenStageSelect = false)
        : shouldOpenStageSelect_(shouldOpenStageSelect) {}

    // タイトルの背景・UI・ステージ情報を準備し、入場時の表示状態を選ぶ。
    void Initialize() override;
    // タイトル内のリソースと演出状態を片付ける。
    void Finalize() override;
    // 現在のタイトル状態へ入力と更新を渡し、背景とUIの演出を進める。
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    bool SubmitRaytracingScene(DxrRenderer& renderer) override;
    void DrawShadow(ShadowMapRenderer& renderer) override;
    // タイトル背景に適した影の描画範囲と設定を返す。
    ShadowSettings GetShadowSettings() const override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    friend class BaseTitleState;
    friend class TitleMenuState;
    friend class StageSelectState;
    friend class SettingsState;
    friend class TutorialStartState;
    friend class StageSelectTransitionState;
    friend class SettingsTransitionState;
    friend class StageConfirmState;
    friend bool RunTitleStateTests();
#if defined(KOHAKU_RENDER_TESTS)
    friend class TitleSceneRenderTest;
#endif
    struct StageRoomObject {
        std::unique_ptr<Object3d> object;
        Vector3 basePosition {};
        Vector3 baseRotation {};
        float motionScale = 1.0f;
        float delay = 0.0f;
    };

    using ViewState = TitleViewState;
    // タイトルの操作状態を切り替え、遷移先の開始処理を呼ぶ。
    void ChangeState(ViewState state);
    // タイトルの操作状態を切り替え、遷移先の開始処理を呼ぶ。
    void ChangeState(const BaseTitleState& state);
    // 訓練開始前のカメラと機体アニメーションの初期状態を作る。
    void InitializeTutorialAnimation();
    // 設定画面が見やすい位置へカメラを配置する。
    void UpdateSettingsCamera();

    // 基地の共通背景として使うモデルを生成し、位置・向き・大きさを設定する。
    Object3d* AddBaseObject(
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    // ステージ紹介用の部屋にモデルを追加し、遷移中の配置情報も保存する。
    Object3d* AddStageRoomObject(
        std::vector<StageRoomObject>& roomObjects,
        const std::string& modelPath,
        const Vector3& position,
        const Vector3& scale = { 1.0f, 1.0f, 1.0f },
        const Vector3& rotation = { 0.0f, 0.0f, 0.0f },
        float motionScale = 1.0f,
        float delay = 0.0f);
    // タイトル背景の基地を構成する建物や小物を配置する。
    void InitializeLaunchBase();
    // 各ステージを紹介する部屋ごとに、背景モデルの組み合わせを作る。
    void InitializeStageRoomSets();
    // タイトル・設定画面の文字やパネルなどを準備する。
    void InitializeInterface();
    // ステージ一覧から選択画面に必要なカードと表示情報を準備する。
    void InitializeStageSelection();
    // 選択中のステージに合わせて名前と説明文を更新する。
    void RefreshStageSelectionText();
    // 左右のステージ切替を開始し、遷移元と遷移先の配置を記録する。
    void StartStageRoomTransition(int direction);
    // 部屋を入れ替えるアニメーションを進め、完了時に選択を確定する。
    void UpdateStageRoomTransition(float deltaTime);
    // 部屋に属するモデル群を、切替演出中の位置と見た目へ揃える。
    void UpdateStageRoomObjectGroup(
        std::vector<StageRoomObject>& roomObjects,
        float offsetX,
        float settleAmount,
        float settleDirection);
    // ステージIDに対応する部屋のオブジェクト一覧を探す。
    std::vector<StageRoomObject>* FindStageRoomObjects(const std::string& stageId);
    // ステージIDに対応する部屋のオブジェクト一覧を探す。
    const std::vector<StageRoomObject>* FindStageRoomObjects(const std::string& stageId) const;
    // タイトルからステージ選択へ移る演出を開始する。
    void StartStageSelectTransition();
    // チュートリアルの進行状況などに応じて、ゲーム開始後の行き先を決める。
    void StartGame();
    // チュートリアルへ進むための開始演出に切り替える。
    void StartTutorial();
    // 訓練への出発演出を進め、演出終了後にシーンを切り替える。
    void UpdateTutorialStart(float deltaTime);
    // ステージ選択からタイトルへ戻る演出を開始する。
    void StartTitleReturn();
    // 選んだステージを確定し、出発の演出へ進める。
    void ConfirmStage();
    // 設定画面を開く・閉じる方向を記録し、画面遷移を開始する。
    void StartSettingsTransition(bool opening);
    // タイトル待機中のカメラを更新する。
    void UpdateTitleCamera(float deltaTime);
    // タイトルとステージ選択の間で、カメラや表示位置を滑らかに切り替える。
    void UpdateStageSelectTransition(float deltaTime);
    // ステージ選択中の表示と背景の動きを更新する。
    void UpdateStageSelection(float deltaTime);
    // 決定後の出発演出を進め、読み込み画面へつなげる。
    void UpdateStageConfirmation(float deltaTime);
    // 設定画面の開閉演出を進め、完了後の状態へ移る。
    void UpdateSettingsTransition(float deltaTime);
    // 現在の状態と遷移の進み具合を、各UIの位置・透明度へ反映する。
    void UpdateInterface(float deltaTime);
    // 表示中の背景とステージに合わせて、照明や霧を更新する。
    void UpdateSceneLighting();
    // 指定されたステージ紹介部屋のモデル群を描画する。
    void DrawRoomObjects(const std::vector<StageRoomObject>* roomObjects, ShadowMapRenderer* shadows);
    // 遷移元と遷移先を考慮し、今必要な部屋だけを通常描画または影描画へ渡す。
    void DrawVisibleRoomObjects(ShadowMapRenderer* shadows);
    // 部屋のモデルをレイトレーシング用のシーンへ登録する。
    void SubmitRaytracingRoom(const std::vector<StageRoomObject>* roomObjects, DxrRenderer& renderer);

    static float Clamp01(float value);
    // 開始と終了で変化が緩やかになる補間値を作り、画面遷移に使う。
    static float SmoothStep(float value);
    // 目標を少し通り越して戻る補間値を作り、UIの出現に弾みを付ける。
    static float EaseOutBack(float value);

    std::unique_ptr<Camera> camera_;
    SpotLightHandle windowLightHandle_ = kInvalidSpotLightHandle;
    std::unique_ptr<OceanSurface> oceanSurface_;
    std::unique_ptr<Object3d> aircraft_;
    std::vector<std::unique_ptr<Object3d>> baseObjects_;
    std::vector<StageRoomObject> firstFlightRoomObjects_;
    std::vector<StageRoomObject> frozenPassageRoomObjects_;

    std::unique_ptr<Sprite> atmosphereTint_;
    std::unique_ptr<Sprite> settingsBackdrop_;
    std::unique_ptr<Sprite> settingsAccent_;
    std::unique_ptr<Sprite> stagePanel_;
    std::unique_ptr<Sprite> frozenFogOverlay_;
    std::unique_ptr<Text> logoText_;
    std::unique_ptr<Text> pushToStartText_;
    std::unique_ptr<Text> settingsShortcutText_;
    std::unique_ptr<Text> settingsTitleText_;
    std::unique_ptr<Text> settingsItemsText_;
    std::unique_ptr<Text> settingsBackText_;
    std::unique_ptr<Text> stageHeaderText_;
    std::unique_ptr<Text> stageNameText_;
    std::unique_ptr<Text> stageDescriptionText_;
    std::unique_ptr<Text> stagePageText_;
    std::unique_ptr<Text> stageControlsText_;
    std::unique_ptr<Sprite> simulationTint_;
    std::unique_ptr<Sprite> simulationScan_;
    std::unique_ptr<Text> simulationText_;
    Vector3 simulationStartEye_ {};

    struct StagePanelData {
        std::string id;
        std::string name;
        std::string description;
    };
    std::vector<StagePanelData> stages_;
    size_t currentStageIndex_ = 0;
    size_t previousStageIndex_ = 0;
    size_t nextStageIndex_ = 0;
    bool stageTransitionQueued_ = false;
    bool stageRoomTransitionActive_ = false;
    bool stageRoomSelectionSwapped_ = false;
    int stageRoomTransitionDirection_ = 1;
    float stageRoomTransitionTime_ = 0.0f;
    float frozenRoomAmount_ = 0.0f;

    ViewState viewState_ = ViewState::Title;
    static bool hasStartedTutorial_;
    bool shouldOpenStageSelect_ = false;
    const BaseTitleState* currentState_ = nullptr;
    float animationTime_ = 0.0f;
    float idleTime_ = 0.0f;
};
