#pragma once
// ======================= ヘッダー・ライブラリ関連 ==========================
#define _USE_MATH_DEFINES
// 標準ライブラリ//
#include "Camera.h"
#include "D3DResourceLeakChecker.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "MatrixMath.h"
#include "Model.h"
#include "ModelCommon.h"
#include "Object3D.h"
#include "Object3dManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "Unknwn.h"
#include "Utility.h"
#include "WinApp.h"
#include "modelManager.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
// Windows・DirectX関連
#include <Windows.h> //ウィンドウAPIで消す
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
// 外部ライブラリ//
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
// リンカオプション
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
class Game {
public:
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon);
    void Update();
    void Draw();
    void Finalize();

private:
    DirectXCommon* dxCommon_ = nullptr;
    WinApp* winApp_ = nullptr;
    Input* input_ = nullptr;
    SoundManager soundManager_;
    Object3dManager* object3dManager_ = nullptr;
    SpriteManager* spriteManager_ = nullptr;
    DebugCamera debugCamera_;
    ModelCommon modelCommon_;
    std::vector<Sprite*> sprites_;
    Object3d object3d_, player2_, enemy_;
    SoundData bgm;
    bool isEnd = false;
    Camera* camera_;
    Sprite* sprite_ = nullptr;
};
