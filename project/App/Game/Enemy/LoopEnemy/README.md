# LoopEnemyの配置設定

ステージJSONの一つの`EnemySpawn`から編隊を生成します。

```json
{
  "name": "Enemy_Loop_Trial",
  "type": "EnemySpawn",
  "file_name": "LoopEnemy",
  "loopEnemy": {
    "count": 3,
    "entrySide": "right",
    "spawnIntervalSeconds": 0.6,
    "activationDistance": 120.0
  },
  "transform": {
    "translation": [0.0, 180.0, -22.0],
    "rotation": [0.0, 0.0, 0.0]
  }
}
```

| 設定 | 意味 | 範囲・既定値 |
| --- | --- | --- |
| `count` | 一つの配置点から出る敵の数 | 整数1〜16、既定1 |
| `entrySide` | 自機から見た進入側。`left`では軌道全体を左右反転 | `left` / `right`、既定`right` |
| `spawnIntervalSeconds` | 後続機が飛び始める間隔（秒）。0で全機同時 | 0〜10、既定0.6 |
| `activationDistance` | 配置点までの前方距離がこの値以下になると編隊の出現を開始 | 40〜150、既定120 |

`translation`の順序は既存ステージと同じ「横・奥行き・高さ」です。
全機が同じ前方距離を保ち、同じ軌道を時間差で飛びます。待機中は非表示で、衝突判定と射撃も停止します。
設定を省略すると以前と同じ右から1体の動作です。不正な項目は警告をログに出し、その項目の既定値を使います。
軌道は進入2秒、一周4秒、離脱2秒。弾は通常敵の通常弾です。
