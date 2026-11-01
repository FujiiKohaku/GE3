#ifndef KOHAKU_VOLUME_DATA
#define KOHAKU_VOLUME_DATA
#ifndef VOLUME_CONSTANT_REGISTER
#define VOLUME_CONSTANT_REGISTER b0
#endif
struct FogVolumeConstants {
    float4 centerAndShape;
    float4 extentsAndDensity;
    float4 radiusAndSoftness;
};
struct VolumetricSpotConstants {
    row_major float4x4 lightViewProjection;
    float4 positionAndDistance;
    float4 directionAndCosAngle;
    float4 colorAndIntensity;
    float4 decayAndFalloffAndShadow;
};
cbuffer VolumeConstants : register(VOLUME_CONSTANT_REGISTER) {
    row_major float4x4 inverseViewProjection;
    row_major float4x4 lightViewProjection;
    float4 cameraAndDistance;
    float4 lightDirectionAndDensity;
    float4 lightColorAndIntensity;
    float4 settings;
    float4 fogColorAndEnabled;
    float4 heightAndVolumeCount;
    float4 noiseScaleAndOffset;
    float4 noiseSettings;
    FogVolumeConstants volumes[8];
    float4 spotCountAndBias;
    VolumetricSpotConstants spotLights[2];
    row_major float4x4 previousRelativeViewProjection;
    float4 previousCameraDelta;
    float4 temporalControls;
    float4 underwaterSettings;
};

#endif
