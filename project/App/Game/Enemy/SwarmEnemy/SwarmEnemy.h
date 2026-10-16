// 編隊内の担当位置へ移動する敵。編隊の形・進行・射撃を個体ごとに計算する。
#pragma once

#include "App/Game/Enemy/BaseEnemy.h"
#include <cstdint>
#include <memory>
#include <algorithm>

class Model;
class Player;

enum class SwarmFormationType { Spiral, Wall, Glyph, Diamond, Wave, Arrow };

class SwarmGroupState {
public:
    explicit SwarmGroupState(int32_t totalCount = 0)
        : totalCount_((std::max)(0, totalCount)), activeCount_(totalCount_) {}
    void RecordRemoval(bool escaped)
    {
        if (activeCount_ <= 0) { return; }
        --activeCount_;
        if (escaped) { ++escapedCount_; } else { ++defeatedCount_; }
    }
    int32_t GetTotalCount() const { return totalCount_; }
    int32_t GetActiveCount() const { return activeCount_; }
    int32_t GetDefeatedCount() const { return defeatedCount_; }
    int32_t GetEscapedCount() const { return escapedCount_; }
private:
    int32_t totalCount_ = 0;
    int32_t activeCount_ = 0;
    int32_t defeatedCount_ = 0;
    int32_t escapedCount_ = 0;
};

class SwarmEnemy : public BaseEnemy {
public:
    std::string_view GetAppearanceId() const override { return "SwarmEnemy"; }
    // 編隊の基準位置・個体番号・進行方向などを記録し、群れの一体として配置する。
    void Initialize(
        Model* model,
        Model* bulletModel,
        Player* player,
        const std::shared_ptr<SwarmGroupState>& groupState,
        SwarmFormationType formationType,
        int32_t slotIndex,
        int32_t travelDirection);

    // 編隊内の移動と射撃を、基底クラスの更新経路で進める。
    void Update() override;
    // 外見を変更し、群れの表示に必要な設定を反映する。
    void SetAppearance(Model* model) override;
    // 見た目に対応する現在の位置と半径を、当たり判定として書き出す。
    void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const override;

private:
    friend bool RunSwarmFormationTests();
    static constexpr int32_t kSwarmMemberCount = 18;
    struct FormationContext {
        int32_t slotIndex;
        int32_t travelDirection;
        float elapsedSeconds;
    };
    // 個体番号を角度へ割り当て、渦状の編隊内の位置を求める。
    static Vector3 CalculateSpiralOffset(const FormationContext& context);
    // 個体を行と列へ割り当て、壁状の編隊内の位置を求める。
    static Vector3 CalculateWallOffset(const FormationContext& context);
    // 文字や記号を形作る編隊内の位置を求める。
    static Vector3 CalculateGlyphOffset(const FormationContext& context);
    // 菱形に広がる編隊内の位置を求める。
    static Vector3 CalculateDiamondOffset(const FormationContext& context);
    // 波状に並ぶ編隊内の位置を求める。
    static Vector3 CalculateWaveOffset(const FormationContext& context);
    // 矢印状に並ぶ編隊内の位置を求める。
    static Vector3 CalculateArrowOffset(const FormationContext& context);
    // 現在の編隊形状における、この個体の相対位置を求める。
    static Vector3 CalculateFormationOffset(SwarmFormationType type, const FormationContext& context);
    // 編隊の種類に対応する表示色を返す。
    static Vector4 GetFormationColor(SwarmFormationType type);
    // 編隊の移動と個体ごとの揺れを合成し、担当位置へ追従させる。
    void Move() override;
    // 編隊の進行状態や発射間隔を調べ、移動しながら撃つ弾を生成する。
    void Attack() override;
    // 群れの一体を撃破した際の演出を開始する。
    void OnDeath() override;
    // 群れの進行方向などをもとに、揺れながら飛ぶ弾を発射する。
    void FireMovingBullet();
    // 現在の編隊形状における、この個体の相対位置を求める。
    Vector3 CalculateFormationOffset() const;

private:
    Player* player_ = nullptr;
    Model* bulletModel_ = nullptr;
    std::shared_ptr<SwarmGroupState> groupState_;
    SwarmFormationType formationType_ = SwarmFormationType::Spiral;
    int32_t slotIndex_ = 0;
    int32_t travelDirection_ = 1;
    float moveTime_ = 0.0f;
    float crossingSpeed_ = 12.0f;
    float centerStartX_ = -38.0f;
    float baseHeight_ = 2.0f;
    float forwardDistance_ = 120.0f;
    float forwardPositionZ_ = 0.0f;
    float previousPlayerPositionZ_ = 0.0f;
    float attackTimer_ = 0.0f;
    float fadeTimer_ = 0.0f;
    int32_t shotsFired_ = 0;
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool isFading_ = false;
    bool escaped_ = false;
};
