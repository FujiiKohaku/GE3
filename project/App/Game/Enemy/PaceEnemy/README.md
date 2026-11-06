# PaceEnemy

横から進入し、レールの前方で並走しながら通常弾を撃ち、反対側へ離脱する敵。
前進・ブースト・ブレーキ・レールの曲がりに追従する。
自機の手動の上下左右移動には追従しないため、照準を合わせたり弾を回避したりできる。
待機中は非表示・当たり判定なし。射撃は並走中のみ。離脱完了時は爆発せず削除する。
HP・射撃間隔・弾速は既存の `level` 設定を使い、放射弾は撃たない。
見た目は白いCUBE。表示スケールはXYZとも4.0（通常敵の2倍）。
当たり判定は表示スケールに連動する回転対応の箱（現在は各辺4.0、半サイズ2.0）。
弾・照準・爆風・自機接触で同じ箱を使い、当たり判定のデバッグ表示も箱になる。

## 配置例

ステージのscene配列へ追加する。Stage1には以下の6体を配置済み。

| 配置点の奥行き | 構成 | 並走時間 |
|---|---|---|
| 460 | 右から進入、やや左上で並走 | 3秒 |
| 1200 | 左から進入、右上で並走 | 5秒 |
| 2200 / 2260 | 左右から時間差、上下・前方距離も異なる2体 | 4.5秒 / 4秒 |
| 3650 | 右から進入、上側で大きく左右移動 | 5秒 |
| 5100 | 左から進入、ボス前の並走 | 4秒 |

いずれも配置点の120手前で進入開始。通常前進ではPaceEnemyの同時出現は最大2体。

```json
{
  "name": "Enemy_Pace_01",
  "type": "EnemySpawn",
  "file_name": "PaceEnemy",
  "level": 1,
  "paceEnemy": {
    "entrySide": "right",
    "activationDistance": 120.0,
    "entryDurationSeconds": 1.5,
    "holdDurationSeconds": 6.0,
    "exitDurationSeconds": 1.5,
    "forwardOffset": 100.0,
    "offsetX": -8.0,
    "offsetY": 4.0,
    "swayAmplitude": 12.0,
    "swayPeriodSeconds": 4.0,
    "sideTravelDistance": 80.0
  },
  "transform": {
    "translation": [0.0, 400.0, 0.0],
    "rotation": [0.0, 0.0, 0.0]
  }
}
```

`translation` は既存と同じ「横・奥行き・高さ」。出現開始の判定用の配置点。
実際の飛行位置は `paceEnemy` のオフセットで指定する。
`offsetX` は右が正、`offsetY` は上が正、`forwardOffset` はレール進行方向が正。
左右移動は `offsetX ± swayAmplitude`。上下と前方のオフセットは固定する。

| 設定 | 意味 | 既定値 / 有効範囲 |
|---|---|---|
| entrySide | 進入側。離脱は反対側 | right / left・right |
| activationDistance | 配置点までの前方距離がこの値以下になると開始 | 120 / 10〜500 |
| entryDurationSeconds | 進入時間（秒） | 1.5 / 0.1〜30 |
| holdDurationSeconds | 前方で並走する時間（秒）。進入・離脱時間は含まない | 6 / 0〜120 |
| exitDurationSeconds | 離脱時間（秒） | 1.5 / 0.1〜30 |
| forwardOffset | レール基準位置からの前方距離 | 100 / 10〜150 |
| offsetX | 左右移動の中心 | 0 / −80〜80 |
| offsetY | 上下位置 | 0 / −80〜80 |
| swayAmplitude | 左右移動の片幅。0なら横揺れなし | 12 / 0〜40 |
| swayPeriodSeconds | 左右移動一往復の秒数 | 4 / 0.1〜60 |
| sideTravelDistance | 進入・離脱端の、offsetXからの横距離 | 80 / 40〜500 |

不正なJSON項目は警告を出してその項目の既定値を使う。
通常敵と同じく、自機の前方かつ射程170以内で射撃する。
遠い前方距離と大きなXYを組み合わせると射程外になる場合がある。
