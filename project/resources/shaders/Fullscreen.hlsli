struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};
cbuffer PostEffectParameter : register(b0)
{
    float grayScaleStrength;
    float vignetteStrength;
    float outlineScale;
    float time;
};