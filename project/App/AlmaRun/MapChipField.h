#pragma once
#include "KamataEngine.h"
#include "Math.h"
#include <deque>
#include <vector>

// マップチップの種類
enum class AlmaMapChipType {
	kBlank,  // 空白
	kBlock,  // 普通のブロック
	kSpike,  // トゲブロックを追加
	kEnemy,  // 敵を追加
	kSpring, // ばね追加
	kMovingFire,//火の玉
	kEnemy2,
	kGoal,
	kCoin,
	kFallingSpike,
	kRisingSpike,
	kBreakable, // 丸まり体当たりで壊せるヒビ割れブロック
	kBoostPad,  // 踏むと加速するダッシュパネル
};

// 当たり判定上「固い」マップチップか
inline bool IsSolidChip(AlmaMapChipType type) { return type == AlmaMapChipType::kBlock || type == AlmaMapChipType::kBreakable; }

struct AlmaMapChipData {
	std::vector<std::vector<AlmaMapChipType>> data;
};

class AlmaMapChipField {
public:
	void Initialize();
	void UpDate();
	void Draw();
	void ResetMapChipData();
	void LoadMapChipCsv(const std::string& filepath);

	AlmaMapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);
	void SetMapChipType(uint32_t xIndex, uint32_t yIndex, AlmaMapChipType type);
	void GenerateNextColumn();
	AlmaVector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndedx);

	uint32_t GetNumBlockVirtical() const { return kNumBlockVirtical; }
	uint32_t GetNumBlockHorizontal() const { return kNumBlockHorizontal; }

	struct AlmaIndexSet {
		uint32_t xIndex;
		uint32_t yIndex;
	};

	struct AlmaRect {
		float left;
		float right;
		float top;
		float bottom;
	};

	AlmaIndexSet GetMapChipIndexSetByposition(const AlmaVector3& position);
	AlmaRect GetRectByIndex(uint32_t xIndex, uint32_t yIndex);
	static inline const float kBlockWidth = 1.0f;
	static inline const float kBlockHeight = 1.0f;

	// 列番号から難易度 (0~4)
	static int GetDifficulty(uint32_t xIndex);

private:
	using Column = std::vector<AlmaMapChipType>;

	// ===== チャンク（パターン）生成 =====
	enum class AlmaPattern {
		kPit,
		kSpikes,
		kWall,
		kSpringWall,
		kFire,
		kTunnel,
		kPress,
		kRising,
		kEnemyGroup,
		kFlyer,
		kBreakableWall,
		kBoostLane,
		kTwoRoute,
		kSpikeUnderFire,
		kCoinRun,
		kCount
	};

	Column BaseColumn() const;
	Column PitColumn() const;
	void Push(const Column& column) { pending_.push_back(column); }
	void BuildNextChunk(uint32_t startX);
	void BuildTutorial();
	AlmaPattern PickPattern(int level);
	void BuildPattern(AlmaPattern pattern, int level);
	void AddGap(int columns, bool allowCoins);
	int Rand(int n) const;

	void BuildPit(int level);
	void BuildSpikes(int level);
	void BuildWall(int level);
	void BuildSpringWall(int level);
	void BuildFire(int level);
	void BuildTunnel(int level);
	void BuildPress(int level);
	void BuildRising(int level);
	void BuildEnemyGroup(int level);
	void BuildFlyer(int level);
	void BuildBreakableWall(int level);
	void BuildBoostLane(int level);
	void BuildTwoRoute(int level);
	void BuildSpikeUnderFire(int level);
	void BuildCoinRun(int level);

	AlmaMapChipData mapChipData_;
	static inline const uint32_t kNumBlockVirtical = 20;
	static inline const uint32_t kNumBlockHorizontal = 200;

	// 床の行 / 地上オブジェクトの行
	static inline const uint32_t kFloorRow = 18;
	static inline const uint32_t kGroundRow = 17;

	// 自動生成用パラメータ
	uint32_t safeColumns_ = 10;
	std::deque<Column> pending_;
	bool tutorialDone_ = false;
	AlmaPattern lastPattern_ = AlmaPattern::kCount;
};
