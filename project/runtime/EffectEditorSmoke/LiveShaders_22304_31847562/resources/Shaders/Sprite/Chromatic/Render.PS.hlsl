#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 baseUv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float offsetAmount = gEffect.amplitude * sin(gFrame.elapsedTime * gEffect.speed);
    float2 offset = gEffect.direction * offsetAmount;
    float red = gTexture.Sample(gSampler, baseUv + offset).r;
    float green = gTexture.Sample(gSampler, baseUv).g;
    float blue = gTexture.Sample(gSampler, baseUv - offset).b;
    float alpha = gTexture.Sample(gSampler, baseUv).a * gMaterial.color.a;
    output.color = float4(float3(red, green, blue) * gMaterial.color.rgb, alpha);
    return output;
}
