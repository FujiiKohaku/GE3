struct SpriteVertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

struct SpriteVertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct SpriteTransform
{
    float4x4 WVP;
};

struct SpriteMaterial
{
    float4 color;
    float4x4 uvTransform;
};

struct SpriteEffectParameters
{
    float amplitude;
    float frequency;
    float speed;
    float phase;
    float2 direction;
    float strength;
    float threshold;
    float2 spriteSize;
    float2 padding;
};

struct SpriteFrameParameters
{
    float elapsedTime;
    float deltaTime;
    float2 screenSize;
};
