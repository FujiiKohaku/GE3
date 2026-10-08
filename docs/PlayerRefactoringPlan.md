# Playerクラス分割計画書

作成日：2026-10-06  
状態：第1〜6段階のコード整理を実施。ビルド結果と動作確認範囲は末尾の実施記録に記載。

## 目的

Playerが担当している射撃・照準・移動・HP処理を役割ごとに整理する。現在の操作感、更新順序、公開APIを維持し、変更したい処理の置き場所が分かる構成にする。

行数を減らすことだけを目的にせず、処理とその状態を同じ担当クラスにまとめる。最初は既存クラス内で関数を整理し、確認後に段階的に担当を移す。

## 現状

Player.cppは約1,109行。特に長い部分は以下のとおり。空行・コメントを含む概算。

| 処理 | 行数 | 主な責任 |
|---|---:|---|
| Update | 173 | 入力、ブースト、武器、熱管理、移動、弾更新、デバッグ描画 |
| UpdateAllRangeMove | 72 | 自由移動、自動帰還、高度制限、機体の傾き |
| ClampRailOffsetToScreen | 68 | 画面内に収める座標補正 |
| FireSingleBullet | 57 | 弾生成、照準、発射位置、誘導設定、エフェクト |
| UpdateHomingTarget | 56 | 候補選択、並べ替え、ロック登録 |
| FireBullet | 53 | 発射可否、待ち時間、一斉発射、音、熱量加算 |
| UpdateRolling | 53 | 二度押し判定、回転、待ち時間 |

GamePlaySceneがPlayerの位置・回転を更新し、Player::Updateとは別にFireBulletも呼び出している。GameplayCollisionSystemはGetBulletsで弾を参照している。この接続を維持する必要がある。

## 分割後の担当

配置先はproject/App/Game/Player/配下とする。以下の名前は実装時の設計候補。

| クラス | 所有する状態・担当 | 主な移動対象 |
|---|---|---|
| Player | 外部との窓口、Object3d、transform、Camera参照、全体の更新順序 | Initialize、Update、Draw、DrawShadow、各公開API |
| PlayerWeaponController | 武器選択、発射間隔、弾の所有・更新・描画、発射音とエフェクト | FireBullet、FireSingleBullet、CreateBullet、UpdateWeaponSwitch、UpdateBullets、RemoveDeadBullets |
| PlayerWeaponHeat | 熱量、冷却、満タン後の猶予、発射停止・復帰判定 | Update内の熱管理、FireBullet内の熱量加算 |
| PlayerHomingLock | 敵候補の共有、ロック対象、押下・解放の履歴、ロック音 | SetHomingTargets、UpdateHomingTarget、GetHomingLockPositions |
| PlayerAimController | 照準座標、照準距離の平滑化、Ray生成、画面内照準制限 | UpdateMouseAim、ClampAimScreenPosition、CreateAimRay、UpdateSmoothedAimDistance、ResolveAimPoint |
| PlayerMovementController | 操作モード、感度、速度、レール基準、自由飛行、ブースト、ローリング | UpdateKeyboardMove、UpdateStarFoxMove、UpdateAllRangeMove、UpdateRolling、画面内の機体補正 |
| PlayerHealth | HP、無敵時間、死亡段階、落下時間・速度 | ApplyDamage、Heal、UpdateDeathAnimation、死亡・HP判定 |

PlayerWeaponControllerがPlayerWeaponHeatとPlayerHomingLockを所有する。Playerは武器・照準・移動・HPの担当を直接保持する。担当クラスは原則として値で保持し、分割だけのための毎フレームの動的確保や仮想関数は追加しない。

レティクル・オーバーヒートゲージの描画は現在のGamePlaySceneとシェーダーに残す。Playerの分割とHUDの作り直しは別作業にする。

## 状態と依存関係のルール

- transformの所有者はPlayerに一本化する。移動・死亡処理には必要な参照を渡し、別の位置や回転を重複保持しない。
- 担当クラスへPlayer全体の参照を渡して自由に内部へアクセスさせない。Camera、照準情報、姿勢、入力など必要な値・参照を渡す。
- 照準担当は画面座標やRay・照準点を提供する。武器担当はそれを利用して発射する。
- 機体を画面内へ補正する処理は移動担当に置く。マウス照準の画面内制限と区別する。
- ロック対象の共有コンテナと弾への受け渡しは、既存のshared_ptrによる寿命管理を維持する。敵が破棄される前の候補更新も維持する。
- 既存のGetBullets、GetRailOffsetなど参照を返すAPIは、移動先の安定したメンバーへの参照を返す。一時オブジェクトを返さない。
- Cameraとデバッグカメラの所有権は変更しない。SetCameraは必要な担当と既存の弾へ伝播させる。
- 公開APIはPlayerに残して担当クラスへ委譲する。GamePlaySceneや衝突処理に担当クラスを直接操作させない。

