#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;

    float2 uv = mul(
        float4(input.texcoord, 0.0f, 1.0f),
        gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;

    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;
    float pulse = 0.82f + sin(time * 1.7f) * 0.18f;

    float highlightCenter = frac(time * 0.16f) * 1.4f - 0.2f;
    float highlightDistance = abs(input.texcoord.x - highlightCenter);
    float highlight = 1.0f - smoothstep(0.0f, 0.13f, highlightDistance);

    float verticalEdge = abs(input.texcoord.y * 2.0f - 1.0f);
    float edgeGlow = smoothstep(0.45f, 1.0f, verticalEdge);

    color.rgb *= pulse;
    color.rgb += gMaterial.color.rgb * highlight * gEffect.strength * 0.75f;
    color.rgb += gMaterial.color.rgb * edgeGlow * gEffect.strength * 0.28f;
    color.rgb = saturate(color.rgb);

    output.color = color;
    return output;
}
