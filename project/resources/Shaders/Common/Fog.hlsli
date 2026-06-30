struct DistanceFogData
{
    float start;
    float end;
    float density;
    float curve;
};

struct HeightFogData
{
    float startHeight;
    float endHeight;
    float density;
    float padding;
};

struct ExponentialFogData
{
    float density;
    float falloff;
    float2 padding;
};

struct NoiseFogData
{
    float scale;
    float speed;
    float strength;
    float padding;
};

struct VolumetricFogData
{
    float scattering;
    float stepLength;
    float maxDistance;
    float padding;
};

struct FogData
{
    float4 color;

    DistanceFogData distance;
    HeightFogData height;
    ExponentialFogData exponential;
    NoiseFogData noise;
    VolumetricFogData volumetric;

    float nearClip;
    float farClip;
    float fovY;
    float aspectRatio;

    int isEnabled;
    int distanceEnabled;
    int heightEnabled;
    int exponentialEnabled;

    int noiseEnabled;
    int volumetricEnabled;
    float time;
    float padding;
};

float RestoreFogViewZ(float depth, FogData fog)
{
    float validNearClip = max(fog.nearClip, 0.0001f);
    float validFarClip = max(fog.farClip, validNearClip + 0.0001f);
    float denominator = validFarClip - depth * (validFarClip - validNearClip);
    denominator = max(denominator, 0.0001f);

    return (validNearClip * validFarClip) / denominator;
}

float3 RestoreFogViewPosition(float depth, float2 texcoord, FogData fog)
{
    float viewZ = RestoreFogViewZ(depth, fog);
    float validFovY = max(fog.fovY, 0.0001f);
    float validAspectRatio = max(fog.aspectRatio, 0.0001f);
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

float CalcDistanceFog(float viewDistance, DistanceFogData distanceFog)
{
    float fogRange = max(distanceFog.end - distanceFog.start, 0.0001f);
    float distanceFactor = saturate((viewDistance - distanceFog.start) / fogRange);
    float validCurve = max(distanceFog.curve, 0.01f);
    float curvedFactor = pow(distanceFactor, validCurve);

    return saturate(curvedFactor * saturate(distanceFog.density));
}

float CalcHeightFog(float worldHeight, HeightFogData heightFog)
{
    float heightRange = max(heightFog.endHeight - heightFog.startHeight, 0.0001f);
    float heightFactor = saturate((heightFog.endHeight - worldHeight) / heightRange);

    return saturate(heightFactor * saturate(heightFog.density));
}

float CalcExponentialFog(float viewDistance, ExponentialFogData exponentialFog)
{
    float density = max(exponentialFog.density, 0.0f);
    float falloff = max(exponentialFog.falloff, 0.0001f);
    float fogFactor = 1.0f - exp(-viewDistance * density * falloff);

    return saturate(fogFactor);
}

float CalcNoise(float3 position, NoiseFogData noiseFog)
{
    float scale = max(noiseFog.scale, 0.0001f);
    float noiseValue = frac(sin(dot(position * scale, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);

    return saturate(noiseValue * saturate(noiseFog.strength));
}

float CalcFogFactor(FogData fog, float3 worldPosition, float viewDistance)
{
    float fogFactor = 0.0f;

    if (fog.isEnabled == 0)
    {
        return fogFactor;
    }

    if (fog.distanceEnabled != 0)
    {
        fogFactor = max(fogFactor, CalcDistanceFog(viewDistance, fog.distance));
    }

    if (fog.heightEnabled != 0)
    {
        fogFactor = max(fogFactor, CalcHeightFog(worldPosition.y, fog.height));
    }

    if (fog.exponentialEnabled != 0)
    {
        fogFactor = max(fogFactor, CalcExponentialFog(viewDistance, fog.exponential));
    }

    if (fog.noiseEnabled != 0)
    {
        float3 noisePosition = worldPosition;
        noisePosition.x += fog.time * fog.noise.speed;
        float noiseFactor = CalcNoise(noisePosition, fog.noise);
        fogFactor = saturate(fogFactor + noiseFactor * (1.0f - fogFactor));
    }

    return saturate(fogFactor);
}

float4 ApplyFog(float4 sceneColor, float3 fogColor, float fogFactor)
{
    float factor = saturate(fogFactor);
    float3 color = lerp(sceneColor.rgb, fogColor, factor);

    return float4(color, sceneColor.a);
}

float4 ApplyFog(float4 sceneColor, FogData fog, float3 worldPosition, float viewDistance)
{
    float fogFactor = CalcFogFactor(fog, worldPosition, viewDistance);

    return ApplyFog(sceneColor, fog.color.rgb, fogFactor);
}
