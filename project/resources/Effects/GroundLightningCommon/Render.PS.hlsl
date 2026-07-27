#include "../Common/ParticleCommon.hlsli"

float Ring(float radius, float target, float width)
{
    return 1.0f - smoothstep(width, width * 2.2f, abs(radius - target));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 p = input.texcoord * 2.0f - 1.0f;
    p.y = -p.y;
    float radius = length(p);
    float angle = atan2(p.y, p.x);
    float outer = Ring(radius, 0.88f, 0.012f);
    float outerEcho = Ring(radius, 0.82f, 0.006f);
    float middle = Ring(radius, 0.62f, 0.010f);
    float inner = Ring(radius, 0.32f, 0.008f);
    float spokes = pow(saturate(cos(angle * 12.0f)), 42.0f);
    spokes *= smoothstep(0.34f, 0.40f, radius) *
              (1.0f - smoothstep(0.76f, 0.82f, radius));
    float petals = abs(cos(angle * 4.0f));
    float petalLines = Ring(radius, 0.44f + 0.12f * petals, 0.008f);
    float waterRipple =
        Ring(radius, 0.73f + sin(angle * 8.0f) * 0.018f, 0.008f);
    float electricBreaks =
        step(0.28f, frac(sin(angle * 17.0f + radius * 31.0f) * 7.13f));
    waterRipple *= electricBreaks;
    float centerGlow = pow(saturate(1.0f - radius), 4.0f) * 0.32f;
    float mask = outer + outerEcho * 0.55f + middle * 0.78f +
                 inner * 0.70f + spokes * 0.62f +
                 petalLines * 0.72f + waterRipple * 0.95f + centerGlow;
    mask *= 1.0f - smoothstep(0.94f, 1.0f, radius);
    float core = saturate(mask * 1.45f);
    float3 blue = lerp(
        float3(0.02f, 0.18f, 1.0f),
        float3(0.72f, 0.95f, 1.0f),
        core);
    float alpha = saturate(mask) * input.color.a;
    if (alpha < 0.006f) discard;
    PixelShaderOutput output;
    output.color =
        float4(blue * input.color.rgb * (1.4f + core * 1.8f), alpha);
    return output;
}
