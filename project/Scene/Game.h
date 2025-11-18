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
#include "Camera.h"
#include "D3DResourceLeakChecker.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "MatrixMath.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "Object3D.h"
#include "Object3dManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Utility.h"
#include "WinApp.h"
// ======================= パーティクル関連 =========================

// ================================================================
// Game クラス
// ================================================================
class Game {
public:
    // ------------------------------
    // 基本処理
    // ------------------------------
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw();
    void Finalize();

private:
    // ------------------------------
    // Core システム
    // ------------------------------
    DirectXCommon* dxCommon_ = nullptr;
    WinApp* winApp_ = nullptr;
    Input* input_ = nullptr;
    Camera* camera_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    // ------------------------------
    // グラフィック / モデル
    // ------------------------------
    ModelCommon modelCommon_;
    Object3dManager* object3dManager_ = nullptr;
    Object3d player2_;
    DebugCamera debugCamera_;
    // ------------------------------
    // スプライト
    // ------------------------------
    SpriteManager* spriteManager_ = nullptr;
    Sprite* sprite_ = nullptr;
    std::vector<Sprite*> sprites_;

    // ------------------------------
    // サウンド
    // ------------------------------
    SoundManager soundManager_;
    SoundData bgm;

    // ------------------------------
    // パーティクル
    Microsoft::WRL::ComPtr<ID3D12Resource> particleVB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleIB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleWVP_;
    D3D12_VERTEX_BUFFER_VIEW particleVBV_;
    D3D12_INDEX_BUFFER_VIEW particleIBV_;

    Matrix4x4* particleWVPData_;

    Microsoft::WRL::ComPtr<ID3D12Resource> particleMaterial_ = nullptr;
    Object3d::Material* particleMaterialData_ = nullptr;
    std::string particleTexture_ = "resources/uvChecker.png";

    // ------------------------------
    // ゲーム状態
    // ------------------------------
    bool isEnd_ = false;
    //------------------------------
    // パーティクル生成構造体
    //------------------------------
    struct Material {
        std::string textureFilePath; // テクスチャの場所
    };
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };
    struct ModelData {
        std::vector<VertexData> vertices; // 頂点4つ
        std::vector<uint32_t> indices; // 三角形2つ分の番号
        Material material; // テクスチャ
    };
    ModelData modelData;
};
