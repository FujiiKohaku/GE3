#include "SpriteCommon.hlsli"

ConstantBuffer<SpriteMaterial> gMaterial : register(b1);
ConstantBuffer<SpriteEffectParameters> gEffect : register(b2);
ConstantBuffer<SpriteFrameParameters> gFrame : register(b3);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
