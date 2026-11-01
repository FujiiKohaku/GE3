#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float zoom = 1.0f + sin(gFrame.elapsedTime * gEffect.speed) * gEffect.amplitude;
    float2 uv = (input.texcoord - 0.5f) / zoom + 0.5f;
    uv = mul(float4(uv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    output.color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    return output;
}
