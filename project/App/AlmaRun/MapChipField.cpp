#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "MapChipField.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <cstdlib> // rand() のため追加

// 内部リンケージ
namespace {
std::map<std::string, AlmaMapChipType> mapChipTable = {
    {"0", AlmaMapChipType::kBlank     },
    {"1", AlmaMapChipType::kBlock     },
    {"2", AlmaMapChipType::kSpike     },
    {"3", AlmaMapChipType::kEnemy     },
    {"4", AlmaMapChipType::kSpring    },
    {"5", AlmaMapChipType::kMovingFire},
    {"6", AlmaMapChipType::kEnemy2    },
    {"7", AlmaMapChipType::kGoal      },
};

// 放物線状にコインを置くための行番号 (13~15)
uint32_t ArcRow(int w, int width) {
	float t = (static_cast<float>(w) + 0.5f) / static_cast<float>(width);
	float h = 4.0f * t * (1.0f - t);
	return 15u - static_cast<uint32_t>(std::lround(h * 2.0f));
}
}

// マップチップデータをリセット（初期平地の生成）
void AlmaMapChipField::ResetMapChipData() {
	mapChipData_.data.clear();
	mapChipData_.data.resize(kNumBlockVirtical);
	safeColumns_ = 10;
	pending_.clear();
	tutorialDone_ = false;
	lastPattern_ = AlmaPattern::kCount;

	// 最初は40列分安全な平地をあらかじめ生成しておく
	for (uint32_t i = 0; i < 40; ++i) {
		GenerateNextColumn();
	}
}

// CSV読み込み（自動生成化に伴い不要になったが、呼び出し元の互換性のため残す）
void AlmaMapChipField::LoadMapChipCsv(const std::string& filePath) {
	(void)filePath;
	ResetMapChipData();
}

// インデックスから座標取得
AlmaVector3 AlmaMapChipField::GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) { return AlmaVector3(kBlockWidth * xIndex, kBlockHeight * (kNumBlockVirtical - 1 - yIndex), 0); }

// 動的拡張付きマップチップ取得
AlmaMapChipType AlmaMapChipField::GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) {
	if (yIndex >= kNumBlockVirtical) {
		return AlmaMapChipType::kBlank;
	}

	// データが足りなければ自動生成で拡張
	while (mapChipData_.data.empty() || mapChipData_.data[0].size() <= xIndex) {
		GenerateNextColumn();
	}

	return mapChipData_.data[yIndex][xIndex];
}

// 動的拡張付きマップチップ設定
void AlmaMapChipField::SetMapChipType(uint32_t xIndex, uint32_t yIndex, AlmaMapChipType type) {
	if (yIndex >= kNumBlockVirtical) return;

	// データが足りなければ自動生成で拡張
	while (mapChipData_.data.empty() || mapChipData_.data[0].size() <= xIndex) {
		GenerateNextColumn();
	}

	mapChipData_.data[yIndex][xIndex] = type;
}

//==================================================
// 難易度：走るほど上がる (0~4)
//==================================================
int AlmaMapChipField::GetDifficulty(uint32_t xIndex) {
	if (xIndex < 150) return 0;
	if (xIndex < 350) return 1;
	if (xIndex < 600) return 2;
	if (xIndex < 900) return 3;
	return 4;
}

int AlmaMapChipField::Rand(int n) const { return n <= 0 ? 0 : rand() % n; }

// デフォルト地面（最下部とその上をブロックにする）
AlmaMapChipField::Column AlmaMapChipField::BaseColumn() const {
	Column column(kNumBlockVirtical, AlmaMapChipType::kBlank);
	column[19] = AlmaMapChipType::kBlock; // 最下部
	column[18] = AlmaMapChipType::kBlock; // 床
	return column;
}

// 穴（奈落）
AlmaMapChipField::Column AlmaMapChipField::PitColumn() const { return Column(kNumBlockVirtical, AlmaMapChipType::kBlank); }

//==================================================
// ルールベース＋チャンク方式の自動生成（クリア不可能な配置を回避する）
// 手作りパターンを難易度に応じた重みで選び、列キューに積んでいく
//==================================================
void AlmaMapChipField::GenerateNextColumn() {
	uint32_t currentX = mapChipData_.data.empty() ? 0 : static_cast<uint32_t>(mapChipData_.data[0].size());

	Column column;
	if (currentX < 40) {
		// スタート位置周辺（最初の40列）は安全な平地
		column = BaseColumn();
	} else {
		if (pending_.empty()) {
			BuildNextChunk(currentX);
		}
		column = pending_.front();
		pending_.pop_front();
	}

	// 1列追加
	for (uint32_t y = 0; y < kNumBlockVirtical; ++y) {
		mapChipData_.data[y].push_back(column[y]);
	}
}

