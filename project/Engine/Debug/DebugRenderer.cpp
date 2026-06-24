#include "DebugRenderer.h"

#include "Engine/3D/Object3dManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Math/MatrixMath.h"
#include "Engine/WinApp/WinApp.h"
#include "Engine/blend/BlendUtil.h"
#include <cassert>

std::unique_ptr<DebugRenderer> DebugRenderer::instance_ = nullptr;

DebugRenderer* DebugRenderer::GetInstance()
{
    if (!instance_) {
        instance_.reset(new DebugRenderer());
    }

    return instance_.get();
}

void DebugRenderer::Finalize()
{
    instance_.reset();
}

DebugRenderer::~DebugRenderer()
{
    if (vertexResource_) {
        vertexResource_->Unmap(0, nullptr);
    }

    if (viewProjectionResource_) {
        viewProjectionResource_->Unmap(0, nullptr);
    }

    if (screenResource_) {
        screenResource_->Unmap(0, nullptr);
    }
}

void DebugRenderer::Initialize()
{
    // DebugRendererの役割
    // ------------------------------------------------------------
    // DebugRenderer はエンジン全体から呼び出せるデバッグ描画専用マネージャーです。
    // ここでは DirectX12 のデバイスやコマンドリストを使えるようにし、
    // ライン描画専用の RootSignature / Pipeline / 定数バッファを準備します。
    //
    // Object3D の既存描画基盤と同じ DirectXCommon を流用しつつ、
    // デバッグライン用に「線を太くするための専用シェーダ」を持たせています。
    dxCommon_ = DirectXCommon::GetInstance();
    assert(dxCommon_);

    CreateRootSignature();
    CreateGraphicsPipeline();

    // CPU側で計算した ViewProjection 行列を GPU へ渡すための定数バッファです。
    // ラインの始点・終点はワールド座標で登録されるため、頂点シェーダで画面に映すには
    // 現在のカメラの ViewProjection 行列が必要になります。
    viewProjectionResource_ = dxCommon_->CreateBufferResource(sizeof(ViewProjectionData));
    viewProjectionResource_->SetName(L"DebugRenderer::ViewProjectionCB");
    viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));
    viewProjectionData_->viewProjection = MatrixMath::MakeIdentity4x4();

    // thicknessの用途
    // ------------------------------------------------------------
    // thickness は「画面上で何ピクセルくらいの太さに見せたいか」を表す値として扱います。
    // ジオメトリシェーダは画面サイズを使って、線分をそのピクセル幅の四角形に広げます。
    screenResource_ = dxCommon_->CreateBufferResource(sizeof(ScreenData));
    screenResource_->SetName(L"DebugRenderer::ScreenCB");
    screenResource_->Map(0, nullptr, reinterpret_cast<void**>(&screenData_));
    screenData_->screenSize = {
        static_cast<float>(WinApp::kClientWidth),
        static_cast<float>(WinApp::kClientHeight)
    };
    screenData_->padding = { 0.0f, 0.0f };

    isInitialized_ = true;
}

void DebugRenderer::Update()
{
    // Update は今後の拡張ポイントです。
    // ------------------------------------------------------------
    // 今回の AddLine() は「登録された線をそのフレームだけ描く」だけなので、
    // 毎フレーム更新する内部状態はまだありません。
    //
    // 将来的に AddSphere(), AddBox(), AddCapsule(), AddArrow() などを追加した場合、
    // 表示時間を持つデバッグ形状、フェードアウト、カテゴリごとのON/OFFなどを
    // ここで更新できるように残しています。
}

void DebugRenderer::AddLine(
    const Vector3& start,
    const Vector3& end,
    const Vector4& color,
    float thickness)
{
    // AddLineで何をしているか
    // ------------------------------------------------------------
    // AddLine() は、GPUへすぐ描画命令を出す関数ではありません。
    // まず CPU側の lines_ に「このフレームで描きたい線の情報」を登録します。
    //
    // start/end はワールド座標の始点と終点です。
    // color はRGBAです。alpha を 1.0f 未満にすれば半透明ラインにもできます。
    // thickness は画面上のピクセル幅として扱い、Draw() でGPUへ送る頂点データに入ります。
    //
    // 0以下の太さは見えなくなるため、デバッグ用途では最低 1.0f に丸めます。
    DebugLine line {};
    line.start = start;
    line.end = end;
    line.color = color;
    line.thickness = thickness;

    if (line.thickness <= 0.0f) {
        line.thickness = 1.0f;
    }

    lines_.push_back(line);
}

