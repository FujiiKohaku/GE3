#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    float amount = saturate(0.5f + 0.5f * sin(gFrame.elapsedTime * gEffect.speed));
    output.color = float4(lerp(color.rgb, 1.0f - color.rgb, amount), color.a);
    return output;
}