## 作業順序

### 第1段階：Player内の関数整理

Updateからブースト、ロック操作、熱管理、連射入力、移動選択、デバッグ描画を名前付き関数へ切り出す。まだ状態を他クラスへ移さない。

特に、現在はロック解放による発射が熱更新より先にあり、連射処理は熱更新より後にある。まとめ方によって順番が変わらないようにする。死亡中は弾更新と死亡アニメーションを続け、通常の入力処理に進まない分岐も維持する。

成果：Updateを読めば更新順序が分かる。関数を短くするために呼び出し順を変更しない。

### 第2段階：熱管理を分離

PlayerWeaponHeatへ熱量と関連定数・タイマーを移す。Update、AddShotHeat、GetHeatRatio、IsOverheatedなどの小さなAPIを用意する。

維持する仕様：通常弾とミニガンで共有、発射成功時に加算、満タン後0.5秒の猶予、猶予中の満タン維持、射撃停止で猶予解除、オーバーヒート後は0まで冷却して復帰。0.5秒は60fpsの30フレーム相当であり、実フレーム数の固定カウンターへ変更しない。

成果：熱管理だけを読んで状態遷移を追える。HP・移動から独立した検証ができる。

### 第3段階：照準を分離

PlayerAimControllerへ照準座標と平滑化した距離を移す。画面サイズ、Camera、必要な入力を渡す。弾生成側でのRayと発射位置の組み合わせは現状を維持する。

成果：照準計算を射撃・移動から区別できる。

### 第4段階：ロックオン・武器を分離

先にPlayerHomingLockへ対象管理を移し、次にPlayerWeaponControllerへ弾と発射処理を移す。第2段階の熱管理も武器担当のメンバーへ移す。

Player::FireBulletを窓口として残す。GamePlayScene::ProcessPlayerShootingからのクリック発射経路も維持する。複数の発射経路の一本化や重複発射の調整は、挙動変更になるため別途提案する。

成果：武器の変更時に、Playerの移動処理を追う必要がなくなる。

### 第5段階：移動を分離

PlayerMovementControllerへレール移動・自由飛行・ブースト・ローリングと関連変数をまとめる。その内部で、操舵、自動帰還、高度制限、位置補正、機体の傾きを名前付き関数に分ける。

外部からのSetRailFrame、ApplyRailAreaForce、SetControlMode、SetRotateなどを維持する。GamePlaySceneも姿勢・位置を補正しているため、ローリング中の回転保護やレール更新との順序を重点確認する。

成果：各移動モードの流れを個別に読める。最初から移動モードごとの継承階層は作らない。

### 第6段階：HP・死亡処理と公開APIを整理

PlayerHealthへHP・無敵・死亡状態を移す。死亡時のtransform更新はPlayerが渡した参照に反映する。点滅描画、爆発準備判定、開発用無敵設定を維持する。

最後にPlayer.hの公開APIを初期化、姿勢、操作、武器、HP、描画の順に整理する。新しいcppをKohakuEngine.vcxprojへ登録し、必要に応じてフィルターも更新する。

成果：Playerは外部APIと担当クラスの連携を中心とする。

## 実行コストの見込み

| 項目 | 見込み |
|---|---|
| CPU | 同じ計算を担当関数へ移す。関数呼び出しは増えるが、最適化でインライン化される可能性がある。速度改善は約束しない |
| GPU | 描画回数、弾・ゲージのシェーダー、描画領域を変更しない |
| メモリ | 値で担当クラスを保持し、状態の二重保持を避ける。アラインメントによる増加は実装後に確認する |
| 毎フレームの確保 | 分割による新規確保は追加しない。既存の弾生成やロック候補コンテナの確保は維持する |
| ビルド | ソースファイル数は増える。変更対象を限定しやすくなるが、ビルド時間の短縮は未計測 |

各段階のコード変更前に、その段階で実際に選ぶ構成とコストを説明する。計測していない処理時間を数値で断定しない。

## 確認項目

各段階でビルドと対象機能の動作確認を行い、最後に全項目を確認する。

