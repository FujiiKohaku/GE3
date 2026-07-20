#include "SpriteCommon.hlsli"

ConstantBuffer<SpriteTransform> gTransformationMatrix : register(b0);
ConstantBuffer<SpriteEffectParameters> gEffect : register(b2);
ConstantBuffer<SpriteFrameParameters> gFrame : register(b3);