void AlmaMapChipField::AddGap(int columns, bool allowCoins) {
	// 平地（安全地帯）を走っている際の簡易直線コイン配置
	bool coinRun = allowCoins && columns >= 5 && Rand(100) < 35;
	for (int i = 0; i < columns; ++i) {
		Column c = BaseColumn();
		if (coinRun && i >= 1 && i <= columns - 2) {
			c[16] = AlmaMapChipType::kCoin; // 走りながら回収できる高さ
		}
		Push(c);
	}
}

void AlmaMapChipField::BuildNextChunk(uint32_t startX) {
	int level = GetDifficulty(startX);

	// 最初のチャンクはチュートリアル
	if (!tutorialDone_) {
		tutorialDone_ = true;
		BuildTutorial();
		return;
	}

	// 難易度が高いとパターンを2つ連結する
	int count = (level >= 3 && Rand(100) < 40) ? 2 : 1;
	for (int k = 0; k < count; ++k) {
		AlmaPattern p = PickPattern(level);
		BuildPattern(p, level);
		lastPattern_ = p;
		if (k + 1 < count) {
			AddGap(3, false);
		}
	}

	// 難易度が上がるほど安全地帯が短くなる
	static const int kGapBase[5] = {9, 8, 7, 6, 5};
	AddGap(kGapBase[std::clamp(level, 0, 4)] + Rand(3), true);
}

AlmaMapChipField::AlmaPattern AlmaMapChipField::PickPattern(int level) {
	// 各パターンの出現重み [パターン][難易度]
	static const int kWeights[static_cast<int>(AlmaPattern::kCount)][5] = {
	    {14, 14, 14, 13, 13}, // 穴
	    {12, 12, 11, 10, 10}, // トゲ
	    {8, 7, 6, 5, 5},      // 壁
	    {6, 6, 6, 6, 6},      // バネ＋高壁
	    {4, 4, 4, 4, 4},      // 動く火
	    {7, 7, 7, 7, 7},      // トンネル
	    {0, 5, 6, 7, 7},      // 落下プレス
	    {3, 6, 6, 6, 6},      // 飛び出すトゲ
	    {12, 11, 10, 10, 10}, // 敵の群れ
	    {0, 4, 5, 6, 6},      // 飛ぶ敵
	    {8, 8, 8, 8, 8},      // ヒビ割れ壁
	    {5, 5, 5, 5, 5},      // ダッシュパネル
	    {6, 7, 8, 8, 8},      // 上下2ルート
	    {0, 0, 4, 6, 7},      // 火の下のトゲ
	    {6, 4, 3, 2, 2},      // コインラン
	};
	level = std::clamp(level, 0, 4);
	auto roll = [&]() {
		int total = 0;
		for (int i = 0; i < static_cast<int>(AlmaPattern::kCount); ++i) {
			total += kWeights[i][level];
		}
		int r = Rand(total);
		for (int i = 0; i < static_cast<int>(AlmaPattern::kCount); ++i) {
			r -= kWeights[i][level];
			if (r < 0) {
				return static_cast<AlmaPattern>(i);
			}
		}
		return AlmaPattern::kPit;
	};
	AlmaPattern p = roll();
	// 同じパターンの連続を減らす
	if (p == lastPattern_ && Rand(100) < 70) {
		p = roll();
	}
	return p;
}

void AlmaMapChipField::BuildPattern(AlmaPattern pattern, int level) {
	switch (pattern) {
	case AlmaPattern::kPit: BuildPit(level); break;
	case AlmaPattern::kSpikes: BuildSpikes(level); break;
	case AlmaPattern::kWall: BuildWall(level); break;
	case AlmaPattern::kSpringWall: BuildSpringWall(level); break;
	case AlmaPattern::kFire: BuildFire(level); break;
	case AlmaPattern::kTunnel: BuildTunnel(level); break;
	case AlmaPattern::kPress: BuildPress(level); break;
	case AlmaPattern::kRising: BuildRising(level); break;
	case AlmaPattern::kEnemyGroup: BuildEnemyGroup(level); break;
	case AlmaPattern::kFlyer: BuildFlyer(level); break;
	case AlmaPattern::kBreakableWall: BuildBreakableWall(level); break;
	case AlmaPattern::kBoostLane: BuildBoostLane(level); break;
	case AlmaPattern::kTwoRoute: BuildTwoRoute(level); break;
	case AlmaPattern::kSpikeUnderFire: BuildSpikeUnderFire(level); break;
	case AlmaPattern::kCoinRun: BuildCoinRun(level); break;
	default: BuildPit(level); break;
	}
}