| 対象 | 確認する操作・結果 |
|---|---|
| 通常弾・ミニガン | クリック、Space、長押し、発射間隔、ダメージ、熱量加算が維持される |
| 熱管理 | 満タン直後は射撃可能、猶予満了で停止、射撃停止で回避、0まで冷えて復帰、武器切替・再加熱・ポーズでも状態が破綻しない |
| ミサイル | 発射間隔、武器切替、発射音・エフェクトを維持する |
| ホーミング | 押下開始、最大6体のロック、解放発射、敵の死亡・候補削除後も安全に動く |
| 照準 | マウス移動、画面端、着弾方向、距離の平滑化、カメラ設定後の動作が維持される |
| 移動 | 両操作モード、レール移動、自由飛行、自動帰還、高度制限、ブースト、左右ローリングを確認する |
| HP・死亡 | 被弾、無敵時間、回復、HP0で操作停止、落下、爆発準備、死亡後の弾更新を確認する |
| 外部接続 | 衝突処理のGetBullets、HUDの熱量・HP取得、開発用設定、リトライ・シーン再生成を確認する |
| 構成 | Debug・Development・Releaseで条件付きコードと新規ファイル登録を確認する |

熱管理の境界条件など、分割で壊れやすい振る舞いには小さな単体テストを追加する。単純な委譲関数をなぞるだけのテストは増やさない。

現在、全体ビルドにはVisual C++のC1902（デバッグ情報の環境エラー）が発生している。構文チェックとシェーダーコンパイルの成功を、全体ビルド・ゲーム動作確認の代わりに扱わない。実装時に環境を再確認し、未確認項目は明記する。

## 実装時の制約

- 三項演算子・ラムダ式を新たに使用しない。
- 移動対象の既存ラムダは名前付き関数や比較用構造体、通常のループへ置き換え、既存の選択・並べ替え結果を維持する。
- 変数・引数はcamelCase、メンバーはcamelCase末尾に_、定数はk＋PascalCase、型・関数はPascalCase。
- boolはis・has・shouldなどで意味を表し、時間・フレーム数・距離には必要な単位を示す。
- 設定構造体の公開データには末尾の_を付けない。外部APIの名前は仕様に従う。
- 命名変更は移動・整理する範囲に限定する。
- 弾性能、冷却速度、入力、HUDの見た目を、この整理作業で変更しない。

## 完了条件

Player::Updateから処理順序が読み取れ、各状態の所有者が一つに定まっている。既存公開APIを通じてシーン・衝突処理が動き、確認項目の結果と残る制限が記録されている。行数目標のために関数やクラスを細分化しない。

最初の実装範囲は第1段階とする。その確認後、第2段階以降を順番に進める。

## 実施記録

### 第1段階：関数の切り出しを実施（2026-10-06）

- UpdateBoost、UpdateHomingFireInput、UpdateWeaponHeat、UpdateFireInput、UpdateMovementInput、DrawAimDebugLinesをPlayer内に追加。
- Updateは約173行から約71行に整理。状態の所有者・公開APIは変更していない。
- ロック解放発射、ミサイル待ち時間更新、熱更新、連射、移動、姿勢反映、弾更新の順序を維持。
- Player.cppのDebug・Development・Releaseの構文チェック、およびGamePlayScene.cppのDebug構文チェックは成功。
- 全体ビルド・ゲーム上の動作確認は未実施。以前の全体ビルドではC1902が発生しているため、構文チェックのみで動作確認済みとは扱わない。
- 第2段階以降のクラス分離は未着手。

### 第2段階：熱管理のクラス分離を実施（2026-10-06）

- PlayerWeaponHeat.h/.cppに熱量、冷却、猶予タイマー、復帰判定を移動。Playerが値で保持する。
- PlayerのGetHeatRatio、IsOverheatedの公開APIを維持し、内部で委譲する。発射成功後にAddShotHeatを呼ぶ。
- 新規ファイルをvcxprojとfiltersへ登録。
- PlayerWeaponHeatTestsをコンパイル・実行して成功。通常弾とミニガンの加熱量、0.5秒の猶予、追加射撃による猶予延長の防止、射撃停止による猶予解除と再加熱、0までの冷却、停止時間を確認。
- Player.cppとPlayerWeaponHeat.cppのDebug・Development・Release構文チェック、およびGamePlayScene.cppのDebug構文チェックは成功。
- 全体ビルド・ゲーム上の動作確認は未実施。第3段階以降は未着手。

### 第3段階：照準のクラス分離を実施（2026-10-06）