void DebugRenderer::Draw()
{
    // Drawで何をしているか
    // ------------------------------------------------------------
    // Draw() は AddLine() で登録された DebugLine をまとめてGPUへ送り、
    // ライン描画専用パイプラインで一括描画します。
    //
    // 登録時点では「線分の始点・終点」だけをCPU側に持っています。
    // Draw() ではそれを「GPUへ送る頂点データ」に変換し、頂点シェーダでカメラ変換、
    // ジオメトリシェーダで thickness 分の幅を持つ四角形へ展開します。
    if (!isInitialized_) {
        return;
    }

    if (lines_.empty()) {
        return;
    }

    Camera* camera = Object3dManager::GetInstance()->GetDefaultCamera();
    if (camera == nullptr) {
        // カメラが無い状態ではワールド座標の線を画面に投影できません。
        // 描けなかった線を次フレームへ持ち越すと、古いデバッグ線が急に出る原因になるため破棄します。
        lines_.clear();
        return;
    }

    viewProjectionData_->viewProjection = camera->GetViewProjectionMatrix();
    screenData_->screenSize = {
        static_cast<float>(WinApp::kClientWidth),
        static_cast<float>(WinApp::kClientHeight)
    };

    const std::size_t vertexCount = lines_.size() * 2;
    EnsureVertexCapacity(vertexCount);
    UploadLineVertices();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());

    // GPUへ送る頂点データ
    // ------------------------------------------------------------
    // DebugLine 1本につき、始点と終点の2頂点をGPUへ送ります。
    // この時点ではまだ細い線分の情報だけです。
    // 太さは各頂点の thickness として送り、ジオメトリシェーダ側で画面上の四角形へ広げます。
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, viewProjectionResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, screenResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);

    // なぜDraw後にlines_をクリアするのか
    // ------------------------------------------------------------
    // DebugRenderer はデバッグ用途なので、登録された線を永続保持しません。
    // そのフレームで必要な線は、そのフレームの Update などから毎回 AddLine() してもらいます。
    //
    // こうすると、レイキャストや当たり判定のように毎フレーム変わる情報が古く残りません。
    // また、不要になったデバッグ線を消すための個別管理も不要になります。
    lines_.clear();
}

void DebugRenderer::CreateRootSignature()
{
    HRESULT hr = S_FALSE;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // b0: ViewProjection
    // ワールド座標のライン頂点をカメラで画面へ投影するため、頂点シェーダへ渡します。
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // b1: ScreenData
    // thickness をピクセル幅として扱うため、ジオメトリシェーダで画面サイズを使います。
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = nullptr;
    desc.NumStaticSamplers = 0;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }

        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void DebugRenderer::CreateGraphicsPipeline()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "COLOR";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "THICKNESS";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/Debug/DebugLine.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> geometryShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/Debug/DebugLine.GS.hlsl", L"gs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->CompileShader(L"resources/Shaders/Debug/DebugLine.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob && geometryShaderBlob && pixelShaderBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    desc.GS = { geometryShaderBlob->GetBufferPointer(), geometryShaderBlob->GetBufferSize() };
    desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.BlendState = CreateBlendDesc(kBlendModeNormal);
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void DebugRenderer::EnsureVertexCapacity(std::size_t vertexCount)
{
    if (vertexCount <= vertexCapacity_) {
        return;
    }

    if (vertexResource_) {
        vertexResource_->Unmap(0, nullptr);
    }

    vertexCapacity_ = static_cast<uint32_t>(vertexCount);

    const std::size_t bufferSize = sizeof(DebugLineVertex) * vertexCapacity_;
    vertexResource_ = dxCommon_->CreateBufferResource(bufferSize);
    vertexResource_->SetName(L"DebugRenderer::LineVertexBuffer");
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexBufferView_.StrideInBytes = sizeof(DebugLineVertex);
}

void DebugRenderer::UploadLineVertices()
{
    // CPU側で生成するデータ
    // ------------------------------------------------------------
    // lines_ は「線分」としての情報です。
    // ここでは、それぞれの線分を GPU が読める頂点バッファ形式へ詰め替えます。
    //
    // 将来的にSphereやBoxを追加する拡張ポイント
    // ------------------------------------------------------------
    // Sphere や Box も、最終的にはラインの集合として表現できます。
    // AddSphere() なら円を複数本の線に分解する、AddBox() なら12本の辺に分解する、
    // という形で lines_ に追加すれば、この UploadLineVertices() と Draw() を流用できます。
    std::size_t vertexIndex = 0;

    for (std::size_t lineIndex = 0; lineIndex < lines_.size(); ++lineIndex) {
        const DebugLine& line = lines_[lineIndex];

        vertexData_[vertexIndex].position = line.start;
        vertexData_[vertexIndex].color = line.color;
        vertexData_[vertexIndex].thickness = line.thickness;
        ++vertexIndex;

        vertexData_[vertexIndex].position = line.end;
        vertexData_[vertexIndex].color = line.color;
        vertexData_[vertexIndex].thickness = line.thickness;
        ++vertexIndex;
    }
}