//==================================================
// チュートリアル：トゲ → 敵（丸まりで倒す）→ ヒビ割れブロック（丸まりで壊す）
//==================================================
void AlmaMapChipField::BuildTutorial() {
	// 最初のギミックとして、確実に「飛び出すトゲ」を出現させる
	for (int w = 0; w < 2; ++w) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kRisingSpike;
		c[14] = AlmaMapChipType::kCoin; // 飛び越しコイン
		Push(c);
	}
	AddGap(8, false);

	// 敵（丸まって体当たりで倒す練習）
	{
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kEnemy;
		Push(c);
	}
	AddGap(8, false);

	// ヒビ割れブロック（丸まって壊す練習）
	{
		Column guide = BaseColumn();
		guide[kGroundRow] = AlmaMapChipType::kCoin;
		Push(guide);
		Column c = BaseColumn();
		c[17] = c[16] = c[15] = AlmaMapChipType::kBreakable;
		Push(c);
		for (int i = 0; i < 3; ++i) {
			Column r = BaseColumn();
			r[kGroundRow] = AlmaMapChipType::kCoin;
			Push(r);
		}
	}
	AddGap(9, true);
}

// 穴（奈落）を生成 (幅2〜4マス) + 【誘導コイン配置】
void AlmaMapChipField::BuildPit(int level) {
	int width = 2 + Rand(level >= 2 ? 3 : 2);

	// ① 穴の手前1列に、ジャンプ踏み切り誘導コイン(y=16)
	Column pre = BaseColumn();
	pre[16] = AlmaMapChipType::kCoin;
	Push(pre);

	// ② 穴の空中上空に放物線を描いてコインを配置
	for (int w = 0; w < width; ++w) {
		Column c = PitColumn();
		c[ArcRow(w, width)] = AlmaMapChipType::kCoin;
		Push(c);
	}

	// ③ 穴の直後に着地誘導コイン(y=16)
	Column post = BaseColumn();
	post[16] = AlmaMapChipType::kCoin;
	Push(post);

	// 高難易度：着地地点の先に敵
	if (level >= 3 && Rand(100) < 35) {
		Push(BaseColumn());
		Push(BaseColumn());
		Column e = BaseColumn();
		e[kGroundRow] = AlmaMapChipType::kEnemy;
		Push(e);
	}
}

// トゲを配置 (床の上 y=17) + 【飛び越し誘導コイン配置】
void AlmaMapChipField::BuildSpikes(int level) {
	int width = 1 + Rand(level >= 2 ? 3 : 2);
	for (int w = 0; w < width; ++w) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kSpike;
		c[width == 1 ? 15 : 14] = AlmaMapChipType::kCoin;
		Push(c);
	}
}

// 壁（高さ2マス。ジャンプで超えられる）
void AlmaMapChipField::BuildWall(int level) {
	Column c = BaseColumn();
	c[17] = c[16] = AlmaMapChipType::kBlock;
	c[14] = AlmaMapChipType::kCoin;
	Push(c);
	if (level >= 2 && Rand(100) < 35) {
		Push(c); // 2マス幅
	}
}

// バネ + 高壁 + 【大ジャンプ軌道の放物線コイン配置】
void AlmaMapChipField::BuildSpringWall(int level) {
	(void)level;
	// 1列目: バネ (バネの上にコイン y=15)
	Column col1 = BaseColumn();
	col1[kGroundRow] = AlmaMapChipType::kSpring;
	col1[15] = AlmaMapChipType::kCoin;
	Push(col1);
	// 2列目: 上昇頂点 (上空高くにコイン y=11)
	Column col2 = BaseColumn();
	col2[11] = AlmaMapChipType::kCoin;
	Push(col2);
	// 3列目: 高壁 (壁の上空 y=11 にコイン)
	Column col3 = BaseColumn();
	col3[17] = col3[16] = col3[15] = col3[14] = AlmaMapChipType::kBlock;
	col3[11] = AlmaMapChipType::kCoin;
	Push(col3);
	// 4列目: 下降・着地側 (少し降りてきた位置にコイン y=13)
	Column col4 = BaseColumn();
	col4[13] = AlmaMapChipType::kCoin;
	Push(col4);
}

