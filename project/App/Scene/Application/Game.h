// エンジン機能とシーンを起動し、毎フレームの入力・更新・描画と終了時の解放を統括する。
#pragma once
// ======================= 標準ライブラリ ==========================
#define _USE_MATH_DEFINES
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>

// ======================= Windows・DirectX関連 =====================
#include <Windows.h>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

// ======================= DirectXTex / ImGui =======================
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"

// ======================= リンカオプション =========================
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// ======================= 自作エンジン関連 =========================
#include "Engine/3D/Model.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/D3DResourceLeakChecker/D3DResourceLeakChecker.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Light/LightManager.h"
#include "Engine/input/Input.h"
#include "Engine/math/MatrixMath.h"

#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/FontManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3D.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Winapp/Utility.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/audio/SoundManager.h"

#include "App/Scene/Common/BaseScene.h"
#include "App/Scene/Gameplay/GamePlayScene.h"
#include "App/Scene/Common/SceneManager.h"
#include "App/Scene/Title/TitleScene.h"

#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/Renderer/Renderer.h"
// ================================================================
// Game クラス
// ================================================================
class Game {
public:
    // ------------------------------
    // 基本処理
    // ------------------------------
    void Initialize();
    // 入力と各管理機能を更新し、現在のシーンを一フレーム進める。
    void Update();
    // 描画処理を実行し、更新済みのゲーム画面を表示する。
    void Draw();
    // シーンやエンジン機能を終了し、確保したリソースを解放する。
    void Finalize();

    WinApp* GetWinApp() const { return WinApp::GetInstance(); }

    bool IsEndRequest() const { return endRequest_; }

private:
#if defined(KOHAKU_RENDER_TESTS)
    friend class FxaaRenderTest;
#endif
    // 操作中にマウスがゲームの表示範囲から出ないようにする。
    void LockCursorToWindow();
    // ウィンドウへのマウス固定を解除し、通常のカーソル操作へ戻す。
    void UnlockCursor();

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    // 性能計測ログを記録するための出力先と開始状態を準備する。
    void InitializePerformanceLog();
    // フレームの計測値を集計し、一定の間隔で性能ログへ書き出す。
    void UpdatePerformanceLog();
#endif

    // Scene
    SceneManager* sceneManager_ = nullptr;

    // App

    ImGuiManager* imguiManager_ = nullptr;

    // ------------------------------
    // Core システム
    // ------------------------------

    // ------------------------------
    // グラフィック / モデル
    // ------------------------------
    ModelCommon modelCommon_;

    // game
    // GamePlayScene* scene_;
    TitleScene* scene_;
    // ------------------------------
    // ゲーム状態
    // ------------------------------
    bool isEnd_ = false;
    //------------------------------
    // パーティクル生成構造体
    //------------------------------
    //
    // マテリアル情報（色・ライティング・UV変換など）
    struct Material {
        Vector4 color; // 色
        int32_t enableLighting; // ライティング有効フラグ
        float padding[3]; // アライメント調整
        Matrix4x4 uvTransform; // UV変換行列
    };

    // 変換行列（WVP＋World）
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // 平行光源データ
    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    // マテリアル外部データ（ファイルパス・テクスチャ番号）
    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = 0;
    };

    // 頂点データ
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // モデル全体データ（頂点配列＋マテリアル）
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };

    // Transform情報（スケール・回転・位置）
    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    bool endRequest_ = false;
    std::unique_ptr<Renderer> renderer_;

    bool isMouseCursorVisible_ = false; // F2 preference
    bool isSystemCursorVisible_ = false;
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    bool showDebugUI_ = false;
#else
    bool showDebugUI_ = true;
#endif

#if defined(_DEBUG) || defined(ENABLE_PERFORMANCE_LOG)
    std::chrono::steady_clock::time_point performanceLogStartTime_;
    std::chrono::steady_clock::time_point performanceFrameStartTime_;
    uint64_t performanceLogProcessTime_ = 0;
    uint32_t performanceLogFrameCount_ = 0;
    uint32_t performanceLogProcessorCount_ = 1;
#endif
};
