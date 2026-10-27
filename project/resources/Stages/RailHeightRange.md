# レール移動の高度設定

各ステージの `settings.json` の `rail` 内で指定する。
最低・最高高度の選択は `GamePlayScene::UpdatePlayerRailHeightRange`、
範囲の計算は `CalculateRailHeightRange`、移動時の補正は
`PlayerMovement::EnforceHeightRange` にまとめている。

```json
"height_range": {
  "minimum_offset_y": -15.0,
  "maximum_offset_y": 15.0,
  "use_floor_minimum": true,
  "floor_clearance": 3.0
},
"lower_height_range": {
  "minimum_offset_y": -15.0,
  "maximum_offset_y": 15.0,
  "use_floor_minimum": false
},
"height_zones": [
  {
    "start_distance": 2000.0,
    "end_distance": 3000.0,
    "is_lower_route": true,
    "minimum_offset_y": -20.0,
    "maximum_offset_y": 25.0
  }
]
```

- `minimum_offset_y` / `maximum_offset_y`: その地点のレール中心のワールドYからの移動可能量。下限は上限未満にする。
- `use_floor_minimum`: trueなら「ステージの床高度＋分岐の高さ補正＋floor_clearance」でも下限を制限する。
- `floor_clearance`: 床からの余裕。0以上。海底モデルの形状を自動判定する設定ではない。
- `height_range`: ステージの基本設定。省略時は上下15、床基準の制限なし。
- `lower_height_range`: 下ルートの設定。省略項目は基本設定を継承する。
- `height_zones`: 場所ごとの上書き。開始距離を含み、終了距離を含まない。`is_lower_route`は省略時false（主ルート）。省略項目は対象ルートの設定を継承する。重複時は後の区間を優先する。

下降・合流中は、分岐レールの深さに応じて主ルートと下ルートの上下限を補間する。
Stage1では海上の海面＋3を維持し、最深部ではレール中心から上下15ずつ動ける。
区間設定を別の値へ変える境界では、上下限を即時に切り替える。

画面端の余白による制限は別に適用されるため、設定した高度まで必ず動けるとは限らない。
全方位飛行モードは既存の `flight.min_height` / `flight.max_height` を使う。
