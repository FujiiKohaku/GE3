#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ScoreManager.h"
#include <algorithm>
#include <fstream>

void AlmaScoreManager::Initialize() {
	distance_ = 0;
	distancePoints_ = 0;
	bonusPoints_ = 0;
	combo_ = 0;
	maxCombo_ = 0;
	comboTimer_ = 0.0f;
	coins_ = 0;
	smashed_ = 0;
	finished_ = false;
	LoadBest(bestDistance_, highScore_);
}

void AlmaScoreManager::Update(float dt) {
	// コンボの持続時間
	if (combo_ > 0) {
		comboTimer_ -= dt;
		if (comboTimer_ <= 0.0f) {
			ResetCombo();
		}
	}
}

void AlmaScoreManager::SetDistance(float distance) {
	distance_ = std::max(distance_, static_cast<int>(distance));
	distancePoints_ = distance_ * 10;
}

int AlmaScoreManager::AddBonus(int base) {
	int value = base * GetMultiplier();
	bonusPoints_ += value;
	return value;
}

void AlmaScoreManager::AddCombo() {
	combo_++;
	maxCombo_ = std::max(maxCombo_, combo_);
	comboTimer_ = kComboTime;
}

void AlmaScoreManager::ResetCombo() {
	combo_ = 0;
	comboTimer_ = 0.0f;
}

int AlmaScoreManager::GetMultiplier() const {
	if (combo_ >= 35) {
		return 5;
	}
	if (combo_ >= 20) {
		return 4;
	}
	if (combo_ >= 10) {
		return 3;
	}
	if (combo_ >= 5) {
		return 2;
	}
	return 1;
}

bool AlmaScoreManager::AddCoin() {
	coins_++;
	return (coins_ % kCoinsPerHeal) == 0;
}

void AlmaScoreManager::Finish() {
	if (finished_) {
		return;
	}
	finished_ = true;

	int totalScore = GetScore();
	bool newBest = distance_ > bestDistance_;
	bool newHigh = totalScore > highScore_;

	if (newBest) {
		bestDistance_ = distance_;
	}
	if (newHigh) {
		highScore_ = totalScore;
	}
	if (newBest || newHigh) {
		SaveBest(bestDistance_, highScore_);
	}

	AlmaGameResult::distance = distance_;
	AlmaGameResult::score = totalScore;
	AlmaGameResult::coins = coins_;
	AlmaGameResult::maxCombo = maxCombo_;
	AlmaGameResult::smashed = smashed_;
	AlmaGameResult::bestDistance = bestDistance_;
	AlmaGameResult::highScore = highScore_;
	AlmaGameResult::newBestDistance = newBest;
	AlmaGameResult::newHighScore = newHigh;
}

void AlmaScoreManager::SaveBest(int bestDistance, int highScore) {
	std::ofstream ofs(kSaveFile);
	if (ofs.is_open()) {
		ofs << bestDistance << " " << highScore << "\n";
	}
}

void AlmaScoreManager::LoadBest(int& bestDistance, int& highScore) {
	bestDistance = 0;
	highScore = 0;
	std::ifstream ifs(kSaveFile);
	if (ifs.is_open()) {
		ifs >> bestDistance >> highScore;
	}
}