// 空中に動く火（高難易度ほど低い位置）
void AlmaMapChipField::BuildFire(int level) {
	Column c = BaseColumn();
	c[level >= 2 ? 14 : 13] = AlmaMapChipType::kMovingFire;
	c[16] = AlmaMapChipType::kCoin;
	Push(c);
	if (level >= 1 && Rand(100) < 50) {
		Push(BaseColumn());
		Push(BaseColumn());
		Push(c);
	}
}

// しゃがみ必須の低天井（トンネル）を生成 + 【トンネル内直線コイン配置】
void AlmaMapChipField::BuildTunnel(int level) {
	// 1. 手前の余裕
	for (int w = 0; w < 2; ++w) {
		Push(BaseColumn());
	}
	Column guide = BaseColumn();
	guide[kGroundRow] = AlmaMapChipType::kCoin;
	Push(guide);

	// 2. トンネル本体 (床直上 y=17 にコイン。高難易度では中に敵)
	int width = 2 + Rand(2) + (level >= 3 ? 1 : 0);
	bool enemyInside = level >= 2 && Rand(100) < 40;
	for (int w = 0; w < width; ++w) {
		Column c = BaseColumn();
		c[16] = c[15] = c[14] = AlmaMapChipType::kBlock;
		c[kGroundRow] = (enemyInside && w == width / 2) ? AlmaMapChipType::kEnemy : AlmaMapChipType::kCoin;
		Push(c);
	}
}

// 落下するプレス機を生成 (前後に十分な余裕)
void AlmaMapChipField::BuildPress(int level) {
	// ① 手前の余裕 (3マスの平地)
	for (int w = 0; w < 3; ++w) {
		Push(BaseColumn());
	}
	// ② プレス機本体の列 (空中 y=13)。下のスライディング隙間にコイン
	int width = 3 + (level >= 3 ? 1 : 0);
	for (int w = 0; w < width; ++w) {
		Column c = BaseColumn();
		c[13] = AlmaMapChipType::kFallingSpike;
		c[kGroundRow] = AlmaMapChipType::kCoin; // スライディング用コイン！
		Push(c);
	}
	// ③ 奥の余裕の1列目 (着地誘導コイン)
	Column post = BaseColumn();
	post[15] = AlmaMapChipType::kCoin;
	Push(post);
}

// 飛び出すトゲ（下からせり出すトゲ）を生成 (幅1〜2マス)
void AlmaMapChipField::BuildRising(int level) {
	(void)level;
	int width = 1 + Rand(2);
	for (int w = 0; w < width; ++w) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kRisingSpike;
		c[width == 1 ? 15 : 14] = AlmaMapChipType::kCoin; // 飛び越し誘導コイン
		Push(c);
	}
}

// 敵の群れ：丸まりで連続撃破するとコンボ！
void AlmaMapChipField::BuildEnemyGroup(int level) {
	int n = std::min(4, 1 + std::min(level, 2) + Rand(2));
	for (int k = 0; k < n; ++k) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kEnemy;
		c[15] = AlmaMapChipType::kCoin; // 飛び越えた時用
		Push(c);
		if (k + 1 < n) {
			Column g = BaseColumn();
			g[kGroundRow] = AlmaMapChipType::kCoin; // 丸まり中に拾える
			Push(g);
		}
	}
}

// 上下に動く敵
void AlmaMapChipField::BuildFlyer(int level) {
	int n = 1 + (level >= 3 ? 1 : 0);
	for (int k = 0; k < n; ++k) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kEnemy2;
		Push(c);
		if (k + 1 < n) {
			for (int i = 0; i < 4; ++i) {
				Push(BaseColumn());
			}
		}
	}
}

// ヒビ割れ壁：丸まり体当たりで粉砕（または二段ジャンプで越える）
void AlmaMapChipField::BuildBreakableWall(int level) {
	Column guide = BaseColumn();
	guide[kGroundRow] = AlmaMapChipType::kCoin;
	Push(guide);
	int width = 1 + (level >= 2 ? Rand(2) : 0);
	for (int w = 0; w < width; ++w) {
		Column c = BaseColumn();
		c[17] = c[16] = c[15] = AlmaMapChipType::kBreakable;
		Push(c);
	}
	for (int i = 0; i < 3; ++i) {
		Column r = BaseColumn();
		r[kGroundRow] = AlmaMapChipType::kCoin;
		Push(r);
	}
}

