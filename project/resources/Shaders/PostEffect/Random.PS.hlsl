#include "Fullscreen.hlsli"

float rand2dTo1d(float2 value)
{
    float random =
        frac(
            sin(
                dot(
                    value,
                    float2(12.9898f, 78.233f))) *
            43758.5453f);

    return random;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float random = rand2dTo1d(input.texcoord);

    return float4(
        random,
        random,
        random,
        1.0f);
}
