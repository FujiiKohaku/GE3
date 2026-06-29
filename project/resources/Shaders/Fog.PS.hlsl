struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D<float4> gSceneColor : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

cbuffer FogData : register(b0)
{
    int isEnabled;
    float startDistance;
    float endDistance;
    float curve;
    float3 color;
    float maxFog;
    float nearClip;
    float farClip;
    float fovY;
    float aspectRatio;
};

float RestoreViewZ(float depth)
{
    float validNearClip = max(nearClip, 0.0001f);
    float validFarClip = max(farClip, validNearClip + 0.0001f);
    float denominator = validFarClip - depth * (validFarClip - validNearClip);

    denominator = max(denominator, 0.0001f);

    return (validNearClip * validFarClip) / denominator;
}

float3 RestoreViewPosition(float depth, float2 texcoord)
{
    float viewZ = RestoreViewZ(depth);
    float validFovY = max(fovY, 0.0001f);
    float validAspectRatio = max(aspectRatio, 0.0001f);
    float tanHalfFovY = tan(validFovY * 0.5f);

    float2 ndc;
    ndc.x = texcoord.x * 2.0f - 1.0f;
    ndc.y = 1.0f - texcoord.y * 2.0f;

    float3 viewPosition;
    viewPosition.x = ndc.x * viewZ * tanHalfFovY * validAspectRatio;
    viewPosition.y = ndc.y * viewZ * tanHalfFovY;
    viewPosition.z = viewZ;

    return viewPosition;
}

float CalculateFogFactor(float viewDistance)
{
    float fogRange = max(endDistance - startDistance, 0.0001f);
    float distanceFactor = saturate((viewDistance - startDistance) / fogRange);
    float validCurve = max(curve, 0.01f);
    float curvedFactor = pow(distanceFactor, validCurve);
    float fogFactor = saturate(curvedFactor * saturate(maxFog));

    return fogFactor;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float depth = gDepthTexture.Sample(gSampler, input.texcoord);

    float3 viewPosition = RestoreViewPosition(depth, input.texcoord);
    float viewDistance = length(viewPosition);
    float fogFactor = CalculateFogFactor(viewDistance);

    if (isEnabled == 0)
    {
        fogFactor = 0.0f;
    }

    return float4(color, fogFactor);
}
