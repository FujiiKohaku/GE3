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
#include "Engine/Camera/Camera.h"
#include "Engine/D3DResourceLeakChecker/D3DResourceLeakChecker.h"
#include "Engine/debugcamera/DebugCamera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/input/Input.h"
#include "Engine/math/MatrixMath.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/ModelCommon.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3D.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Winapp/Utility.h"
#include "Engine/Winapp/WinApp.h"

#include "App/Scene/GamePlayScene.h"
#include "App/Scene/TitleScene.h"
#include "App/Scene/BaseScene.h"
#include "App/Scene/SceneManager.h"

#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/PostEffect/OffscreenRenderer.h"

#include "Engine/PostEffect/CopyImageRenderer.h"
// ================================================================
// Game クラス
// ================================================================
class Game {
public:
    // ------------------------------
    // 基本処理
    // ------------------------------
    void Initialize();
    void Update();
    void Draw();
    void Finalize();

    WinApp* GetWinApp() const { return winApp_.get(); }

    bool IsEndRequest() const { return endRequest_; }

private:
    // Scene
    SceneManager* sceneManager_ = nullptr;

    // App
    std::unique_ptr<WinApp> winApp_;

    ImGuiManager* imguiManager_ = nullptr;

    // ------------------------------
    // Core システム
    // ------------------------------

    // ------------------------------
    // グラフィック / モデル
    // ------------------------------
    ModelCommon modelCommon_;
    DebugCamera debugCamera_;

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
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;
    std::unique_ptr<CopyImageRenderer> copyImageRenderer_;
};