- PlayerAimController.h/.cppに照準座標、距離平滑化、マウス座標取得、画面内制限、Ray生成、照準点計算を移動。
- Playerは値で保持し、GetAimScreenPositionは担当クラス内の座標への参照を返す。
- ロックオンと操舵も同じ座標を参照する。照準距離更新、射撃、マウス座標更新、操舵、照準制限の既存順序を維持。
- 平滑化にはPlayerからdeltaTimeSecondsを渡す。Cameraの所有権は変更していない。
- 重複していた照準初期化をInitializeへ集約。既存の制限幅64px・距離220・平滑化速度12を維持。
- 新規ファイルをvcxprojとfiltersへ登録。
- Player.cppとPlayerAimController.cppのDebug・Development・Release構文チェック、およびGamePlayScene.cppのDebug構文チェックは成功。
- 全体ビルド・ゲーム上の動作確認は未実施。第4段階以降は未着手。

### 第4〜6段階：武器・移動・HPを分離（2026-10-06）

| クラス | 実施した分離 |
|---|---|
| PlayerWeaponController | 武器選択、入力と発射、弾の所有・更新・描画、熱管理の所有、発射デバッグ描画 |
| PlayerHomingLock | 候補共有、ロック対象、押下・解放の履歴、ロック位置取得 |
| PlayerMovementController | レール移動、自由飛行、ブースト、ローリング、機体の画面内補正 |
| PlayerHealth | HP、無敵時間、点滅描画判定、死亡段階と落下アニメーション |

- Player.cppは約203行。姿勢とObject3dを所有し、公開APIと更新順序の窓口として残した。
- 姿勢はPlayerだけが所有する。移動担当はその参照を保持し、死亡担当は更新時に参照を受け取る。参照先を壊すコピー・ムーブをPlayerで禁止した。
- 武器担当は姿勢・照準・移動・HPの必要な参照だけを受け取り、Player全体には依存しない。
- PlayerWeaponHeatとPlayerHomingLockは武器担当内に値で保持する。
- 死亡時はPlayerが移動停止とロック解除を行う。死亡後も弾を更新する。
- 自由飛行を操舵、自動帰還、高度制限、位置更新、機体の傾きの関数へ分割した。
- 移動対象のロック候補比較・候補削除のラムダを名前付き比較関数と通常のループへ置き換えた。
- シーンからの直接発射経路、GetBulletsなど参照を返すAPI、Player::ControlModeによる指定を維持した。
- カメラ設定は担当クラスと既存の弾へ伝播する。
- 新規クラスをvcxprojとfiltersへ登録。HPと移動のテストソースおよびテスト用プロジェクトも追加した。

確認済み：熱管理テスト、HP・無敵・回復・死亡・開発用無敵設定のテスト、レール基準と範囲制限・姿勢への反映・自由飛行への切替・操作感度の移動テスト。移動テストは実際のエンジンオブジェクトにリンクして実行した。

通常のDebugビルドはC1902で失敗。コンパイルを/Z7に変更した検証用ビルドではコンパイル成功後、リンク時にLNK1101（MSPDB140.DLLの不整合）が発生した。検証用ターゲットでコンパイル情報を/Z7、リンクのPDB生成を無効にすると、Debugは全体ビルド成功。通常のプロジェクト設定は変更していない。

検証用設定：generated/PlayerRefactoringBuild.targets。ビルド時にForceImportBeforeCppTargetsで指定する。検証用設定ではPDBを生成しないため、通常設定のデバッグ情報生成が成功したことにはならない。

| 構成 | 検証用ビルド | ログ |
|---|---|---|
| Debug / x64 | コンパイル・リンク成功 | generated/PlayerRefactoring-Debug-Embedded.log |
| Development / x64 | コンパイル・最適化・リンク成功 | generated/PlayerRefactoring-Development.log |
| Release / x64 | コンパイル・最適化・リンク成功 | generated/PlayerRefactoring-Release.log |

PlayerHealthTests.vcxprojのDebugビルドと実行も成功。移動テストはDebugのエンジンオブジェクトと同じランタイム設定でリンクして実行し、成功した。

ゲーム上の見た目・操作感を確認する手動プレイは未実施。単体テストで未検証のロックオン、連射、自動帰還、ローリングについては、ビルド成功だけを動作確認済みとして扱わない。

### Releaseのデバッグ情報を再生成（2026-10-06）

