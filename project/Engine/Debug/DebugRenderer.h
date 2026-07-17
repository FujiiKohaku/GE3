#pragma once

#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl.h>

class DirectXCommon;
struct Skeleton;

// DebugLine は、CPU側で「どこからどこまで線を描きたいか」を覚えておくためのデータです。
// AddLine() が呼ばれるたびに、この構造体を lines_ に追加します。
// 実際にGPUへ送る頂点データは Draw() の中で DebugLine から作ります。
struct DebugLine {
    Vector3 start;
    Vector3 end;
    Vector4 color;
    float thickness;
};

// DebugRendererの役割
// ------------------------------------------------------------
// DebugRenderer は、ゲーム中の確認用ラインをまとめて描画するための専用マネージャーです。
// レールシューティングのレール可視化、当たり判定の向き、レイキャストの結果など、
// 「ゲーム本番の見た目」ではなく「開発者が状態を確認するための描画」を扱います。
//
// 使い方は毎フレーム AddLine() で描きたい線を登録し、そのフレームの Draw() でまとめて描画します。
// Draw() 後に lines_ をクリアするため、永続的に表示したい線も毎フレーム AddLine() してください。
//
// 将来的に AddSphere(), AddBox(), AddCapsule(), AddArrow() を追加するときも、
// まずCPU側に「描きたい形状の情報」をためて、Draw() でまとめてGPUへ送る形に拡張できます。
class DebugRenderer {
public:
    static DebugRenderer* GetInstance();
    static void Finalize();

    void Initialize();

    void Update();

    void Draw();

    void AddLine(
        const Vector3& start,
        const Vector3& end,
        const Vector4& color,
        float thickness = 1.0f);

    void AddOverlayLine(
        const Vector3& start,
        const Vector3& end,
        const Vector4& color,
        float thickness = 1.0f);

    void AddSkeleton(
        const Skeleton& skeleton,
        const Matrix4x4& worldMatrix);

    void AddSkeleton(
        const Skeleton& skeleton,
        const Matrix4x4& worldMatrix,
        const Vector4& color,
        float thickness = 2.0f);

    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }

    ~DebugRenderer();

private:
    struct DebugLineVertex {
        Vector3 position;
        Vector4 color;
        float thickness;
    };

    struct ViewProjectionData {
        Matrix4x4 viewProjection;
    };

    struct ScreenData {
        Vector2 screenSize;
        Vector2 padding;
    };

private:
    static std::unique_ptr<DebugRenderer> instance_;

    DebugRenderer() = default;
    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void EnsureVertexCapacity(std::size_t vertexCount);
    void UploadLineVertices();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> overlayPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    ViewProjectionData* viewProjectionData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> screenResource_;
    ScreenData* screenData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    DebugLineVertex* vertexData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    uint32_t vertexCapacity_ = 0;

    bool isInitialized_ = false;
    bool isVisible_ = true;

    // 1フレーム分のライン情報を保持
    // ------------------------------------------------------------
    // ここに入っているのは「CPU側で生成するデータ」です。
    // AddLine() は GPU を直接触らず、まずこの配列に描きたい線を追加します。
    // Draw() でまとめて GPU 用の頂点データへ変換することで、呼び出し側は手軽にデバッグ線を登録できます。
    std::vector<DebugLine> lines_;
    std::vector<DebugLine> overlayLines_;
};