// ダッシュパネル → コインレーン（→ 高難易度はヒビ割れ壁を突き破る）
void AlmaMapChipField::BuildBoostLane(int level) {
	Column pad = BaseColumn();
	pad[kGroundRow] = AlmaMapChipType::kBoostPad;
	Push(pad);
	Push(pad);
	for (int i = 0; i < 8; ++i) {
		Column c = BaseColumn();
		c[kGroundRow] = AlmaMapChipType::kCoin;
		Push(c);
	}
	if (level >= 2) {
		Push(BaseColumn());
		Column wall = BaseColumn();
		wall[17] = wall[16] = wall[15] = AlmaMapChipType::kBreakable;
		Push(wall);
	}
}

// 上下2ルート：上＝コインが多い安全ルート / 下＝丸まりで敵とブロックを粉砕するルート
void AlmaMapChipField::BuildTwoRoute(int level) {
	// 段差（3列）
	for (int i = 0; i < 3; ++i) {
		Column step = BaseColumn();
		step[kGroundRow] = AlmaMapChipType::kBlock;
		Push(step);
	}
	// 段差から降りるためのすき間（3列）
	for (int i = 0; i < 3; ++i) {
		Push(BaseColumn());
	}
	// 空中の足場
	int length = 8 + Rand(4);
	for (int k = 0; k < length; ++k) {
		Column c = BaseColumn();
		c[15] = AlmaMapChipType::kBlock; // 足場
		c[14] = AlmaMapChipType::kCoin;  // 上ルートのコイン
		if (k % 4 == 1) {
			c[kGroundRow] = (Rand(2) == 0) ? AlmaMapChipType::kEnemy : AlmaMapChipType::kBreakable;
		} else {
			c[kGroundRow] = AlmaMapChipType::kCoin;
		}
		if (level >= 3 && k == length / 2) {
			c[12] = AlmaMapChipType::kMovingFire; // 上ルートにも危険
		}
		Push(c);
	}
	Push(BaseColumn());
	Push(BaseColumn());
}

// 火の下のトゲ：低く小さくジャンプしないと火に当たる
void AlmaMapChipField::BuildSpikeUnderFire(int level) {
	(void)level;
	Column c1 = BaseColumn();
	c1[kGroundRow] = AlmaMapChipType::kSpike;
	c1[13] = AlmaMapChipType::kMovingFire;
	c1[15] = AlmaMapChipType::kCoin;
	Push(c1);
	Column c2 = BaseColumn();
	c2[kGroundRow] = AlmaMapChipType::kSpike;
	c2[15] = AlmaMapChipType::kCoin;
	Push(c2);
}

// コインラン：波打つコインの列
void AlmaMapChipField::BuildCoinRun(int level) {
	for (int k = 0; k < 10; ++k) {
		Column c = BaseColumn();
		float s = std::sin(static_cast<float>(k) * 0.8f);
		uint32_t row = 16u - static_cast<uint32_t>(std::lround(1.5f * (1.0f + s)));
		c[row] = AlmaMapChipType::kCoin;
		if (level >= 1 && k == 5 && row <= 14) {
			c[kGroundRow] = AlmaMapChipType::kSpike;
		}
		Push(c);
	}
}

AlmaMapChipField::AlmaIndexSet AlmaMapChipField::GetMapChipIndexSetByposition(const AlmaVector3& position) {
	AlmaIndexSet indexSet = {};
	indexSet.xIndex = static_cast<uint32_t>((position.x + kBlockWidth / 2.0f) / kBlockWidth);
	indexSet.yIndex = kNumBlockVirtical - 1 - static_cast<uint32_t>((position.y + kBlockHeight / 2.0f) / kBlockHeight);
	return indexSet;
}

AlmaMapChipField::AlmaRect AlmaMapChipField::GetRectByIndex(uint32_t xIndex, uint32_t yIndex) {
	AlmaVector3 center = GetMapChipPositionByIndex(xIndex, yIndex);
	AlmaRect rect = {};
	rect.left = center.x - kBlockWidth / 2.0f;
	rect.right = center.x + kBlockWidth / 2.0f;
	rect.top = center.y + kBlockHeight / 2.0f;
	rect.bottom = center.y - kBlockHeight / 2.0f;
	return rect;
}
