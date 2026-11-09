#pragma once
#include <string>

//==================================================
// リザルト受け渡し用（AlmaGameScene → AlmaGameOverScene）
//==================================================
struct AlmaGameResult {
	static inline int distance = 0;
	static inline int score = 0;
	static inline int coins = 0;
	static inline int maxCombo = 0;
	static inline int smashed = 0;
	static inline int bestDistance = 0;
	static inline int highScore = 0;
	static inline bool newHighScore = false;
	static inline bool newBestDistance = false;
};

//==================================================
// スコア・コンボ・ハイスコア管理
//==================================================
class AlmaScoreManager {
public:
	// 初期化（セーブデータ読み込み）
	void Initialize();

	// 毎フレーム更新（コンボ時間）
	void Update(float dt);

	// 走行距離（m）を設定 → 距離スコアに反映
	void SetDistance(float distance);

	// ボーナス加算（倍率込みの実際の加算値を返す）
	int AddBonus(int base);

	// コンボ
	void AddCombo();
	void ResetCombo();
	int GetCombo() const { return combo_; }
	int GetMaxCombo() const { return maxCombo_; }
	int GetMultiplier() const;
	float GetComboTimeRatio() const { return combo_ > 0 ? comboTimer_ / kComboTime : 0.0f; }

	// コイン（50枚ごとに true を返す → HP回復）
	bool AddCoin();
	int GetCoins() const { return coins_; }

	// 撃破数
	void AddSmash() { smashed_++; }

	// スコア
	int GetScore() const { return distancePoints_ + bonusPoints_; }
	int GetDistance() const { return distance_; }
	int GetBestDistance() const { return bestDistance_; }
	int GetHighScore() const { return highScore_; }

	// ゲーム終了時：ハイスコア保存＆リザルトへ書き出し
	void Finish();

	// タイトル画面などからの読み込み用
	static void LoadBest(int& bestDistance, int& highScore);

	static inline const int kCoinsPerHeal = 50;

private:
	static void SaveBest(int bestDistance, int highScore);
	static inline const char* kSaveFile = "resources/AlmaRun/save_data.txt";
	static inline const float kComboTime = 2.5f;

	int distance_ = 0;
	int distancePoints_ = 0;
	int bonusPoints_ = 0;
	int combo_ = 0;
	int maxCombo_ = 0;
	float comboTimer_ = 0.0f;
	int coins_ = 0;
	int smashed_ = 0;
	int bestDistance_ = 0;
	int highScore_ = 0;
	bool finished_ = false;
};
