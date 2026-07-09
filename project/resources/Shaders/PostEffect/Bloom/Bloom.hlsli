struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

cbuffer BloomParameter : register(b0)
{
    int bloomEnabled;
    float threshold;
    int blurRadius;
    float blurSigma;

    float intensity;
    int blurDirection;
    float padding0;
    float padding1;
};
