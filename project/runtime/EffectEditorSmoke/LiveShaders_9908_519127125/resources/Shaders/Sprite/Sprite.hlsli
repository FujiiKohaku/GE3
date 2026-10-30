// ===============================
// Sprite 用 VS → PS 出力
// ===============================
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// ===============================
// Sprite 用 Material
// ===============================
struct Material2D
{
    float4 color;
    float4x4 uvTransform;
};

// ===============================
// Sprite 用 Transform（VS用）
// ===============================
struct TransformationMatrix
{
    float4x4 WVP;
};
