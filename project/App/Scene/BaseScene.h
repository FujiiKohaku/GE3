#pragma once


// シーン基底クラス
class BaseScene {
public:
    // 各シーンが必ず実装すべき基本処理
    virtual void Initialize() = 0;

    virtual void Finalize() = 0;

    virtual void Update() = 0;

    virtual void Draw2D() = 0;
    virtual void Draw3D() = 0;
    virtual void DrawImGui() = 0;

    // 仮想デストラクタ（必須）
    virtual ~BaseScene() = default;
};
