#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    float time = gFrame.elapsedTime * gEffect.speed;
    float3 cycle = 0.65f + 0.35f * sin(time + float3(0.0f, 2.094395f, 4.188790f));
    color.rgb *= cycle * 1.35f;
    output.color = color;
    return output;
}
