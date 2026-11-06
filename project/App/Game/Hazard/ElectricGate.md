# ElectricGate

Stage1に設置した、白い支柱と上下2段のオレンジ色の放電のゲート。
常時放電し、電撃に接触するとHPが1減る。既存の被弾音・演出と1秒間の無敵時間を適用する。
予兆と周期的なオン・オフは未実装。

- 配置距離: `resources/Stages/Stage01/settings.json` の `electric_gate_distances`。現在は400。
- 表示倍率: `GamePlayScene::InitializeElectricGates` の `kDisplayScale`。現在は5。
- 支柱下部の水没量: 同関数の `kBaseSubmersion`。現在は4。
- 放電形状更新間隔: `ElectricGate.cpp` の `kShapeIntervalSeconds`。現在は0.06秒。
- ダメージ: `ElectricGate::kContactDamage`。現在は1。
- 当たり判定は主放電の揺れを覆う上下2本の固定帯。帯の間は安全。枝分かれと支柱は判定対象外。
- プレイヤーの半径は通常の障害物と同じ1。前フレームからの移動区間を調べ、高速通過時のすり抜けも防ぐ。
- 各段18区間の主放電と3本の枝分かれを、固定サイズの単一メッシュにまとめる。
- 初期化時に確保した頂点バッファを書き換える。更新中にオブジェクトやバッファを新規確保しない。
- 放電は両面描画し、深度書き込み、影、レイトレーシングへの登録を無効にする。
- 元のBlenderファイルと書き出しスクリプトは `output/electric_gate_v1`。
