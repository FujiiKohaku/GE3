# IceMotes

氷ステージ（stage03）専用の淡い青紫の光粒子。プレイヤー周囲に生成し、既存EffectManagerで更新・描画する。シーン終了時のStopAllEffectsで停止する。

- 最大128個、0.3秒ごとに2個、寿命12秒。最大個数は生成シェーダーでもEmitterの容量を参照する。
- 既存blue_spark.pngと共通粒子VS/PS、Snowの更新処理を再利用。加算描画、深度テストON、深度書き込みOFF。
- 発光強度2。既存HDR合成とBloomでにじませる。
- 新しい粒子バッファとエフェクト1系統の更新・描画が追加される。専用のGPU時間は未分離。

結晶の発光はStageIceCrystalのピクセルシェーダー、ボス周辺の局所霧と青紫の照明はGamePlayScene::Initializeで設定する。澄んだ遠景用の設定はresources/Graphics/visual-presets.jsonのstage03にある。
