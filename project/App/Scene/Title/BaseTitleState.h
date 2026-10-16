// タイトル・ステージ選択・設定の状態ごとに、入力と遷移中の更新を切り替える。
#pragma once

class TitleScene;
class Input;

enum class TitleViewState {
    Title, StartingTutorial, EnteringStageSelect, StageSelect, LeavingStageSelect,
    ConfirmingStage, EnteringSettings, Settings, LeavingSettings
};

class BaseTitleState {
public:
    virtual ~BaseTitleState() = default;
    virtual TitleViewState GetType() const = 0;
    // 状態へ入るたびに、画面遷移用の経過時間をリセットする。
    virtual void Enter(TitleScene& scene) const;
    virtual void HandleInput(TitleScene&, const Input&) const {}
    virtual void Update(TitleScene& scene, float dt) const = 0;
    virtual void Exit(TitleScene&) const {}
};

// 指定状態に対応する共有の振る舞いを返す。状態オブジェクトを毎回作り直さない。
const BaseTitleState& GetTitleState(TitleViewState type);