- 利用者からReleaseのvc145.pdbの型レコード破損が報告された。
- 通常環境で小さなテストを/Ziと/DEBUGでビルドし、PDB生成・リンク・テスト実行が成功することを確認した。
- Releaseの中間生成物をgenerated/PdbRepairBackup/Release-20261006へ退避し、既存の出力PDBも同じ場所へ保存した。
- 検証用設定を指定せず、Release/x64を通常設定でRebuildした。全ソースのコンパイル・最適化・リンク・実行ファイル生成が成功した。
- ビルドログで/Ziと/DEBUGを確認した。前回のPDB無効化による確認不足を修正し、ゲームコード・通常のプロジェクト設定は変更していない。
- ログ：generated/PdbRepair-Release.log。
- シンボル検証は一時的に自動承認レビューの混雑で止まったが、再試行で実行できた。symchk /pfでReleaseの実行ファイルと完全なソース情報を含むPDBの一致を確認し、失敗0件。
- cdbの起動時停止位置でPlayer、PlayerMovementController、PlayerWeaponController、PlayerHomingLock、PlayerAimController、PlayerHealth、PlayerWeaponHeatの型情報を読み取り、各メンバーとオフセットの取得に成功。ゲーム本体の処理開始前に終了した。
- デバッガー検証ログ：generated/PdbRepair-Debugger.log。Visual Studio側で既に読み込まれた古いシンボルは、次回のデバッグ開始で再読み込みする。

### ファイル配置の整理（2026-10-06）

Player直下にはPlayer.cppとPlayer.hを残し、物理フォルダーとVisual Studioのフィルターを次のように整理した。include、vcxproj、テストプロジェクトの参照も更新した。

```text
project/App/Game/Player/
├─ Player.cpp / Player.h
├─ Aim/       PlayerAimController
├─ Movement/  PlayerMovementController
├─ Weapon/    PlayerWeaponController、PlayerWeaponHeat、PlayerHomingLock
├─ Health/    PlayerHealth
└─ Bullet/    既存の弾クラス
```

処理内容・公開API・実行コストは変更していない。

移動後の通常Release/x64ビルドは、PDBを有効にした設定でコンパイル・リンク成功。ログ：generated/PlayerFolderOrganization-Release.log。

### Movementの責務分割（2026-10-06）

- PlayerMovementController.cppを478行から249行へ整理した。
- Movement/PlayerScreenConstraint：画面内への補正、レール座標からワールド座標への変換。
- Movement/PlayerRollController：ダブルタップ判定、ロール回転、クールダウン。
- Movement/PlayerSteeringController：マウス位置の入力変換、デッドゾーン、感度、操舵の平滑化。
- PlayerMovementControllerには移動モードの選択、ブースト、レール移動、全方向飛行と飛行範囲への復帰を残した。
- 各部品は値で保持し、動的確保・描画処理は追加していない。既存の計算とタイミングを維持し、実行コストの増加は小さな関数委譲が中心。
- 自身のレール座標系への参照を持つため、Movementのコピー・ムーブを禁止した。
- vcxprojとVisual StudioのMovementフィルターに3組のファイルを登録した。
- ロール時間・クールダウン・タップ期限、操舵デッドゾーン・感度・時間刻みのテスト成功。
- 既存のレール移動・全方向飛行初期化に加え、Cameraを使った画面内補正とレール基準位置の更新のテスト成功。テストではSetProjectionJitterで行列を再計算し、GPU初期化は行わない。
- 最終状態の通常Release/x64ビルドはPDB有効のままコンパイル・リンク成功。ログ：generated/PlayerMovementSplit-Release.log。ゲームを操作しての体感確認は未実施。

### Movement内のフォルダー整理（2026-10-08）

移動処理のまとめ役はMovement直下に残し、補助クラスは責務ごとに分けた。

```text
Movement/
├─ PlayerMovementController.cpp / .h
├─ Roll/             PlayerRollController.cpp / .h
├─ Steering/         PlayerSteeringController.cpp / .h
└─ ScreenConstraint/ PlayerScreenConstraint.cpp / .h
```

include、vcxproj、Visual Studioのフィルター、既存テストの参照を更新した。
処理内容と実行コストは変更していない。
移動後の通常Release/x64ビルドはコンパイル・リンク成功。ログ：generated/MovementFolderOrganization-Release.log。

### Player更新と状態管理の修正（2026-10-08）

- 最新の照準位置を取得・画面内制限・距離補間してから移動し、移動後の銃口から発射する順序に変更。
- 生成直後の弾のUpdateを削除し、通常更新で1回だけ進める。
- FireBulletは発射成功をboolで返す。ホーミングロックは発射成功時だけ解除する。
- 連射間隔・ミサイル待機・ロール・被弾後の無敵を経過秒数に変更。60FPS時の長さを維持する。
- SetTranslateでレール基準位置、SetRotateで全方向飛行の内部角度と方向を同期する。
- ロールと無敵時間について30/60/120FPSで検証成功。既存HP・操舵・画面補正のテスト、位置・姿勢同期のテスト成功。
- ゲーム操作による照準・発射・ホーミングの体感確認は未実施。
最終状態の通常Release/x64ビルド成功。ログ：generated/PlayerDesignFix-Release.log。
